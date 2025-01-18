#include "internal/event_handler.hpp"

#include "internal/assert.hpp"
#include "internal/event_loop.hpp"

namespace rudp::internal {

linuxfd_t event_handler::fd() const noexcept {
    return m_fd;
}

void event_handler::assert_initialised_handler(const char *caller) const noexcept {
    // Handler state.
    RUDP_ASSERT(m_initialised, "[%s] A connection must be in an initialised state when asserted.",
                caller);
    RUDP_ASSERT(is_valid_sockfd(m_fd), "[%s] A connection must have a valid underlying linuxfd.",
                caller);

    // Event-loop state.
    RUDP_ASSERT(m_event_loop != nullptr,
                "[%s] For a connection to exist, the event loop instance must be allocated.",
                caller);
    RUDP_ASSERT(m_event_loop->assert_initialised_state(__PRETTY_FUNCTION__));
}

}  // namespace rudp::internal
