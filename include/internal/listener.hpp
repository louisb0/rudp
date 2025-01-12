#pragma once

#include <sys/socket.h>

#include <limits>
#include <queue>

#include "internal/common.hpp"

namespace rudp::internal {

RUDP_STATIC_ASSERT(SOMAXCONN <= std::numeric_limits<u16>::max(),
                   "SOMAXCONN must fit in a u16 backlog.");

class listener {
public:
    listener(linuxfd_t fd, u16 backlog) : m_fd(fd), m_backlog(backlog) {};

    [[nodiscard]] bool init() noexcept;

private:
    linuxfd_t m_fd;
    std::queue<rudpfd_t> m_ready;
    u16 m_backlog;
};

}  // namespace rudp::internal
