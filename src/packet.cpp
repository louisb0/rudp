#include "packet.hpp"

#include "socket.hpp"

namespace rudp::internal {

packet create_syn(const socket* const sock) noexcept {
    packet pkt;
    pkt.seq_num = sock->seq_num;
    pkt.ack_num = 0;
    pkt.flags = SYN;
    return pkt;
}

packet create_synack(const socket* const sock) noexcept {
    packet pkt;
    pkt.seq_num = sock->seq_num;
    pkt.ack_num = sock->peer.seq_num + 1;
    pkt.flags = SYN | ACK;
    return pkt;
}

packet create_ack(const socket* const sock) noexcept {
    packet pkt;
    pkt.seq_num = sock->seq_num;
    pkt.ack_num = sock->peer.seq_num + 1;
    pkt.flags = ACK;
    return pkt;
}

}  // namespace rudp::internal
