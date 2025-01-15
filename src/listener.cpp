#include "internal/listener.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>

#include <condition_variable>
#include <mutex>
#include <queue>

#include "internal/common.hpp"
#include "internal/event_loop.hpp"
#include "internal/socket.hpp"

namespace rudp::internal {

bool listener::init() noexcept {
    RUDP_ASSERT(!m_initialised, "A listener must not be initialised twice.");

    auto [err, event_loop] = event_loop::instance();
    if (err != event_loop::result::error::NONE) {
        return false;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = m_rudpfd;

    if (epoll_ctl(event_loop->epollfd(), EPOLL_CTL_ADD, m_linuxfd, &ev) < 0) {
        // NOTE: By way of the guarantees provided by RUDP_ASSERT, the only two possible errors we
        // could get are ENOMEM and ENOSPC, both of which are fine to return back to the user.
        return false;
    }

    m_initialised = true;
    return true;
}

rudpfd_t listener::wait_and_accept() noexcept {
    RUDP_ASSERT(assert_initialised_state());

    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this]() { return !m_ready.empty(); });

    rudpfd_t fd = m_ready.front();
    m_ready.pop();
    return fd;
}

bool listener::assert_initialised_state() const noexcept {
    // Listener state
    RUDP_ASSERT(m_initialised, "A listener must be in an initialised state when asserted..");
    RUDP_ASSERT(is_valid_sockfd(m_linuxfd), "A listener must have a valid underlying linuxfd.");
    RUDP_ASSERT(g_sockets.find(m_rudpfd) != g_sockets.end(),
                "A listener must have a valid underlying rudpfd.");

    // Event-loop state
    auto [err, event_loop] = event_loop::instance();

    RUDP_ASSERT(err == event_loop::result::error::NONE,
                "For a listener to exist, the event loop creation must not have failed.");
    RUDP_ASSERT(event_loop != nullptr,
                "For a listener to exist, the event loop instance must be allocated.");
    RUDP_ASSERT(event_loop->assert_initialised_state());

    return true;
}

}  // namespace rudp::internal
