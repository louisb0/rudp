#include "internal/socket.hpp"

#include <sys/fcntl.h>
#include <sys/socket.h>

#include <unordered_map>

#include "internal/common.hpp"

namespace rudp::internal {

rudpfd_t next_fd = 0;
std::unordered_map<rudpfd_t, socket> g_sockets;

linuxfd_t create_raw_socket() {
    linuxfd_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        ::close(fd);
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(fd);
        return -1;
    }

    return fd;
}

}  // namespace rudp::internal
