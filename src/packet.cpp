#include "internal/packet.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstring>
#include <optional>
#include <variant>
#include <vector>

#include "internal/assert.hpp"
#include "internal/common.hpp"
#include "internal/simulator.hpp"

namespace rudp::internal {

RUDP_STATIC_ASSERT(sizeof(packet_header) == 16,
                   "Don't forget to update the serialisation functions :)");
RUDP_STATIC_ASSERT(offsetof(packet_header, magic) == 0);
RUDP_STATIC_ASSERT(offsetof(packet_header, version) == 2);
RUDP_STATIC_ASSERT(offsetof(packet_header, flags) == 3);
RUDP_STATIC_ASSERT(offsetof(packet_header, seqnum) == 4);
RUDP_STATIC_ASSERT(offsetof(packet_header, acknum) == 8);
RUDP_STATIC_ASSERT(offsetof(packet_header, length) == 12);

std::vector<u8> packet::serialise() const noexcept {
    std::vector<u8> result;
    result.reserve(sizeof(packet_header) + m_data.size());

    u16 net_magic = htons(header.magic);
    u32 net_seqnum = htonl(header.seqnum);
    u32 net_acknum = htonl(header.acknum);
    u32 net_length = htonl(header.length);

    // Header.
    const u8 *magic = reinterpret_cast<const u8 *>(&net_magic);
    result.insert(result.end(), magic, magic + sizeof(net_magic));

    result.push_back(header.version);
    result.push_back(header.flags);

    const u8 *seqnum = reinterpret_cast<const u8 *>(&net_seqnum);
    result.insert(result.end(), seqnum, seqnum + sizeof(net_seqnum));

    const u8 *acknum = reinterpret_cast<const u8 *>(&net_acknum);
    result.insert(result.end(), acknum, acknum + sizeof(net_acknum));

    const u8 *length = reinterpret_cast<const u8 *>(&net_length);
    result.insert(result.end(), length, length + sizeof(net_length));

    // Data.
    result.insert(result.end(), m_data.begin(), m_data.end());

    return result;
}

std::optional<packet> packet::deserialise(const std::vector<u8> &data) {
    if (data.size() < sizeof(packet_header)) {
        return std::nullopt;
    }

    size_t i = 0;
    packet_header header;

    // Header.
    u16 net_magic{};
    std::memcpy(&net_magic, &data[i], sizeof(net_magic));
    header.magic = ntohs(net_magic);
    i += 2;

    if (header.magic != packet_header{}.magic) {
        return std::nullopt;
    }

    header.version = data[i++];
    header.flags = data[i++];

    u32 net_seqnum{};
    std::memcpy(&net_seqnum, &data[i], sizeof(net_seqnum));
    header.seqnum = ntohl(net_seqnum);
    i += 4;

    u32 net_acknum{};
    std::memcpy(&net_acknum, &data[i], sizeof(net_acknum));
    header.acknum = ntohl(net_acknum);
    i += 4;

    u32 net_length{};
    std::memcpy(&net_length, &data[i], sizeof(net_length));
    header.length = ntohl(net_length);
    i += 4;

    packet packet(header);

    // Data.
    if (header.length > 0) {
        if (i + header.length > data.size()) {
            return std::nullopt;
        }

        size_t copy = std::min(static_cast<size_t>(header.length),
                               static_cast<size_t>(constants::MAX_DATA_BYTES));

        packet.m_data.assign(
            data.begin() + static_cast<std::vector<u8>::difference_type>(i),
            data.begin() + static_cast<std::vector<u8>::difference_type>(i + copy));
    }

    return packet;
}

const std::vector<u8> &packet::data() const noexcept {
    return m_data;
}

void packet::push_data(u8 byte) noexcept {
    m_data.push_back(byte);
}

ssize_t packet::sendto(linuxfd_t fd, const packet &packet, const sockaddr_in *addr) {
    std::vector<u8> serialised = packet.serialise();

    return simulator::sendto(fd, serialised.data(), serialised.size(), 0,
                             reinterpret_cast<const sockaddr *>(addr), sizeof(*addr));
}

std::optional<packet> packet::recvfrom(linuxfd_t fd, sockaddr_in *addr) {
    std::vector<u8> buffer(sizeof(packet_header) + constants::MAX_DATA_BYTES);

    socklen_t addrlen = sizeof(sockaddr_in);
    ssize_t bytes = ::recvfrom(fd, buffer.data(), buffer.size(), 0,
                               reinterpret_cast<sockaddr *>(addr), &addrlen);
    if (bytes <= 0) {
        return std::nullopt;
    }

    buffer.resize(static_cast<size_t>(bytes));
    return packet::deserialise(buffer);
}

}  // namespace rudp::internal
