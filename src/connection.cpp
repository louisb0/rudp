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

    while (true) {
        // Receive pending data.
        packet_header pkt;
        sockaddr_in connecting_addr{};
        socklen_t connecting_addr_len = sizeof(connecting_addr);

        ssize_t bytes =
            recvfrom(m_fd, &pkt, sizeof(pkt), 0,
                     reinterpret_cast<struct sockaddr *>(&connecting_addr), &connecting_addr_len);
        if (bytes < 0 && errno == EWOULDBLOCK) {
            break;
        }
        if (bytes == 0) {
            break;
        }

        if (connecting_addr.sin_family != AF_INET) {
            continue;
        }

        if (m_peer_known && !m_tuple.equals_dst(&connecting_addr)) {
            continue;
        }

        // Dispatch the packet accordingly.
        switch (m_state) {
        case state::syn_sent: {
            // Received: nothing
            // Sent: SYN from closed
            // Expecting: SYNACK
            // Replying: ACK
            if (pkt.flags != (flag::SYN | flag::ACK)) {
                continue;
            }

            set_peer(connecting_addr);

            packet_header ack{};
            ack.seqnum = m_seqnum++;
            ack.acknum = pkt.seqnum + 1;
            ack.flags = flag::ACK;

            if (sendto(m_fd, &ack, sizeof(ack), 0, m_tuple.dst(), m_tuple.addr_size()) <= 0) {
                perror("wtf1");
            }

            m_prev_state = m_state;
            m_state = state::established;
            m_cv.notify_one();
            break;
        }

        case state::syn_rcvd: {
            // Received: SYN in closed
            // Sent: SYNACK
            // Expecting: ACK
            // Replying: nothing
            if (pkt.flags != flag::ACK) {
                continue;
            }

            m_prev_state = m_state;
            m_state = state::established;
            break;
        }

        case state::closed:
        case state::established:
            RUDP_UNREACHABLE();
        }
    }
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

void connection::set_peer(const sockaddr_in &addr) {
    m_peer_known = true;
    m_tuple.dst_ip = addr.sin_addr.s_addr;
    m_tuple.dst_port = addr.sin_port;
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
