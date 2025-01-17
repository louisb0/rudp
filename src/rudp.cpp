#include <sys/socket.h>
#include <sys/types.h>

#include <cstring>
#include <memory>

#include "internal/common.hpp"
#include "internal/connection.hpp"
#include "internal/listener.hpp"
#include "internal/socket.hpp"

namespace rudp {

int socket(void) noexcept {
    rudpfd_t fd = internal::next_fd++;
    internal::g_sockets.emplace(fd, internal::socket{});
    return fd;
}

int bind(int sockfd, struct sockaddr *addr, socklen_t addrlen) noexcept {
    if (addr->sa_family != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    // Socket lookup and state validation.
    auto it = internal::g_sockets.find(sockfd);
    if (it == internal::g_sockets.end()) {
        errno = EBADF;
        return -1;
    }

    internal::socket *sock = &it->second;
    if (sock->state != internal::socket::state::created) {
        errno = EOPNOTSUPP;
        return -1;
    }

    // Bind the underlying FD.
    linuxfd_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    if (::bind(fd, addr, addrlen) < 0) {
        // NOTE: If close() failed, we would have a leak of a bound socket here.
        internal::preserve_errno([fd]() { ::close(fd); });
        return -1;
    }

    // Update the socket state.
    sock->state = internal::socket::state::bound;
    sock->data.bound_fd = fd;

    return 0;
}

int listen(int sockfd, int backlog) noexcept {
    if (backlog < 0 || backlog > SOMAXCONN) {
        errno = EINVAL;
        return -1;
    }

    // Socket lookup and state validation.
    auto it = internal::g_sockets.find(sockfd);
    if (it == internal::g_sockets.end()) {
        errno = EBADF;
        return -1;
    }

    internal::socket *sock = &it->second;
    if (sock->state != internal::socket::state::bound) {
        errno = EOPNOTSUPP;
        return -1;
    }

    linuxfd_t fd = sock->data.bound_fd;
    RUDP_ASSERT(internal::is_valid_sockfd(fd),
                "A socket in the BOUND state must hold a valid linuxfd_t.");

    // Create and initialise the listener.
    auto listener = std::make_unique<internal::listener>(sockfd, fd, backlog);
    if (!listener) {
        errno = ENOMEM;
        return -1;
    }

    if (!listener->init()) {
        return -1;
    }
    RUDP_ASSERT(listener->assert_initialised_state(__PRETTY_FUNCTION__));

    // Update the socket state.
    sock->state = internal::socket::state::listening;
    sock->data.lstnr = std::move(listener);

    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) noexcept {
    if (addr != nullptr) {
        if (addrlen == nullptr || *addrlen != sizeof(struct sockaddr_in)) {
            errno = EINVAL;
            return -1;
        }
    }

    // Socket lookup and state validation.
    auto it = internal::g_sockets.find(sockfd);
    if (it == internal::g_sockets.end()) {
        errno = EBADF;
        return -1;
    }

    internal::socket *sock = &it->second;
    if (sock->state != internal::socket::state::listening) {
        errno = EOPNOTSUPP;
        return -1;
    }

    // Validate state of the listener & forward blocking accept call.
    auto &listener = sock->data.lstnr;
    RUDP_ASSERT(listener != nullptr,
                "A socket in the LISTENING state must hold a non-null listener.");
    RUDP_ASSERT(listener->assert_initialised_state(__PRETTY_FUNCTION__));

    rudpfd_t fd = listener->wait_and_accept();
    if (fd < 0) {
        return -1;
    }

    // Validate state of the accepted socket.
    RUDP_ASSERT(internal::g_sockets.contains(fd),
                "wait_and_accept() must return a valid rudpfd_t.");
    RUDP_ASSERT(internal::g_sockets.at(fd).state == internal::socket::state::connected,
                "Accepted socket must be in CONNECTED state.");
    auto &accepted_sock = internal::g_sockets.at(fd);

    // Fill out the callers addr and addrlen.
    if (addr != nullptr && addrlen != nullptr) {
        struct sockaddr_in peer_addr = {};
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_addr.s_addr = accepted_sock.data.conn.dst_ip;
        peer_addr.sin_port = accepted_sock.data.conn.dst_port;

        memcpy(addr, &peer_addr, std::min(static_cast<unsigned long>(*addrlen), sizeof(peer_addr)));
        *addrlen = sizeof(peer_addr);
    }

    return fd;
}

int connect(int sockfd, struct sockaddr *addr, socklen_t addrlen) noexcept {
    if (addrlen != sizeof(struct sockaddr_in)) {
        errno = EINVAL;
        return -1;
    }

    if (addr->sa_family != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    // Socket lookup and state validation.
    auto it = internal::g_sockets.find(sockfd);
    if (it == internal::g_sockets.end()) {
        errno = EBADF;
        return -1;
    }

    internal::socket *sock = &it->second;
    if (sock->state != internal::socket::state::created &&
        sock->state != internal::socket::state::bound) {
        errno = EOPNOTSUPP;
        return -1;
    }

    // Ensure socket is bound, either existing or implicitly.
    if (sock->state != internal::socket::state::created) {
        struct sockaddr_in bind_addr = {};
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        bind_addr.sin_port = 0;
        if (rudp::bind(sockfd, reinterpret_cast<struct sockaddr *>(&bind_addr), sizeof(bind_addr)) <
            0) {
            return -1;
        }
    }
    RUDP_ASSERT(sock->state == internal::socket::state::bound,
                "A socket must have an allocated port prior to becoming connected.");

    // Get local address information for internal::connection_tuple.
    linuxfd_t fd = sock->data.bound_fd;
    RUDP_ASSERT(internal::is_valid_sockfd(fd),
                "A socket in the BOUND state must hold a valid linuxfd_t.");

    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    if (getsockname(fd, reinterpret_cast<struct sockaddr *>(&local_addr), &local_len) < 0) {
        return -1;
    }

    // Create and initialise the connection.
    internal::connection_tuple tuple = {
        .src_ip = local_addr.sin_addr.s_addr,
        .src_port = local_addr.sin_port,
        .dst_port = (reinterpret_cast<struct sockaddr_in *>(addr))->sin_port,
        .dst_ip = (reinterpret_cast<struct sockaddr_in *>(addr))->sin_addr.s_addr,
    };

    internal::connection conn(tuple);
    if (!conn.init()) {
        // TODO: Consider what would be forwarded from init() - is it user-friendly, or must it be
        // proxied?
        // NOTE: If close() failed, we would have a leak of a bound socket here.
        internal::preserve_errno([fd]() { ::close(fd); });
        sock->state = internal::socket::state::created;

        return -1;
    }
    RUDP_ASSERT(conn.assert_initialised_state(__PRETTY_FUNCTION__));

    // Register the connection and update socket state.
    auto [_, inserted] = internal::g_connections.emplace(tuple, std::move(conn));
    if (!inserted) {
        errno = EADDRINUSE;
        return -1;
    }

    sock->state = internal::socket::state::connected;
    new (&sock->data.conn) internal::connection_tuple(tuple);

    return 0;
}

size_t send(int sockfd, const void *buf, size_t len, int flags) noexcept;
size_t recv(int sockfd, void *buf, size_t len, int flags) noexcept;
int close(int sockfd) noexcept;

}  // namespace rudp
