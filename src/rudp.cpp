#include <sys/socket.h>
#include <sys/types.h>

#include "internal/common.hpp"
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

int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int connect(int sockfd, struct sockaddr *addr, socklen_t addrlen);
size_t send(int sockfd, const void *buf, size_t len, int flags);
size_t recv(int sockfd, void *buf, size_t len, int flags);
int close(int sockfd);

}  // namespace rudp
