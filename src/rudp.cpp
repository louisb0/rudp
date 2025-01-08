#include "rudp.hpp"

#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>

#include "internal/common.hpp"
#include "internal/socket.hpp"

namespace rudp {

int socket(void) {
    return internal::socket_manager::instance().create();
}

int bind(rudpfd_t sockfd, struct sockaddr *addr, socklen_t addrlen) {
    internal::socket *socket = internal::socket_manager::instance().get(sockfd);

    if (!socket) {
        errno = EBADF;
        return -1;
    }

    RUDP_ASSERT(socket->fd >= 0, "all sockets should have a valid underlying fd");

    if (socket->state != internal::socket::CLOSED) {
        errno = EOPNOTSUPP;
        return -1;
    }

    // NOTE: We are handing validation of addr/addrlen to the kernel.
    return ::bind(socket->fd, addr, addrlen);
}

int listen(rudpfd_t sockfd, int backlog) {
    internal::socket *socket = internal::socket_manager::instance().get(sockfd);

    if (!socket) {
        errno = EBADF;
        return -1;
    }

    RUDP_ASSERT(socket->fd >= 0, "all sockets should have a valid underlying fd");

    if (backlog < 0 || backlog > SOMAXCONN) {
        errno = EINVAL;
        return -1;
    }

    if (socket->state != internal::socket::CLOSED) {
        errno = EOPNOTSUPP;
        return -1;
    }

    RUDP_ASSERT(socket->listen_data.backlog == -1, "reused listen socket not cleaned up");
    RUDP_ASSERT(socket->listen_data.queue.empty(), "reused listen socket not cleaned up");

    socket->state = internal::socket::LISTENING;
    socket->listen_data.backlog = backlog;

    return 0;
}

int accept(rudpfd_t sockfd, struct sockaddr *addr, socklen_t *addrlen);
int connect(rudpfd_t sockfd, struct sockaddr *addr, socklen_t addrlen);
size_t send(rudpfd_t sockfd, const void *buf, size_t len, int flags);
size_t recv(rudpfd_t sockfd, void *buf, size_t len, int flags);
int close(rudpfd_t sockfd);

}  // namespace rudp
