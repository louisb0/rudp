#include "internal/socket.hpp"

#include <unordered_map>

#include "internal/common.hpp"

namespace rudp::internal {

rudpfd_t next_fd = 0;
std::unordered_map<rudpfd_t, socket> g_sockets;

}  // namespace rudp::internal
