#include "internal/event_loop.hpp"

#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <functional>
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
    m_running = true;

    assert_initialised_state(__PRETTY_FUNCTION__);
    assert_event_thread(__PRETTY_FUNCTION__);

    m_thread_started.set_value();
    epoll_event events[max_events]{};

    while (m_running) {
        int nfds = epoll_wait(m_epollfd, events, max_events, 50);
        if (nfds < 0) {
            RUDP_ASSERT(errno == EINTR, "EINTR is the only possible error, but we received %s.",
                        strerror(errno));
            continue;
        }

        assert_initialised_state(__PRETTY_FUNCTION__);

        // NOTE: The 64-bits of event data has the lower 32-bits as the handler type, and upper
        // 32-bits as the file descriptor. This allows us to batch by handler type.
        std::sort(events, events + nfds, [](const epoll_event &a, const epoll_event &b) {
            return a.data.u64 < b.data.u64;
        });

        for (int i = 0; i < nfds; i++) {
            u64 id = events[i].data.u64;
            // TODO: Use .contains()
            RUDP_ASSERT(m_handlers.count(id) == 1,
                        "There must exist a handler for every epoll registered file descriptor.");

            m_handlers[id]();
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
            // away from the purpose of the project: networking.
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

bool event_loop::add_handler(handler_type type, linuxfd_t fd,
                             std::function<void()> handler) noexcept {
    RUDP_ASSERT(type == handler_type::connection || type == handler_type::listener,
                "A handler must be of type connection or listener.");
    RUDP_ASSERT(is_valid_sockfd(fd),
                "add_handler() must never be called with an invalid underlying file descriptor.");

    u64 id = calculate_id(type, fd);
    RUDP_ASSERT(m_handlers.count(id) == 0, "A handler must not be added twice.");

    // Register the handler.
    struct epoll_event ev = {
        .events = EPOLLIN,
        .data = {.u64 = id},
    };

    if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        RUDP_ASSERT(errno == ENOMEM || errno == ENOSPC,
                    "epoll_ctl() can only fail due to the environment, but we got %s.",
                    strerror(errno));
        return false;
    }

    m_handlers[id] = handler;
    return true;
}

bool event_loop::remove_handler(handler_type type, linuxfd_t fd) noexcept {
    RUDP_ASSERT(type == handler_type::connection || type == handler_type::listener,
                " A handler must be of type connection or listener.");
    RUDP_ASSERT(
        is_valid_sockfd(fd),
        "remove_handler() must never be called with an invalid underlying file descriptor.");

    u64 id = calculate_id(type, fd);
    RUDP_ASSERT(m_handlers.count(id) == 1, "A handler must exist in order to be deleted.");

    // Deregister the handler.
    if (epoll_ctl(m_epollfd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        RUDP_ASSERT(errno == ENOMEM || errno == ENOSPC,
                    "epoll_ctl() can only fail due to the environment, but we got %s.",
                    strerror(errno));
        return false;
    }

    m_handlers.erase(id);
    return true;
}

u64 event_loop::calculate_id(handler_type type, linuxfd_t fd) const noexcept {
    return (static_cast<u64>(type) << 32) | static_cast<u32>(fd);
}

void event_loop::assert_initialised_state(const char *caller) const noexcept {
    RUDP_ASSERT(m_epollfd != constants::UNINITIALISED_FD,
                "[%s] A running event loop must have an initialised epollfd.", caller);
    RUDP_ASSERT(is_valid_epollfd(m_epollfd),
                "[%s] %d A running event loop must have a valid epollfd.", caller, errno);
    RUDP_ASSERT(m_running, "[%s] A running event loop must have a respective running thread.",
                caller);
}

// NOTE: These asserts assume the thread is already initialised.
void event_loop::assert_event_thread(const char *caller) const noexcept {
    RUDP_ASSERT(std::this_thread::get_id() == m_thread.get_id(),
                "[%s] The caller should run only on the event thread.", caller);
}

void event_loop::assert_user_thread(const char *caller) const noexcept {
    RUDP_ASSERT(std::this_thread::get_id() != m_thread.get_id(),
                "[%s] The caller should run only on the user thread.", caller);
}
// NOTE ends

void event_loop::assert_handler_exists(const char *caller, handler_type type,
                                       linuxfd_t fd) const noexcept {
    u64 id = calculate_id(type, fd);
    RUDP_ASSERT(m_handlers.count(id) == 1,
                "[%s] The handler of type=%d and fd=%d must be registered.", caller,
                static_cast<u32>(type), fd);
}

}  // namespace rudp::internal
