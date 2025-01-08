#include "internal/event_loop.hpp"

#include <sys/epoll.h>

#include <mutex>
#include <thread>

#include "internal/common.hpp"

namespace rudp::internal {

linuxfd_t epollfd{-1};
bool running{false};

void event_loop() {}

void ensure_event_loop() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        epollfd = epoll_create1(0);

        std::thread(event_loop).detach();
    });
}

}  // namespace rudp::internal
