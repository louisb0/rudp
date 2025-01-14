#pragma once

#include <sys/socket.h>

#include <condition_variable>
#include <limits>
#include <mutex>
#include <queue>

#include "internal/common.hpp"

namespace rudp::internal {

RUDP_STATIC_ASSERT(SOMAXCONN <= std::numeric_limits<u16>::max(),
                   "SOMAXCONN must fit in a u16 backlog.");

class listener {
public:
    listener(linuxfd_t fd, u16 backlog) : m_fd(fd), m_backlog(backlog) {};
    [[nodiscard]] bool init() noexcept;

    [[nodiscard]] rudpfd_t wait_and_accept() noexcept;
    // {
    // NOTE: It is not possible for the listener to be deleted from another thread while we are
    // blocked waiting for a connection to accept, as only the user-interface thread can perform
    // that action.
    //
    //     std::unique_lock<std::mutex> lock(m_mtx);
    //     m_cv.wait(lock, [this]() { return !m_ready.empty(); });
    //
    //     rudpfd_t fd = m_ready.front();
    //     m_ready.pop();
    //     return fd;
    // }
    [[nodiscard]] bool assert_state() const noexcept;

private:
    const linuxfd_t m_fd;
    const u16 m_backlog;

    std::mutex m_mtx;
    std::condition_variable m_cv;
    std::queue<rudpfd_t> m_ready;
};

}  // namespace rudp::internal
