#pragma once

#include "internal/common.hpp"

namespace rudp::internal {

extern linuxfd_t epollfd;
extern bool running;

void event_loop();
void ensure_event_loop();

}  // namespace rudp::internal
