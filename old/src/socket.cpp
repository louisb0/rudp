#include "socket.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <mutex>
#include <unordered_map>

#include "common.hpp"
#include "session.hpp"

namespace rudp::internal {

static rudpfd_t s_next_fd = 0;
static std::unordered_map<rudpfd_t, socket> g_sockets;

std::mutex g_sockets_mtx;

rudpfd_t socket_create() noexcept {
    std::lock_guard<std::mutex> lock(g_sockets_mtx);

    linuxfd_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    int optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        close(fd);
        return -1;
    }

    rudpfd_t new_fd = s_next_fd++;
    socket& sock = g_sockets[new_fd];
    sock.fd = fd;
    sock.state = socket::CLOSED;
    sock.peer.addr_len = sizeof(sock.peer.addr);

    // TODO: Consider edge triggered epoll with non-blocking sockets.
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.u32 = static_cast<u32>(new_fd);

    if (epoll_ctl(session::instance().epollfd(), EPOLL_CTL_ADD, fd, &ev) < 0) {
        close(fd);
        return -1;
    }

    session::instance().unref();
    return new_fd;
}

socket* socket_get(rudpfd_t fd) noexcept {
    auto it = g_sockets.find(fd);
    if (it == g_sockets.end()) {
        return nullptr;
    }
    return &it->second;
}

void socket_delete(rudpfd_t fd) noexcept {
    std::lock_guard<std::mutex> lock(g_sockets_mtx);
    g_sockets.erase(fd);
    session::instance().unref();
}

}  // namespace rudp::internal
