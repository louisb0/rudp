#pragma once

#include <netinet/in.h>
#include <sys/socket.h>

#include <condition_variable>
#include <mutex>
#include <queue>

#include "common.hpp"

namespace rudp::internal {

extern std::mutex g_sockets_mtx;

struct socket {
    linuxfd_t fd;

    struct peer {
        struct sockaddr_in addr;
        socklen_t addr_len;
        u32 seq_num;
    } peer;

    enum state {
        CLOSED,
        LISTEN,
        SYN_SENT,
        SYN_RCVD,
        ESTABLISHED,
    } state;

    u32 seq_num;
    u32 ack_num;

    std::condition_variable cv;
    std::mutex mtx;

    // Only for listening sockets.
    socket* producer;
    std::queue<rudpfd_t> accept_queue;
};

[[nodiscard]] rudpfd_t socket_create() noexcept;
[[nodiscard]] socket* socket_get(rudpfd_t fd) noexcept;
void socket_delete(rudpfd_t fd) noexcept;

}  // namespace rudp::internal
