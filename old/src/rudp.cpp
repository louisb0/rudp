#include "rudp.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <mutex>

#include "common.hpp"
#include "packet.hpp"
#include "reliability.hpp"
#include "socket.hpp"

namespace rudp {

int socket(void) {
    return internal::socket_create();
}

int bind(rudpfd_t sockfd, struct sockaddr* addr, socklen_t addrlen) {
    return ::bind(internal::socket_get(sockfd)->fd, addr, addrlen);
}

int listen(rudpfd_t sockfd, int backlog) {
    internal::socket_get(sockfd)->state = internal::socket::LISTEN;
    return 0;
}

rudpfd_t accept(rudpfd_t sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    internal::socket* listen_sock = internal::socket_get(sockfd);
    std::unique_lock<std::mutex> lock(listen_sock->mtx);

    listen_sock->cv.wait(lock, [listen_sock]() { return !listen_sock->accept_queue.empty(); });

    rudpfd_t new_fd = listen_sock->accept_queue.front();
    listen_sock->accept_queue.pop();

    if (addr && addrlen) {
        internal::socket* new_sock = internal::socket_get(new_fd);
        memcpy(addr, &new_sock->peer.addr, std::min(*addrlen, new_sock->peer.addr_len));
        *addrlen = new_sock->peer.addr_len;
    }

    return new_fd;
}

int connect(rudpfd_t sockfd, struct sockaddr* addr, socklen_t addrlen) {
    internal::socket* sock = internal::socket_get(sockfd);
    std::unique_lock<std::mutex> lock(sock->mtx);

    memcpy(&sock->peer.addr, addr, addrlen);
    sock->peer.addr_len = addrlen;
    sock->peer.seq_num = 0;

    internal::reliably_send(sock, internal::create_syn(sock));
    sock->state = internal::socket::SYN_SENT;

    sock->cv.wait(lock, [sock]() { return sock->state == internal::socket::ESTABLISHED; });
    return 0;
}

size_t send(rudpfd_t sockfd, const void* buf, size_t len, int flags);
size_t recv(rudpfd_t sockfd, void* buf, size_t len, int flags);

int close(rudpfd_t sockfd) {
    internal::socket* sock = internal::socket_get(sockfd);
    linuxfd_t fd = sock->fd;

    internal::socket_delete(sockfd);

    // TCP closing sequence
    return ::close(fd);
}

}  // namespace rudp
