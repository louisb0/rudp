#pragma once

#include <sys/socket.h>

#include <condition_variable>
#include <limits>
#include <mutex>
#include <queue>

#include "internal/assert.hpp"
#include "internal/common.hpp"

namespace rudp::internal {

RUDP_STATIC_ASSERT(SOMAXCONN <= std::numeric_limits<u16>::max(),
                   "SOMAXCONN must fit in a u16 backlog.");

class listener {
public:
    listener(linuxfd_t fd, u16 backlog) noexcept : m_fd(fd), m_backlog(backlog) {}

    void handle_events() noexcept;

    [[nodiscard]] rudpfd_t wait_and_accept() noexcept;

private:
    const linuxfd_t m_fd;
    const u16 m_backlog;

    std::queue<rudpfd_t> m_ready;
    std::condition_variable m_cv;
    std::mutex m_mtx;

    void assert_external_state(const char *caller) const noexcept;
};

}  // namespace rudp::internal
