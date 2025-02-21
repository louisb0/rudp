#include "internal/event_handler.hpp"

#include "internal/assert.hpp"
#include "internal/event_loop.hpp"

namespace rudp::internal {

// TODO: This is an assertion of state which doesn't even need to exist. The event_handler
// abstraction was a mistake - see comment on add_handler().
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
    m_event_loop->assert_initialised_state(__PRETTY_FUNCTION__);
}

}  // namespace rudp::internal
