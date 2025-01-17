#pragma once

#include <future>
#include <thread>
#include <unordered_map>

#include "internal/common.hpp"
#include "internal/event_handler.hpp"

namespace rudp::internal {

class event_loop {
public:
    event_loop() noexcept : m_epollfd(constants::UNINITIALISED_FD) {}

    event_loop(const event_loop &) = delete;
    event_loop &operator=(const event_loop &) = delete;
    event_loop(event_loop &&) = delete;
    event_loop &operator=(event_loop &&) = delete;

    struct result {
        enum class error {
            none,
            epoll_creation,
            thread_creation,
        };

        error err;
        event_loop *instance;
    };

    [[nodiscard]] static result instance() noexcept;
    [[nodiscard]] static bool add_handler(event_handler *handler);
    [[nodiscard]] static bool remove_handler(event_handler *handler);

    void loop() noexcept;

    [[nodiscard]] linuxfd_t epollfd() const noexcept;
    [[nodiscard]] bool assert_initialised_state(const char *caller) const noexcept;
    [[nodiscard]] bool assert_correct_thread(const char *caller) const noexcept;

private:
    linuxfd_t m_epollfd;
    std::thread m_thread;
    std::promise<void> m_thread_started;

    std::unordered_map<linuxfd_t, event_handler *> m_handlers;
};

}  // namespace rudp::internal
