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
    listener(rudpfd_t rudpfd, linuxfd_t linuxfd, u16 backlog) noexcept
        : m_rudpfd(rudpfd), m_linuxfd(linuxfd), m_backlog(backlog), m_initialised(false) {};
    [[nodiscard]] bool init() noexcept;

    [[nodiscard]] rudpfd_t wait_and_accept() noexcept;
    [[nodiscard]] bool assert_initialised_state(const char *caller) const noexcept;

private:
    const rudpfd_t m_rudpfd;
    const linuxfd_t m_linuxfd;
    const u16 m_backlog;
    bool m_initialised;

    std::mutex m_mtx;
    std::condition_variable m_cv;
    std::queue<rudpfd_t> m_ready;
};

}  // namespace rudp::internal
