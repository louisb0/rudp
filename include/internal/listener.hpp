#pragma once

#include <sys/socket.h>

#include <condition_variable>
#include <limits>
#include <mutex>
#include <queue>

#include "internal/assert.hpp"
#include "internal/common.hpp"
#include "internal/event_loop.hpp"

namespace rudp::internal {

RUDP_STATIC_ASSERT(SOMAXCONN <= std::numeric_limits<u16>::max(),
                   "SOMAXCONN must fit in a u16 backlog.");

class listener : public event_handler {
public:
    listener(linuxfd_t fd, u16 backlog) noexcept : event_handler(fd), m_backlog(backlog) {}
    [[nodiscard]] bool init() noexcept;

    void handle_event() noexcept;

    [[nodiscard]] rudpfd_t wait_and_accept() noexcept;
    [[nodiscard]] bool assert_initialised_state(const char *caller) const noexcept;

private:
    const u16 m_backlog;

    std::mutex m_mtx;
    std::condition_variable m_cv;
    std::queue<rudpfd_t> m_ready;
};

}  // namespace rudp::internal
