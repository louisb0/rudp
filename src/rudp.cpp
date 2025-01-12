#include <sys/socket.h>
#include <sys/types.h>

#include <memory>

#include "internal/common.hpp"
#include "internal/listener.hpp"
#include "internal/socket.hpp"

namespace rudp {

int socket(void) {
    rudpfd_t fd = internal::next_fd++;
    internal::g_sockets.emplace(fd);
    return fd;
}

int bind(int sockfd, struct sockaddr *addr, socklen_t addrlen) {
    if (addr->sa_family != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    auto it = internal::g_sockets.find(sockfd);
    if (it == internal::g_sockets.end()) {
        errno = EBADF;
        return -1;
    }

    internal::socket *sock = &it->second;
    if (sock->type != internal::socket::type::CREATED) {
        errno = EOPNOTSUPP;
        return -1;
    }

    RUDP_ASSERT(std::holds_alternative<std::monostate>(sock->data),
                "A socket in the CREATED state must hold a std::monostate.");

    linuxfd_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    // NOTE: Validation of addr and addlen are being forwarded to the kernel.
    if (::bind(fd, addr, addrlen) < 0) {
        int saved_errno = errno;
        ::close(fd);
        errno = saved_errno;
        return -1;
    }

    sock->type = internal::socket::type::BOUND;
    sock->data = fd;

    return 0;
}

int listen(int sockfd, int backlog) {
    if (backlog < 0 || backlog > SOMAXCONN) {
        errno = EINVAL;
        return -1;
    }

    auto it = internal::g_sockets.find(sockfd);
    if (it == internal::g_sockets.end()) {
        errno = EBADF;
        return -1;
    }

    internal::socket *sock = &it->second;
    if (sock->type != internal::socket::type::BOUND) {
        errno = EOPNOTSUPP;
        return -1;
    }

    RUDP_ASSERT(std::holds_alternative<linuxfd_t>(sock->data),
                "A socket in the BOUND state must hold a linuxfd_t.");

    // TODO: Would it be worthwhile validating the FD with a syscall instead of by-value?
    linuxfd_t fd = std::get<linuxfd_t>(sock->data);
    RUDP_ASSERT(fd >= 0, "A socket in the BOUND state must hold a valid linuxfd_t.");

    auto listener = std::make_unique<internal::listener>(fd, backlog);
    if (!listener) {
        errno = ENOMEM;
        return -1;
    }

    if (!listener->init()) {
        // NOTE: This will only be reached when epoll fails, forwarding errno.
        return -1;
    }

    // TODO: Consider whether this should be a post-invariant of listener::init().
    RUDP_ASSERT(listener->assert_state(),
                "This message will not be shown due to assert_state() calling RUDP_ASSERT");

    sock->type = internal::socket::type::LISTENING;
    sock->data = std::move(listener);

    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    if (addr != nullptr) {
        if (addrlen == nullptr) {
            errno = EINVAL;
            return -1;
        }

        if (*addrlen < sizeof(struct sockaddr_in)) {
            errno = EINVAL;
            return -1;
        }
    }

    auto it = internal::g_sockets.find(sockfd);
    if (it == internal::g_sockets.end()) {
        errno = EBADF;
        return -1;
    }

    internal::socket *sock = &it->second;
    if (sock->type != internal::socket::type::LISTENING) {
        errno = EOPNOTSUPP;
        return -1;
    }

    RUDP_ASSERT(std::holds_alternative<std::unique_ptr<internal::listener>>(sock->data),
                "A socket in the LISTENING state must hold a listener.");

    auto &listener = std::get<std::unique_ptr<internal::listener>>(sock->data);

    RUDP_ASSERT(listener != nullptr,
                "A socket in the LISTENING state must hold a non-null listener.");
    RUDP_ASSERT(listener->assert_state(),
                "This message will not be shown due to assert_state() calling RUDP_ASSERT");

    return listener->wait_and_accept();
}

int connect(int sockfd, struct sockaddr *addr, socklen_t addrlen);
size_t send(int sockfd, const void *buf, size_t len, int flags);
size_t recv(int sockfd, void *buf, size_t len, int flags);
int close(int sockfd);

}  // namespace rudp
