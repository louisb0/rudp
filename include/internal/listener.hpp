#pragma once

#include <sys/socket.h>

#include <limits>
#include <queue>

#include "internal/assert.hpp"
#include "internal/common.hpp"
#include "internal/event_handler.hpp"

namespace rudp::internal {

RUDP_STATIC_ASSERT(SOMAXCONN <= std::numeric_limits<u16>::max(),
                   "SOMAXCONN must fit in a u16 backlog.");

class listener : public event_handler {
public:
    listener(linuxfd_t fd, u16 backlog) noexcept : event_handler(fd), m_backlog(backlog) {}

    void handle_events() noexcept;

    [[nodiscard]] rudpfd_t wait_and_accept() noexcept;

private:
    const u16 m_backlog;

    std::queue<rudpfd_t> m_ready;
};

}  // namespace rudp::internal
