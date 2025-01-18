#include "internal/connection.hpp"

#include "internal/assert.hpp"
#include "internal/event_loop.hpp"
#include "internal/packet.hpp"

namespace rudp::internal {

void connection::handle_event() noexcept {
    RUDP_ASSERT(assert_initialised_handler(__PRETTY_FUNCTION__));
    RUDP_ASSERT(m_event_loop->assert_correct_thread(__PRETTY_FUNCTION__));
}

bool connection::on_syn(packet_header pkt) noexcept {
    // NOTE: We assert for this handler and not the others as this is public facing.
    RUDP_ASSERT(assert_initialised_handler(__PRETTY_FUNCTION__));
    RUDP_ASSERT(m_event_loop->assert_correct_thread(__PRETTY_FUNCTION__));

    RUDP_ASSERT(m_prev_state == state::closed, "A connection must be closed to process a raw SYN.");
    RUDP_ASSERT(m_state == state::closed, "A connection must be closed to process a raw SYN.");

    packet_header synack;
    synack.flags = flag::SYN | flag::ACK;
    synack.seqnum = m_seqnum++;
    synack.acknum = pkt.seqnum + 1;

    if (!send_packet(synack)) {
        return false;
    }

    m_prev_state = state::closed;
    m_state = state::syn_rcvd;

    return true;
}

// TODO: Make these types of functions void and remove their RUDP_ASSERT wrappers. The purpose of
// wrapping them was to be able to enable/disable them with ifdefs but we will be keeping them on in
// the release build anyway.
// TODO: A state transition table is arguably cleaner but a pain to maintain. It may be a better
// choice as complexity grows.
bool connection::assert_state(const char *caller) const noexcept {
    if (m_prev_state == state::closed) {
        RUDP_ASSERT(m_state == state::closed || m_state == state::syn_rcvd,
                    "[%s] A connection's closed state can lead only to remaining closed or "
                    "receiving a SYN.",
                    caller);
        return false;
    }

    return true;
}

}  // namespace rudp::internal
