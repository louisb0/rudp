#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

// TODO: Check imports project-wide.
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
    rudpfd_t fd = internal::g_next_fd++;
    internal::g_sockets.try_emplace(fd);
    return fd;
}

int bind(int sockfd, struct sockaddr *addr, socklen_t addrlen) noexcept {
    // Argument validation.
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

    internal::socket &sock = sock_it->second;
    if (!sock.created()) {
        errno = EOPNOTSUPP;
        return -1;
    }

    // Create and bind an underlying FD.
    linuxfd_t fd = internal::create_raw_socket();
    if (fd < 0) {
        // NOTE: errno is forwarded from socket() or fcntl().
        return -1;
    }

    if (::bind(fd, addr, addrlen) < 0) {
        int saved = errno;
        ::close(fd);
        errno = saved;
        return -1;
    }

    // Transition state.
    sock.data = fd;
    return 0;
}

int listen(int sockfd, int backlog) noexcept {
    // Argument validation.
    if (backlog == -1 || backlog > SOMAXCONN) {
        backlog = SOMAXCONN;
    }

    if (backlog <= 0) {
        errno = EINVAL;
        return -1;
    }

    // Socket validation.
    auto sock_it = internal::g_sockets.find(sockfd);
    if (sock_it == internal::g_sockets.end()) {
        errno = EBADF;
        return -1;
    }

    internal::socket &sock = sock_it->second;
    if (!sock.bound()) {
        errno = EOPNOTSUPP;
        return -1;
    }

    linuxfd_t fd = sock.fd();
    RUDP_ASSERT(internal::is_valid_sockfd(fd),
                "A bound socket must have a valid underlying file descriptor.");

    // Create and initialise the listener.
    auto listener = std::make_unique<internal::listener>(fd, backlog);
    if (!listener) {
        errno = ENOMEM;
        return -1;
    }

    auto [err, event_loop] = internal::event_loop::instance();
    if (err != internal::event_loop::result::error::none) {
        // NOTE: errno is already set in the epoll_creation case.
        if (err == internal::event_loop::result::error::thread_creation) {
            errno = ENOMEM;
        }

        return -1;
    }
    event_loop->assert_initialised_state(__PRETTY_FUNCTION__);

    if (!event_loop->add_handler(internal::handler_type::listener, fd,
                                 [listener = listener.get()]() { listener->handle_events(); })) {
        // NOTE: errno is forwarded from epoll_ctl().
        return -1;
    }

    // Transition state.
    sock.data = std::move(listener);
    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) noexcept {
    bool fillout_peer_addr = (addr != nullptr);

    // Argument validation.
    if (fillout_peer_addr) {
        if (addrlen == nullptr) {
            errno = EINVAL;
            return -1;
        }

        if (*addrlen != sizeof(struct sockaddr_in)) {
            errno = EINVAL;
            return -1;
        }
    }

    // Socket validation.
    auto sock_it = internal::g_sockets.find(sockfd);
    if (sock_it == internal::g_sockets.end()) {
        errno = EBADF;
        return -1;
    }

    internal::socket &sock = sock_it->second;
    if (!sock.listening()) {
        errno = EOPNOTSUPP;
        return -1;
    }

    // Block until we have a connection to return.
    internal::listener *listener = sock.listener();
    RUDP_ASSERT(listener != nullptr, "A listening socket's unique_ptr must be non-null.");

    rudpfd_t fd = listener->wait_and_accept();
    RUDP_ASSERT(internal::g_sockets.contains(fd),
                "wait_and_accept() must return a valid rudpfd_t.");
    RUDP_ASSERT(internal::g_sockets.at(fd).connected(),
                "A socket freshly spawned by listen() must be connected.");

    auto &spawned = internal::g_sockets.at(fd);

    // Conditionally fill out the peer address information.
    if (fillout_peer_addr) {
        RUDP_ASSERT(addrlen != nullptr,
                    "addrlen must be validated as non-null when filling out the peer address.");

        struct sockaddr_in peer_addr = spawned.connection()->peer();
        memcpy(addr, &peer_addr, std::min(static_cast<unsigned long>(*addrlen), sizeof(peer_addr)));
        *addrlen = sizeof(peer_addr);
    }

    return fd;
}

int connect(int sockfd, struct sockaddr *addr, socklen_t addrlen) noexcept {
    // Argument validation.
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

    internal::socket &sock = sock_it->second;
    if (!sock.created() && !sock.bound()) {
        errno = EOPNOTSUPP;
        return -1;
    }

    // Ensure the socket is bound so that we can identify the connection.
    if (sock.created()) {
        struct sockaddr_in bind_addr{};
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        bind_addr.sin_port = 0;

        if (rudp::bind(sockfd, reinterpret_cast<struct sockaddr *>(&bind_addr), sizeof(bind_addr)) <
            0) {
            RUDP_ASSERT(
                errno == ENOMEM || errno == EADDRINUSE,
                "bind() can only fail with resource exhaustion errors after argument validation.");
            return -1;
        }
    }

    linuxfd_t fd = sock.fd();
    RUDP_ASSERT(internal::is_valid_sockfd(fd),
                "A bound socket must have a valid underlying file descriptor.");

    // Create and register the connection.
    auto connection = std::make_unique<internal::connection>(fd);
    if (!connection) {
        errno = ENOMEM;
        return -1;
    }

    auto [err, event_loop] = internal::event_loop::instance();
    if (err != internal::event_loop::result::error::none) {
        // NOTE: errno is already set in the epoll_creation case.
        if (err == internal::event_loop::result::error::thread_creation) {
            errno = ENOMEM;
        }

        return -1;
    }
    event_loop->assert_initialised_state(__PRETTY_FUNCTION__);

    if (!event_loop->add_handler(
            internal::handler_type::connection, fd,
            [connection = connection.get()]() { connection->handle_events(); })) {
        // NOTE: errno is forwarded from epoll_ctl().
        return -1;
    }

    // Block until a connection is established.
    if (!connection->active_open(*reinterpret_cast<sockaddr_in *>(addr))) {
        event_loop->remove_handler(internal::handler_type::connection, fd);
        // NOTE: errno is forwarded from sendto().
        return -1;
    }
    connection->wait_for_established();

    // Transition state.
    sock.data = std::move(connection);
    return 0;
}

size_t send(int sockfd, const void *buf, size_t len, int flags) noexcept;
size_t recv(int sockfd, void *buf, size_t len, int flags) noexcept;

int close(int sockfd) noexcept;

}  // namespace rudp
