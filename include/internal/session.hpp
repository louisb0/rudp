#pragma once
#include <sys/epoll.h>

#include <atomic>
#include <thread>

#include "common.hpp"

namespace rudp::internal {

class session {
public:
    [[nodiscard]] static session& instance() noexcept;

    session(const session&) = delete;
    session& operator=(const session&) = delete;
    session(session&&) = delete;
    session& operator=(session&&) = delete;

    [[nodiscard]] s32 epollfd() const noexcept {
        return m_epollfd;
    }

    [[nodiscard]] b8 running() const noexcept {
        return m_running;
    }

    void ref() noexcept;
    void unref() noexcept;

private:
    session();
    ~session() noexcept;

    s32 m_epollfd;
    // NOTE: Atomic doesn't seem worthwhile as the worst-case is the event loop runs one extra
    // iteration.
    b8 m_running;
    std::thread m_event_loop_thread;
    std::atomic<u16> m_ref_count{0};
};

}  // namespace rudp::internal
