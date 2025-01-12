#pragma once

#include <condition_variable>
#include <memory>
#include <queue>
#include <unordered_map>

#include "internal/common.hpp"

namespace rudp::internal {

inline constexpr s8 UNINITALISED_BACKLOG = -1;

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

    struct listen_data_t {
        s32 backlog;
        std::queue<rudpfd_t> queue;
    } listen_data;

    // NOTE: Synchronisation primiatives are not movable, copyable, or deleteable. We manage sockets
    // in containers which requires these properties, hence the unique_ptr. We assume a
    // single-threaded user layer - there are no race conditions around deletion.
    std::unique_ptr<std::mutex> mtx;
    std::unique_ptr<std::condition_variable> cv;
};

class socket_manager {
public:
    [[nodiscard]] static socket_manager &instance() noexcept;
    [[nodiscard]] rudpfd_t create() noexcept;
    [[nodiscard]] socket *get(rudpfd_t fd) noexcept;
    bool destroy(rudpfd_t fd) noexcept;

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
