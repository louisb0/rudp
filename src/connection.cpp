#include "internal/connection.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "internal/assert.hpp"
#include "internal/common.hpp"
#include "internal/event_loop.hpp"
#include "internal/packet.hpp"
#include "internal/state.hpp"

namespace rudp::internal {
namespace {
    [[nodiscard]] static bool addr_equals(const sockaddr_in &first, const sockaddr_in &second) {
        return (first.sin_addr.s_addr == second.sin_addr.s_addr) &&
               (first.sin_port == second.sin_port);
    }
}  // namespace

std::map<std::chrono::steady_clock::time_point, std::unique_ptr<class connection>>
    g_time_wait_connections;

void connection::handle_events() noexcept {
    assert_external_state(__PRETTY_FUNCTION__);

    buffer_pending();

    while (!m_received.empty() && m_acknum == m_received.begin()->first) {
        const auto &[packet, peer] = m_received.begin()->second;
        RUDP_ASSERT(packet.header.seqnum == m_received.begin()->first,
                    "A received packet in m_received must have it's sequence number as it's key.");

        if (packet.header.flags & (static_cast<u8>(flag::SYN) | static_cast<u8>(flag::ACK))) {
            handle_synack(packet, peer);
        }

        if (packet.header.flags & static_cast<u8>(flag::ACK)) {
            handle_ack(packet);
        }

        m_received.erase(packet.header.seqnum);
        m_acknum += packet.header.length;
    }

    u8 flags = m_state.derive_flags();
    if (flags != state::NO_FLAGS) {
        send_store_packet(flags);
    }
}

void connection::buffer_pending() noexcept {
    while (true) {
        sockaddr_in peer_addr{};

        std::optional<packet> packet_opt = packet::recvfrom(m_fd, &peer_addr);
        if (!packet_opt.has_value()) {
            RUDP_ASSERT(
                errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR,
                "recvfrom() can only fail due to non-blocking or interruption, but we got %s.",
                strerror(errno));
            break;
        }

        if (peer_addr.sin_family != AF_INET) {
            continue;
        }

        if (!addr_equals(m_peer, constants::UNINITIALISED_PEER) &&
            !addr_equals(m_peer, peer_addr)) {
            continue;
        }

        packet packet = packet_opt.value();
        m_received[packet.header.seqnum] = {packet, peer_addr};
    }
}

void connection::handle_ack(const packet &packet) noexcept {
    RUDP_ASSERT(packet.header.seqnum == m_acknum,
                "m_acknum must be in sync with the current packet being handled.");
    RUDP_ASSERT(!addr_equals(m_peer, constants::UNINITIALISED_PEER),
                "A connection must have processed a valid SYN(ACK) from our peer prior to "
                "processing an individual ACK.");

    while (!m_sent.empty() && m_sent.begin()->first < packet.header.acknum) {
        const auto &[sent_packet, _, __] = m_sent.begin()->second;
        RUDP_ASSERT(sent_packet.header.seqnum == m_sent.begin()->first,
                    "A sent packet in m_sent must have it's sequence number as it's key.");

        m_sent.erase(sent_packet.header.seqnum);
    }

    // NOTE: This is handling an 'ACK' response to our 'SYNACK'; it is the transition following
    // connection::passive_open().
    if (m_state.current() == state::kind::syn_rcvd) {
        m_state.transition(state::kind::established);
    }
}

void connection::handle_synack(const packet &packet, const sockaddr_in &peer) noexcept {
    RUDP_ASSERT(packet.header.seqnum == m_acknum,
                "m_acknum must be in sync with the current packet being handled.");

    // NOTE: This is handling a 'SYNACK' response to our 'SYN'; it is the transition following
    // connection::active_open().
    if (m_state.current() == state::kind::syn_sent) {
        RUDP_ASSERT(addr_equals(m_peer, constants::UNINITIALISED_PEER),
                    "A connection cannot have processed a packet from it's peer prior to a "
                    "transition out of SYN_SENT.");

        m_peer = peer;
        m_state.transition(state::kind::established);

        m_cv.notify_one();
    }
}

void connection::retransmit() noexcept {
    for (const auto &[_, sent_packet] : m_sent) {
        if (sent_packet.retransmits == constants::MAX_RETRANSMITS) {
            RUDP_ASSERT(false, "Max retransmits reached; you must decide how to handle this.");
        }

        auto now = std::chrono::steady_clock::now();
        if (now - sent_packet.sent_at > constants::RETRANSMIT_TIME) {
            packet::sendto(m_fd, sent_packet.packet, &m_peer);
        }
    }
}

bool connection::passive_open(const sockaddr_in &peer) noexcept {
    RUDP_ASSERT(addr_equals(m_peer, constants::UNINITIALISED_PEER),
                "A connection cannot cannot both respond to an intial open and have previously "
                "processed a packet from it's peer.");

    m_peer = peer;
    m_state.transition(state::kind::syn_rcvd);
    return send_store_packet(m_state.derive_flags());
}

bool connection::active_open(const sockaddr_in &listening_peer) noexcept {
    RUDP_ASSERT(addr_equals(m_peer, constants::UNINITIALISED_PEER),
                "A connection cannot cannot both initiate an open and have previously processed a "
                "packet from it's peer.");

    m_state.transition(state::kind::syn_sent);
    return send_store_packet(m_state.derive_flags(), listening_peer);
}

void connection::wait_for_established() noexcept {
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this]() { return m_state.current() == state::kind::established; });
}

const sockaddr_in &connection::peer() const noexcept {
    return m_peer;
}

// TODO: The entire packet setup is currently very inefficient and copies a lot of data.
bool connection::send_store_packet(u8 flags, std::optional<sockaddr_in> to) noexcept {
    RUDP_ASSERT(m_state.current() != state::kind::created,
                "A state transition must preceed any sending of packets.");
    RUDP_ASSERT(!m_sent.contains(m_seqnum),
                "A packet must not be sent twice through send_store_packet(flags).");

    packet packet(packet_header{
        .flags = flags,
        .seqnum = m_seqnum++,
        .acknum = m_acknum,
        .length = 1,
    });

    m_sent[packet.header.seqnum] = {
        .packet = packet,
        .sent_at = std::chrono::steady_clock::now(),
        .retransmits = 0,
    };

    const sockaddr_in &peer = (to.has_value()) ? to.value() : m_peer;
    return (packet::sendto(m_fd, packet, &peer) > 0);
}

void connection::assert_external_state(const char *caller) const noexcept {
    auto [err, event_loop] = internal::event_loop::instance();
    RUDP_ASSERT(err == internal::event_loop::result::error::none && event_loop != nullptr,
                "A connection must not exist unless an event loop was succesfully created.");

    event_loop->assert_initialised_state(caller);
    event_loop->assert_handler_exists(caller, handler_type::connection, m_fd);

    RUDP_ASSERT(is_valid_sockfd(m_fd), "A connection's underlying file descriptor must be valid.");
}

}  // namespace rudp::internal
