#include "internal/listener.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>

#include <condition_variable>
#include <mutex>
#include <queue>

#include "internal/common.hpp"
#include "internal/event_loop.hpp"

namespace rudp::internal {

bool listener::init() noexcept {
    RUDP_ASSERT(!m_initialised, "A listener must not be initialised twice.");

    auto [err, event_loop] = event_loop::instance();
    if (err != event_loop::result::error::none) {
        return false;
    }

    return event_loop->add_handler(this);
}

rudpfd_t listener::wait_and_accept() noexcept {
    RUDP_ASSERT(assert_initialised_state(__PRETTY_FUNCTION__));

    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this]() { return !m_ready.empty(); });

    rudpfd_t fd = m_ready.front();
    m_ready.pop();
    return fd;
}

bool listener::assert_initialised_state(const char *caller) const noexcept {
    // Listener state
    RUDP_ASSERT(m_initialised, "[%s] A listener must be in an initialised state when asserted.",
                caller);
    RUDP_ASSERT(is_valid_sockfd(m_fd), "[%s] A listener must have a valid underlying linuxfd.",
                caller);

    // Event-loop state
    auto [err, event_loop] = event_loop::instance();

    RUDP_ASSERT(err == event_loop::result::error::none,
                "[%s] For a listener to exist, the event loop creation must not have failed.",
                caller);
    RUDP_ASSERT(event_loop != nullptr,
                "[%s] For a listener to exist, the event loop instance must be allocated.", caller);
    RUDP_ASSERT(event_loop->assert_initialised_state(__PRETTY_FUNCTION__));

    return true;
}

}  // namespace rudp::internal
