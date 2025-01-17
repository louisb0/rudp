#include "internal/connection.hpp"

#include "internal/event_loop.hpp"

namespace rudp::internal {

void connection::handle_event() noexcept {
    RUDP_ASSERT(assert_initialised_handler(__PRETTY_FUNCTION__));
    RUDP_ASSERT(m_event_loop->assert_correct_thread(__PRETTY_FUNCTION__));
}

}  // namespace rudp::internal
