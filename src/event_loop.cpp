#include "internal/event_loop.hpp"

#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "internal/assert.hpp"
#include "internal/common.hpp"

namespace rudp::internal {

static constexpr std::chrono::milliseconds thread_start_timeout = std::chrono::milliseconds(200);
static constexpr u16 max_events = 32;

RUDP_STATIC_ASSERT(max_events > 0,
                   "max_events must be non-negative or else epoll_wait() will error.");

void event_loop::loop() noexcept {
    RUDP_ASSERT(assert_correct_thread(__PRETTY_FUNCTION__));
    RUDP_ASSERT(assert_initialised_state(__PRETTY_FUNCTION__));

    m_thread_started.set_value();
    epoll_event events[max_events]{};

    while (true) {
        int nfds = epoll_wait(m_epollfd, events, max_events, 50);
        if (nfds < 0) {
            RUDP_ASSERT(errno == EINTR, "EINTR is the only possible error, but we received %s.",
                        strerror(errno));
            continue;
        }

        RUDP_ASSERT(assert_initialised_state(__PRETTY_FUNCTION__));

        // TODO: Implement batching by generating IDs as a handler lookup which we can sort.
        for (int i = 0; i < nfds; i++) {
            linuxfd_t fd = events[i].data.fd;

            auto it = m_handlers.find(fd);
            RUDP_ASSERT(it != m_handlers.end(),
                        "A handler must be registered for all epoll-registered FDs.");
            it->second->handle_event();
        }
    }
};

event_loop::result event_loop::instance() noexcept {
    static std::once_flag initialise;
    static event_loop *instance = nullptr;
    static result::error error = result::error::none;

    std::call_once(initialise, []() {
        auto loop = std::make_unique<event_loop>();

        loop->m_epollfd = epoll_create1(0);
        if (loop->m_epollfd < 0) {
            error = result::error::epoll_creation;
            return;
        }

        try {
            loop->m_thread = std::thread(&event_loop::loop, loop.get());

            // NOTE: This is not entirely safe and we could leave an event-loop running detached if
            // the OS is slow to schedule the thread. The complexity to make this reliable will take
            // away from the purpose of the project - networking.
            bool started = loop->m_thread_started.get_future().wait_for(thread_start_timeout) ==
                           std::future_status::ready;
            if (!started) {
                throw std::runtime_error("Thread start timeout. Now moving to cleanup.");
            }

            instance = loop.release();
        } catch (...) {
            if (loop->m_thread.joinable()) {
                loop->m_thread.detach();
            }

            close(loop->m_epollfd);
            error = result::error::thread_creation;
        }
    });

    return {.err = error, .instance = instance};
}

bool event_loop::add_handler(event_handler *handler) {
    RUDP_ASSERT(handler, "A handler should never be null.");

    // Get event loop and assert state.
    auto [err, event_loop] = instance();
    if (err != result::error::none) {
        // NOTE: errno is already set in the epoll_creation case.
        if (err == result::error::thread_creation) {
            errno = ENOMEM;
        }

        return false;
    }

    RUDP_ASSERT(event_loop->assert_initialised_state(__PRETTY_FUNCTION__));
    RUDP_ASSERT(!handler->m_initialised, "A handler must not be initialised twice.");

    // Register the handler's FD to epoll.
    struct epoll_event ev = {
        .events = EPOLLIN,
        .data = {.fd = handler->fd()},
    };

    if (epoll_ctl(event_loop->m_epollfd, EPOLL_CTL_ADD, handler->fd(), &ev) < 0) {
        // NOTE: errno would be set as ENOMEM or ENOSPC by epoll_ctl.
        return false;
    }

    // Register the handler and update state.
    event_loop->m_handlers[handler->fd()] = handler;
    handler->m_initialised = true;
    handler->m_event_loop = event_loop;
    RUDP_ASSERT(handler->assert_initialised_handler(__PRETTY_FUNCTION__));

    return true;
}

linuxfd_t event_loop::epollfd() const noexcept {
    RUDP_ASSERT(assert_initialised_state(__PRETTY_FUNCTION__));

    return m_epollfd;
}

bool event_loop::assert_initialised_state(const char *caller) const noexcept {
    RUDP_ASSERT(m_epollfd != constants::UNINITIALISED_FD,
                "[%s] A running event loop must have an initialised epollfd.", caller);
    RUDP_ASSERT(is_valid_sockfd(m_epollfd), "[%s] A running event loop must have a valid epollfd.",
                caller);
    RUDP_ASSERT(m_thread.joinable(),
                "[%s] A running event loop must have a respective running thread.", caller);

    return true;
}

bool event_loop::assert_correct_thread(const char *caller) const noexcept {
    RUDP_ASSERT(std::this_thread::get_id() == m_thread.get_id(),
                "[%s] The core event loop must run on a dedicated thread.", caller);

    return true;
}

}  // namespace rudp::internal
