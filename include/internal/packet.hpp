#pragma once

#include <optional>
#include <vector>

#include "internal/common.hpp"

namespace rudp::internal {

enum class flag : u8 {
    SYN = 1 << 0,
    ACK = 1 << 1,
};

struct packet_header {
    u16 magic{0x1234};  // NOTE: For detection in tools like Wireshark.
    u8 version{1};
    u8 flags{};
    u32 seqnum{};
    u32 acknum{};
    u32 length{};
};

// TODO: Look at something more efficient with m_data and serialise().
class packet {
public:
    packet_header header;

    packet() = default;
    explicit packet(packet_header h) : header(h) {};

    static ssize_t sendto(linuxfd_t fd, const packet &packet, const sockaddr_in *addr);
    static std::optional<packet> recvfrom(linuxfd_t fd, sockaddr_in *addr);

    [[nodiscard]] const std::vector<u8> &data() const noexcept;
    void push_data(u8 byte) noexcept;

private:
    std::vector<u8> m_data;

    [[nodiscard]] static std::optional<packet> deserialise(const std::vector<u8> &data);
    [[nodiscard]] std::vector<u8> serialise() const noexcept;
};

}  // namespace rudp::internal
