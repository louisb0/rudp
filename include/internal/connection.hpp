#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "internal/common.hpp"
#include "internal/packet.hpp"
#include "internal/state.hpp"

namespace rudp::internal {

struct sent_packet {
    class packet packet;
    std::chrono::steady_clock::time_point sent_at;
    u8 retransmits;
};

struct received_packet {
    class packet packet;
    sockaddr_in peer;
};

class connection {
public:
    // TODO: Implement an actual circular buffer.  We could try that paging technique?
    // TODO: Encapsulate - e.g. send() only writes to the end.
    std::deque<u8> send_buffer;
    std::deque<u8> recv_buffer;

    connection(linuxfd_t fd) : m_fd(fd) {}

    void handle_events() noexcept;
    void retransmit() noexcept;
    void process_sends() noexcept;

    [[nodiscard]] bool passive_open(const sockaddr_in &peer, const packet &packet) noexcept;
    [[nodiscard]] bool active_open(const sockaddr_in &listening_peer) noexcept;

    void wait_for_established() noexcept;
    void wait_for_send_space() noexcept;
    void wait_for_recv_data() noexcept;

    void on_established(std::function<void()> callback) noexcept;
    [[nodiscard]] const sockaddr_in &peer() const noexcept;

    template <typename Func>
    auto synchronise(Func &&func) {
        std::lock_guard<std::mutex> lock(m_mtx);
        return func();
    }

private:
    const linuxfd_t m_fd;

    state m_state{};
    u32 m_seqnum{};
    u32 m_acknum{};

    std::function<void()> m_listener_established{};

    // NOTE: connection::listener will spawn new connections on an ephemeral kernel port, meaning
    // that we do not know the port of our peer until we first receive a valid packet from them.
    sockaddr_in m_peer{constants::UNINITIALISED_PEER};

    // NOTE: We assume a single user thread, meaning that the user thread can only ever be waiting
    // on one condition at any given time, so we need only one condition variable.
    // TODO: The above does not imply only one mutex; suppose a user is placing data on the send
    // buffer while the event loop is trying to place data on the receive buffer.
    std::mutex m_mtx;
    std::condition_variable m_cv;

    std::unordered_map<u32, sent_packet> m_sent;
    std::map<u32, received_packet> m_received;

    void buffer_pending() noexcept;

    void handle_ack(const packet &packet) noexcept;
    void handle_synack(const packet &packet, const sockaddr_in &peer) noexcept;

    bool send_control_packet(u8 flags, std::optional<sockaddr_in> to = std::nullopt) noexcept;
    [[nodiscard]] bool send_packet(const packet &packet,
                                   std::optional<sockaddr_in> to = std::nullopt) noexcept;

    u32 get_sequence_advance(const packet &packet) noexcept;

    void assert_external_state(const char *caller) const noexcept;
};

extern std::map<std::chrono::steady_clock::time_point, std::unique_ptr<class connection>>
    g_time_wait_connections;

}  // namespace rudp::internal
