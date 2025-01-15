#pragma once

#include <future>
#include <thread>

#include "internal/common.hpp"

namespace rudp::internal {

class event_loop {
public:
    // TODO: Add noexcept to constructors.
    event_loop() : m_epollfd(constants::UNINITIALISED_FD) {}

    event_loop(const event_loop &) = delete;
    event_loop &operator=(const event_loop &) = delete;
    event_loop(event_loop &&) = delete;
    event_loop &operator=(event_loop &&) = delete;

    struct result {
        // TODO: Make enum values lower-case.
        enum class error {
            NONE,
            EPOLL_CREATION,
            THREAD_CREATION,
        };

        error err;
        event_loop *instance;
    };

    [[nodiscard]] static result instance() noexcept;

    void loop() noexcept;

    [[nodiscard]] linuxfd_t epollfd() const noexcept;
    [[nodiscard]] bool assert_initialised_state() const noexcept;

private:
    linuxfd_t m_epollfd;
    std::thread m_thread;
    std::promise<void> m_thread_started;

    [[nodiscard]] bool init_epoll() noexcept;
    [[nodiscard]] bool init_thread() noexcept;
};

}  // namespace rudp::internal
