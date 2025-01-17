#pragma once

#include <future>
#include <thread>
#include <unordered_map>

#include "internal/common.hpp"

namespace rudp::internal {

class event_handler {
    friend class event_loop;

public:
    event_handler(linuxfd_t fd) noexcept : m_fd(fd), m_initialised(false) {}

    virtual ~event_handler() = default;
    virtual void handle_event() = 0;

    [[nodiscard]] linuxfd_t fd() const noexcept {
        return m_fd;
    }

protected:
    linuxfd_t m_fd;
    bool m_initialised;
};

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

private:
    linuxfd_t m_epollfd;
    std::thread m_thread;
    std::promise<void> m_thread_started;

    std::unordered_map<linuxfd_t, event_handler *> m_handlers;

    [[nodiscard]] bool init_epoll() noexcept;
    [[nodiscard]] bool init_thread() noexcept;
};

}  // namespace rudp::internal
