#include "internal/event_loop.hpp"

#include <sys/epoll.h>
#include <unistd.h>

#include <future>
#include <memory>
#include <mutex>
#include <thread>

#include "internal/common.hpp"

namespace rudp::internal {

// RUDP_ASSERT(std::this_thread::get_id() == m_thread.get_id(),
//             "Event handlers must run on event loop thread");

void event_loop::loop() noexcept {
    RUDP_ASSERT(std::this_thread::get_id() == m_thread.get_id(),
                "The core event loop must run on a dedicated thread.");

    m_thread_started.set_value();
};

event_loop::result event_loop::instance() noexcept {
    static std::once_flag initialise;
    static event_loop *instance = nullptr;
    static result::error error = result::error::NONE;

    std::call_once(initialise, []() {
        auto temp = std::make_unique<event_loop>();

        if (!temp->init_epoll()) {
            error = result::error::EPOLL_CREATION;
            return;
        }

        if (!temp->init_thread()) {
            close(temp->m_epollfd);

            error = result::error::THREAD_CREATION;
            return;
        }

        instance = temp.release();
    });

    RUDP_ASSERT(error == result::error::NONE, "All errors must prevent event loop initialisation.");
    return {.err = error, .instance = instance};
}

bool event_loop::init_epoll() noexcept {
    RUDP_ASSERT(m_epollfd == constants::UNINITIALISED_FD,
                "The event loop's epollfd cannot be created twice.");

    m_epollfd = epoll_create1(0);
    if (m_epollfd < 0) {
        return false;
    }

    return true;
}

bool event_loop::init_thread() noexcept {
    RUDP_ASSERT(m_epollfd != constants::UNINITIALISED_FD,
                "The event loop thread should not be created prior to epoll initialisation.");
    RUDP_ASSERT(is_valid_sockfd(m_epollfd),
                "The event loop thread's creation requires a valid epollfd.");
    RUDP_ASSERT(!m_thread.joinable(), "The event loop thread cannot be created twice.");

    // NOTE: This is not completely safe - a thread may not get scheduled in time and we'd exit
    // out. However, our thread creation is an exactly once operation which suceeds or fails, and
    // the failure case would be at startup (i.e. creating the loop) and likely irrecoverable. If
    // this were recoverable, or happened with data in-flight, it may be worth more robustness. This
    // seems like a hard problem out-of-scope of the purpose of this project for now.
    try {
        m_thread = std::thread(&event_loop::loop, this);

        return m_thread_started.get_future().wait_for(std::chrono::milliseconds(200)) ==
               std::future_status::ready;
    } catch (...) {
        return false;
    }

    return true;
}

linuxfd_t event_loop::epollfd() const noexcept {
    RUDP_ASSERT(assert_initialised_state());

    return m_epollfd;
}

// NOTE: We won't know who called this.
bool event_loop::assert_initialised_state() const noexcept {
    RUDP_ASSERT(m_epollfd != constants::UNINITIALISED_FD,
                "A running event loop must have an initialised epollfd.");
    RUDP_ASSERT(is_valid_sockfd(m_epollfd), "A running event loop must have a valid epollfd.");
    RUDP_ASSERT(m_thread.joinable(), "A running event loop must have a respective running thread.");

    return true;
}

}  // namespace rudp::internal
