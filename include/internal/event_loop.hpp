#pragma once

#include <functional>
#include <future>
#include <thread>
#include <unordered_map>

#include "internal/common.hpp"

namespace rudp::internal {

enum class handler_type : u32 { listener, connection };

class event_loop {
public:
    event_loop() noexcept : m_epollfd(constants::UNINITIALISED_FD), m_running(false) {}

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
    [[nodiscard]] bool add_handler(handler_type type, linuxfd_t fd,
                                   std::function<void()> handler) noexcept;
    bool remove_handler(handler_type type, linuxfd_t fd) noexcept;

    void loop() noexcept;

    void assert_initialised_state(const char *caller) const noexcept;
    void assert_event_thread(const char *caller) const noexcept;
    void assert_user_thread(const char *caller) const noexcept;
    void assert_handler_exists(const char *caller, handler_type type, linuxfd_t fd) const noexcept;

private:
    linuxfd_t m_epollfd;
    bool m_running;

    std::thread m_thread;
    std::promise<void> m_thread_started;
    std::unordered_map<u64, std::function<void()>> m_handlers;

    u64 calculate_id(handler_type type, linuxfd_t fd) const noexcept;
};

}  // namespace rudp::internal
