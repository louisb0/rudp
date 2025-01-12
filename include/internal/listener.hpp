#pragma once

#include <queue>

#include "internal/common.hpp"

namespace rudp::internal {

class listener {
    listener(u16 backlog) : m_fd(constants::UNINITIALISED_FD), m_backlog(backlog) {};

    [[nodiscard]] bool init() noexcept;

    // std::mutex m_mtx;
    // std::condition_variable m_cv;

private:
    linuxfd_t m_fd;
    std::queue<rudpfd_t> m_ready;
    u16 m_backlog;
};

}  // namespace rudp::internal
