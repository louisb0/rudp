#pragma once

#include "internal/common.hpp"

namespace rudp::internal {

class event_loop;

class event_handler {
    // NOTE: This is purely so that when registering an event_handler we can get and set
    // m_initialised and m_event_loop. This introduces an unintuitive design which I don't like,
    // but making it public public or using a setter implies multiple users of that interface.
    friend class event_loop;

public:
    event_handler(linuxfd_t fd) noexcept : m_fd(fd), m_event_loop(nullptr), m_initialised(false) {}

    virtual ~event_handler() = default;
    virtual void handle_event() = 0;

    [[nodiscard]] linuxfd_t fd() const noexcept;

    [[nodiscard]] bool assert_initialised_handler(const char *caller) const noexcept;

protected:
    linuxfd_t m_fd;
    event_loop *m_event_loop;
    bool m_initialised;
};

}  // namespace rudp::internal
