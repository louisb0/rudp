#pragma once

#include "internal/common.hpp"

namespace rudp::internal {

extern linuxfd_t epollfd;

void event_loop() noexcept;
[[nodiscard]] bool ensure_event_loop() noexcept;

}  // namespace rudp::internal
