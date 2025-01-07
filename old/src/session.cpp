#include "session.hpp"

#include <sys/epoll.h>
#include <unistd.h>

#include <atomic>
#include <stdexcept>

#include "event_loop.hpp"

namespace rudp::internal {

session& session::instance() noexcept {
    static session instance;
    return instance;
}

session::session() {
    m_epollfd = epoll_create1(0);
    if (m_epollfd < 0) {
        throw std::runtime_error("Failed to create epoll instance.");
    }

    m_running = true;
    m_event_loop_thread = std::thread(event_loop);
}

session::~session() noexcept {
    m_running = false;

    if (m_event_loop_thread.joinable()) {
        m_event_loop_thread.join();
    }

    if (m_epollfd >= 0) {
        close(m_epollfd);
    }
}

void session::ref() noexcept {
    m_ref_count++;
}

void session::unref() noexcept {
    if (m_ref_count.fetch_sub(1) == 1) {
        m_running = false;
        if (m_event_loop_thread.joinable()) {
            m_event_loop_thread.join();
        }
    }
}

}  // namespace rudp::internal
