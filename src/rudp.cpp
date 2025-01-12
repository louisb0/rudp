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

    if (backlog < 0 || backlog > SOMAXCONN) {
        errno = EINVAL;
        return -1;
    }

    if (socket->state != internal::socket::CLOSED) {
        errno = EOPNOTSUPP;
        return -1;
    }
    RUDP_ASSERT(socket->listen_data.backlog == internal::UNINITALISED_BACKLOG,
                "closed socket has initialised listening data");
    RUDP_ASSERT(socket->listen_data.queue.empty(), "closed socket has initialised listening data");

    socket->state = internal::socket::LISTENING;
    socket->listen_data.backlog = backlog;

    return 0;
}

int accept(rudpfd_t sockfd, struct sockaddr * /** addr */, socklen_t * /** addrlen */) {
    internal::socket *socket = internal::socket_manager::instance().get(sockfd);

    if (!socket) {
        errno = EBADF;
        return -1;
    }

    if (socket->state != internal::socket::LISTENING) {
        errno = EOPNOTSUPP;
        return -1;
    }

    // TODO: Block with CV until there is an rudpfd on the socket's listen queue.
    // TODO: Return the rudpfd to the user.

    return 0;
}

int connect(rudpfd_t sockfd, struct sockaddr * /** addr */, socklen_t /** addrlen */) {
    internal::socket *socket = internal::socket_manager::instance().get(sockfd);

    if (!socket) {
        errno = EBADF;
        return -1;
    }

    if (socket->state != internal::socket::CLOSED) {
        errno = EOPNOTSUPP;
        return -1;
    }

    // TODO: Send out a SYN.
    // TODO: Transition to SYN_SENT.
    // TODO: Block with CV until handshake completed, or error.

    return 0;
}

size_t send(rudpfd_t sockfd, const void *buf, size_t len, int flags);
size_t recv(rudpfd_t sockfd, void *buf, size_t len, int flags);

int close(rudpfd_t sockfd) {
    internal::socket *socket = internal::socket_manager::instance().get(sockfd);

    if (!socket) {
        errno = EBADF;
        return -1;
    }

    switch (socket->state) {
    case internal::socket::CLOSED:
        break;

    case internal::socket::SYN_SENT:
    case internal::socket::LISTENING:
        // TODO: Transition to CLOSED.
        break;

    case internal::socket::ESTABLISHED:
    case internal::socket::SYN_RCVD:
        // TODO: Send out a FIN.
        // TODO: Transition to FIN_WAIT_1.
        // TODO: Block with CV until TIME_WAIT state is reached.
        break;

    default:
        errno = EOPNOTSUPP;
        return -1;
    }

    // NOTE: This will fail only if the close() syscall fails, setting errno.
    if (!internal::socket_manager::instance().destroy(sockfd)) {
        return -1;
    }

    return 0;
}

}  // namespace rudp
