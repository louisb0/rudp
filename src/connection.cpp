#include "internal/connection.hpp"

#include <sys/socket.h>

#include <cstdio>
#include <cstring>

#include "internal/assert.hpp"
#include "internal/event_loop.hpp"
#include "internal/packet.hpp"

namespace rudp::internal {

std::unordered_map<connection_tuple, std::unique_ptr<connection>, connection_tuple_hash>
    g_connections;

void connection::handle_events() noexcept {
    assert_initialised_handler(__PRETTY_FUNCTION__);
    m_event_loop->assert_event_thread(__PRETTY_FUNCTION__);
}

bool connection::syn() noexcept {
    assert_initialised_handler(__PRETTY_FUNCTION__);
    m_event_loop->assert_user_thread(__PRETTY_FUNCTION__);

    RUDP_ASSERT(m_prev_state == state::closed, "A connection must be closed to send a plain SYN.");
    RUDP_ASSERT(m_state == state::closed, "A connection must be closed to send a plain SYN.");

    packet_header syn;
    syn.flags = flag::SYN;
    syn.seqnum = m_seqnum++;
    syn.acknum = 0;

    // TODO: Reliably send.
    if (sendto(m_fd, &syn, sizeof(syn), 0, m_tuple.dst(), m_tuple.addr_size()) <= 0) {
        return false;
    }

    m_prev_state = state::closed;
    m_state = state::syn_sent;

    return true;
}

bool connection::synack(packet_header pkt) noexcept {
    assert_initialised_handler(__PRETTY_FUNCTION__);
    m_event_loop->assert_event_thread(__PRETTY_FUNCTION__);

    RUDP_ASSERT(m_prev_state == state::closed,
                "A connection must be closed to process a plain SYN.");
    RUDP_ASSERT(m_state == state::closed, "A connection must be closed to process a plain SYN.");

    packet_header synack;
    synack.flags = flag::SYN | flag::ACK;
    synack.seqnum = m_seqnum++;
    synack.acknum = pkt.seqnum + 1;

    // TODO: Reliably send.
    if (sendto(m_fd, &synack, sizeof(synack), 0, m_tuple.dst(), m_tuple.addr_size()) <= 0) {
        return false;
    }

    m_prev_state = state::closed;
    m_state = state::syn_rcvd;

    return true;
}

void connection::wait_for_established() noexcept {
    assert_initialised_handler(__PRETTY_FUNCTION__);
    m_event_loop->assert_user_thread(__PRETTY_FUNCTION__);

    // TODO: This doesn't account for error states, e.g. a reset or timeout.
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this]() { return m_state == state::established; });
}

// NOTE: A state transition table is arguably cleaner but a pain to maintain. It may be a better
// choice as complexity grows.
void connection::assert_state(const char *caller) const noexcept {
    if (m_prev_state == state::closed) {
        RUDP_ASSERT(m_state == state::closed || m_state == state::syn_rcvd,
                    "[%s] A connection's closed state can lead only to remaining closed or "
                    "receiving a SYN.",
                    caller);
    }
}

}  // namespace rudp::internal
