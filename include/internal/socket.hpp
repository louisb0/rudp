#pragma once

#include <condition_variable>
#include <queue>
#include <unordered_map>

#include "common.hpp"

namespace rudp::internal {

struct socket {
    linuxfd_t fd;
    enum state_t {
        CLOSED,
        LISTENING,
        SYN_SENT,
        SYN_RCVD,
        ESTABLISHED,
        FIN_WAIT_1,
        FIN_WAIT_2,
        TIME_WAIT,
        CLOSE_WAIT,
        LAST_ACK,
    } state;

    std::condition_variable cv;

    // TODO: Consider a union with connecting / listening socket types.
    struct {
        s32 backlog;
        std::queue<rudpfd_t> queue;
    } listen_data;
};

class socket_manager {
public:
    [[nodiscard]] static socket_manager &instance() noexcept;
    [[nodiscard]] rudpfd_t create() noexcept;
    [[nodiscard]] socket *get(rudpfd_t fd) noexcept;

private:
    socket_manager() : m_next_fd(0) {}
    ~socket_manager() = default;

    socket_manager(const socket_manager &) = delete;
    socket_manager &operator=(const socket_manager &) = delete;
    socket_manager(socket_manager &&) = delete;
    socket_manager &operator=(socket_manager &&) = delete;

    rudpfd_t m_next_fd;
    std::unordered_map<rudpfd_t, socket> m_sockets;
};

}  // namespace rudp::internal
