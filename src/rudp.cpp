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

    linuxfd_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

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

    linuxfd_t fd = std::get<linuxfd_t>(sock->data);
    RUDP_ASSERT(fd >= 0, "A bound socket must have a valid FD.");

    auto listener = std::make_unique<internal::listener>(fd, backlog);
    if (!listener) {
        errno = ENOMEM;
        return -1;
    }
    if (!listener->init()) {
        // NOTE: This will only be reached when epoll fails, forwarding errno.
        return -1;
    }

    sock->type = internal::socket::type::LISTENING;
    sock->data = std::move(listener);

    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int connect(int sockfd, struct sockaddr *addr, socklen_t addrlen);
size_t send(int sockfd, const void *buf, size_t len, int flags);
size_t recv(int sockfd, void *buf, size_t len, int flags);
int close(int sockfd);

}  // namespace rudp
