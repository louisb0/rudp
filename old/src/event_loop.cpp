#include "event_loop.hpp"

#include <sys/epoll.h>

#include <cstring>
#include <mutex>

#include "common.hpp"
#include "packet.hpp"
#include "reliability.hpp"
#include "session.hpp"
#include "socket.hpp"

namespace rudp::internal {

constexpr int max_events = 16;

static void handle_syn(socket* const sock);
static void handle_synack(socket* const sock);
static void handle_ack(socket* const sock, rudpfd_t sockfd);

void event_loop() {
    epoll_event events[max_events];
    session& session = session::instance();

    while (session.running()) {
        int nfds = epoll_wait(session.epollfd(), events, max_events, 50);
        if (nfds < 0) {
            break;  // need proper handling
        }

        for (int i = 0; i < nfds; i++) {
            rudpfd_t sockfd = static_cast<rudpfd_t>(events[i].data.u32);
            socket* sock = socket_get(sockfd);

            switch (sock->state) {
            case socket::LISTEN:
                handle_syn(sock);
                break;

            case socket::SYN_SENT:
                handle_synack(sock);
                break;

            case socket::SYN_RCVD:
                handle_ack(sock, sockfd);
                break;

            case socket::CLOSED:
            case socket::ESTABLISHED:
                break;
            }
        }

        retransmit();
    }
}

static void handle_syn(socket* const sock) {
    assert(sock->state == socket::LISTEN);
    assert(sock->seq_num == 0);
    assert(sock->ack_num == 0);

    std::lock_guard<std::mutex> lock(sock->mtx);

    std::optional<packet> syn = reliably_recv(sock);
    if (!syn.has_value()) {
        return;
    }

    rudpfd_t new_socket_fd = socket_create();
    socket* new_sock = socket_get(new_socket_fd);

    struct sockaddr_in local_addr;
    socklen_t local_addr_len = sizeof(local_addr);
    getsockname(sock->fd, reinterpret_cast<struct sockaddr*>(&local_addr), &local_addr_len);

    if (bind(new_sock->fd, reinterpret_cast<struct sockaddr*>(&local_addr), local_addr_len) < 0) {
        // TODO handle
        socket_delete(new_socket_fd);
        return;
    }

    new_sock->producer = sock;

    new_sock->peer = sock->peer;
    new_sock->peer.seq_num = syn->seq_num;

    new_sock->seq_num = 0;
    new_sock->ack_num = syn->seq_num + 1;
    new_sock->state = socket::SYN_RCVD;

    reliably_send(new_sock, create_synack(new_sock));
}

static void handle_synack(socket* const sock) {
    assert(sock->state == socket::SYN_SENT);
    assert(sock->seq_num == 1);
    assert(sock->ack_num == 0);
    assert(sock->peer.seq_num == 0);

    std::lock_guard<std::mutex> lock(sock->mtx);

    std::optional<packet> synack = reliably_recv(sock);
    if (!synack.has_value()) {
        return;
    }

    sock->peer.seq_num = synack->seq_num;

    reliably_send(sock, create_ack(sock));

    sock->state = socket::ESTABLISHED;
    sock->cv.notify_one();
}

static void handle_ack(socket* const sock, rudpfd_t sockfd) {
    assert(sock->state == socket::SYN_RCVD);
    assert(sock->seq_num == 1);
    assert(sock->ack_num == 1);
    assert(sock->peer.seq_num == 0);

    std::unique_lock<std::mutex> connected_lock(sock->mtx);

    std::optional<packet> ack = reliably_recv(sock);
    if (!ack.has_value()) {
        return;
    }

    sock->peer.seq_num = ack->seq_num;
    sock->state = socket::ESTABLISHED;

    connected_lock.unlock();

    std::lock_guard<std::mutex> producer_lock(sock->producer->mtx);
    sock->producer->accept_queue.push(sockfd);
    sock->producer->cv.notify_one();
}

}  // namespace rudp::internal
