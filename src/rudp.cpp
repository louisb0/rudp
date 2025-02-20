#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>

#include "internal/common.hpp"
#include "internal/connection.hpp"
#include "internal/event_loop.hpp"
#include "internal/listener.hpp"
#include "internal/socket.hpp"

namespace rudp {

int socket(void) noexcept {
    rudpfd_t fd = internal::next_fd++;
    // TODO: Use emplace properly.
    internal::g_sockets.emplace(fd, internal::socket{});
    return fd;
}

int bind(int sockfd, struct sockaddr *addr, socklen_t addrlen) noexcept {
    // Address validation.
    if (addr == nullptr) {
        errno = EINVAL;
        return -1;
    }

    if (addrlen != sizeof(struct sockaddr_in)) {
        errno = EINVAL;
        return -1;
    }

    if (addr->sa_family != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    // Socket validation.
    auto sock_it = internal::g_sockets.find(sockfd);
    if (sock_it == internal::g_sockets.end()) {
        errno = EBADF;
        return -1;
    }

    internal::socket *sock = &sock_it->second;
    if (sock->state != internal::socket::state::created) {
        errno = EOPNOTSUPP;
        return -1;
    }

    // Create and bind an underlying FD.
    linuxfd_t fd = internal::create_raw_socket();
    if (fd < 0) {
        return -1;
    }

    if (::bind(fd, addr, addrlen) < 0) {
        internal::preserve_errno([fd]() { ::close(fd); });
        return -1;
    }

    // Update the socket state.
    sock->state = internal::socket::state::bound;
    sock->data.bound_fd = fd;

    return 0;
}

int listen(int sockfd, int backlog) noexcept {
    // TODO: Accept -1 as an indicator of no backlog.
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
    auto listener = std::make_unique<internal::listener>(fd, backlog);
    if (!listener) {
        errno = ENOMEM;
        return -1;
    }

    if (!internal::event_loop::add_handler(listener.get())) {
        // NOTE: add_handler() sets errno.
        return -1;
    }
    listener->assert_initialised_handler(__PRETTY_FUNCTION__);

    // Update the socket state.
    sock->state = internal::socket::state::listening;
    new (&sock->data.lstnr) std::unique_ptr<internal::listener>(std::move(listener));

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
    listener->assert_initialised_handler(__PRETTY_FUNCTION__);

    rudpfd_t fd = listener->wait_and_accept();

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
    // TODO: Check addr is not nullptr.

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
    if (sock->state == internal::socket::state::created) {
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

    if (internal::g_connections.contains(tuple)) {
        errno = EADDRINUSE;
        return -1;
    }

    auto conn = std::make_unique<internal::connection>(fd, tuple);
    if (!conn) {
        errno = ENOMEM;
        return -1;
    }

    if (!internal::event_loop::add_handler(conn.get())) {
        // NOTE: add_handler() sets errno.
        return -1;
    }
    conn->assert_initialised_handler(__PRETTY_FUNCTION__);

    // Send the SYN and await connection establishment.
    if (!conn->syn()) {
        // NOTE: syn() sets errno.
        return -1;
    }

    conn->wait_for_established();

    // Insert the connection and transition socket state.
    // TODO: Use emplace properly.
    auto [conn_it, inserted] = internal::g_connections.emplace(tuple, std::move(conn));
    RUDP_ASSERT(inserted, "connect() will not insert an existing connection.");

    sock->state = internal::socket::state::connected;
    new (&sock->data.conn) internal::connection_tuple(tuple);

    return 0;
}

size_t send(int sockfd, const void *buf, size_t len, int flags) noexcept;
size_t recv(int sockfd, void *buf, size_t len, int flags) noexcept;
int close(int sockfd) noexcept;

}  // namespace rudp
