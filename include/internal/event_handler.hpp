#pragma once

#include <condition_variable>
#include <mutex>

#include "internal/common.hpp"

namespace rudp::internal {

class event_loop;

class event_handler {
    friend class event_loop;

public:
    event_handler(linuxfd_t fd) noexcept : m_fd(fd), m_event_loop(nullptr), m_initialised(false) {}

    virtual ~event_handler() = default;
    virtual void handle_events() = 0;

    void assert_initialised_handler(const char *caller) const noexcept;

protected:
    linuxfd_t m_fd;
    event_loop *m_event_loop;
    bool m_initialised;

    std::mutex m_mtx;
    std::condition_variable m_cv;
};

}  // namespace rudp::internal
