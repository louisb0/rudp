#include "internal/connection.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cerrno>
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
    [[nodiscard]] static bool equals(const sockaddr_in &first, const sockaddr_in &second) {
        return (first.sin_addr.s_addr == second.sin_addr.s_addr) &&
               (first.sin_port == second.sin_port);
    }
}  // namespace

std::map<std::chrono::steady_clock::time_point, std::unique_ptr<connection>>
    g_time_wait_connections;

void connection::handle_events() noexcept {
    assert_external_state(__PRETTY_FUNCTION__);

    buffer_pending();

    bool received_data = false;
    while (!m_received.empty() && m_acknum == m_received.begin()->first) {
        auto it = m_received.begin();
        const auto &[packet, peer] = it->second;
        RUDP_ASSERT(packet.header.seqnum == m_received.begin()->first,
                    "A received packet in m_received must have it's sequence number as it's key.");

        if (packet.header.flags == (static_cast<u8>(flag::SYN) | static_cast<u8>(flag::ACK))) {
            handle_synack(packet, peer);
        }

        if (packet.header.flags == static_cast<u8>(flag::ACK)) {
            handle_ack(packet);
        }

        if (!packet.data().empty()) {
            std::lock_guard<std::mutex> lock(m_mtx);
            recv_buffer.insert(recv_buffer.end(), packet.data().begin(), packet.data().end());
            received_data = true;
        }

        m_acknum += get_sequence_advance(packet);
        m_received.erase(it);
    }

    u8 flags = m_state.derive_flags();
    flags |= static_cast<u8>(flag::ACK) & -static_cast<u8>(received_data != 0);

    if (flags != state::NO_FLAGS) {
        send_control_packet(flags);
    }

    if (received_data) {
        m_cv.notify_one();
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

        if (!equals(m_peer, constants::UNINITIALISED_PEER) && !equals(m_peer, peer_addr)) {
            continue;
        }

        packet packet = packet_opt.value();
        m_received.insert({packet.header.seqnum, {packet, peer_addr}});
    }
}

void connection::handle_synack(const packet &packet, const sockaddr_in &peer) noexcept {
    RUDP_ASSERT(packet.header.seqnum == m_acknum,
                "m_acknum must be in sync with the current packet being handled.");

    // NOTE: We expect a SYNACK in response to our SYN (active_open()).
    if (m_state.current() == state::kind::syn_sent) {
        RUDP_ASSERT(equals(m_peer, constants::UNINITIALISED_PEER),
                    "A connection cannot have processed a packet from it's peer prior to a "
                    "transition out of SYN_SENT.");
        RUDP_ASSERT(!m_listener_established,
                    "A callback must not be registered in the active open case.");

        // TODO: on_first_packet()
        m_peer = peer;
        m_state.transition(state::kind::established);
        m_cv.notify_one();
    }
}

void connection::handle_ack(const packet &packet) noexcept {
    RUDP_ASSERT(packet.header.seqnum == m_acknum,
                "m_acknum must be in sync with the current packet being handled.");
    RUDP_ASSERT(!equals(m_peer, constants::UNINITIALISED_PEER),
                "A connection must have processed a valid SYN(ACK) from our peer prior to "
                "processing an individual ACK.");

    while (!m_sent.empty() && m_sent.begin()->first < packet.header.acknum) {
        const auto &[sent_packet, _, __] = m_sent.begin()->second;
        RUDP_ASSERT(sent_packet.header.seqnum == m_sent.begin()->first,
                    "A sent packet in m_sent must have it's sequence number as it's key.");

        m_sent.erase(sent_packet.header.seqnum);
    }

    // NOTE: We expect an 'ACK' in response to our SYNACK (passive_open()).
    if (m_state.current() == state::kind::syn_rcvd) {
        RUDP_ASSERT(m_listener_established,
                    "A callback must be registered in the passive open case.");

        m_state.transition(state::kind::established);
        m_listener_established();
    }
}

bool connection::send_control_packet(u8 flags, std::optional<sockaddr_in> to) noexcept {
    packet packet(packet_header{
        .flags = flags,
        .seqnum = m_seqnum,
        .acknum = m_acknum,
        .length = 0,
    });

    if (flags & static_cast<u8>(flag::SYN) || flags & static_cast<u8>(flag::FIN)) {
        m_seqnum++;
    }

    return send_packet(packet, to);
}

bool connection::send_packet(const packet &packet, std::optional<sockaddr_in> to) noexcept {
    RUDP_ASSERT(m_state.current() != state::kind::created,
                "A state transition must preceed any sending of packets.");
    RUDP_ASSERT(!m_sent.contains(m_seqnum),
                "A packet must not be sent twice through send_store_packet().");

    // clang-format off
    bool needs_ack = 
        (packet.header.flags & static_cast<u8>(flag::SYN)) ||
        (packet.header.flags & static_cast<u8>(flag::FIN)) ||
        !packet.data().empty();
    // clang-format on 

    if (needs_ack) {
        m_sent[packet.header.seqnum] = {
            .packet = packet,
            .sent_at = std::chrono::steady_clock::now(),
            .retransmits = 0,
        };
    }

    const sockaddr_in &peer = (to.has_value()) ? to.value() : m_peer;
    return (packet::sendto(m_fd, packet, &peer) > 0);
}

u32 connection::get_sequence_advance(const packet& packet) noexcept {
    u32 advance = packet.header.flags & static_cast<u8>(flag::SYN);
    advance += packet.header.length;

    return advance;
}

RUDP_STATIC_ASSERT(
    constants::MAX_DATA_BYTES <= std::numeric_limits<u16>::max(),
    "proccess_send()'s cast from size_t to u16 assumes that constants::MAX_DATA_BYTES is u16.");
void connection::process_sends() noexcept {
    if (m_state.current() != state::kind::established) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mtx);

    while (!send_buffer.empty()) {
        const u16 to_send = static_cast<u16>(
            std::min(static_cast<size_t>(constants::MAX_DATA_BYTES), send_buffer.size()));

        packet packet(packet_header{
            .seqnum = m_seqnum,
            .acknum = m_acknum,
            .length = to_send,
        });

        auto it = send_buffer.begin();
        for (size_t i = 0; i < to_send; i++) {
            packet.push_data(*it);
            ++it;
        }

        if (!send_packet(packet)) {
            if (errno == ECONNRESET) {
                RUDP_ASSERT(false, "Connection reset; you must decide how to handle this.");
            }

            RUDP_ASSERT(
                errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ENOMEM,
                "sendto() can only fail due to non-blocking or the environment, but we got %s.",
                strerror(errno));
            break;
        }

        send_buffer.erase(send_buffer.begin(), send_buffer.begin() + to_send);
        m_seqnum += to_send;
    }
}

void connection::retransmit() noexcept {
    for (auto &[_, sent_packet] : m_sent) {
        if (sent_packet.retransmits == constants::MAX_RETRANSMITS) {
            RUDP_ASSERT(false, "Max retransmits reached; you must decide how to handle this.");
        }

        auto now = std::chrono::steady_clock::now();
        if (now - sent_packet.sent_at > constants::RETRANSMIT_TIME) {
            sent_packet.retransmits++;
            sent_packet.sent_at = now;

            packet::sendto(m_fd, sent_packet.packet, &m_peer);
        }
    }
}

bool connection::passive_open(const sockaddr_in &peer, const packet &packet) noexcept {
    RUDP_ASSERT(equals(m_peer, constants::UNINITIALISED_PEER),
                "A connection cannot cannot both respond to an intial open and have previously "
                "processed a packet from it's peer.");

    m_acknum = packet.header.seqnum + 1;
    m_peer = peer;
    m_state.transition(state::kind::syn_rcvd);
    return send_control_packet(m_state.derive_flags());
}

bool connection::active_open(const sockaddr_in &listening_peer) noexcept {
    RUDP_ASSERT(equals(m_peer, constants::UNINITIALISED_PEER),
                "A connection cannot cannot both initiate an open and have previously processed a "
                "packet from it's peer.");

    m_state.transition(state::kind::syn_sent);
    return send_control_packet(m_state.derive_flags(), listening_peer);
}

void connection::wait_for_established() noexcept {
    assert_external_state(__PRETTY_FUNCTION__);

    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this]() { return m_state.current() == state::kind::established; });
}

void connection::wait_for_send_space() noexcept {
    assert_external_state(__PRETTY_FUNCTION__);
    RUDP_ASSERT(m_state.current() == state::kind::established,
                "A connection must be established before the user thread can send data.");

    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this]() {
        RUDP_ASSERT(send_buffer.size() <= constants::MAX_DATA_BYTES,
                    "A connection's send buffer should never exceed it's cap.");

        return send_buffer.size() < constants::MAX_DATA_BYTES;
    });
}

void connection::wait_for_recv_data() noexcept {
    assert_external_state(__PRETTY_FUNCTION__);
    RUDP_ASSERT(m_state.current() == state::kind::established,
                "A connection must be established before the user thread can receive data. %d",
                static_cast<int>(m_state.current()));

    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this]() { return !recv_buffer.empty(); });
}

void connection::on_established(std::function<void()> callback) noexcept {
    m_listener_established = std::move(callback);
}

const sockaddr_in &connection::peer() const noexcept {
    return m_peer;
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
