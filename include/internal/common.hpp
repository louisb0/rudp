#pragma once

#include <cassert>
#include <cstdint>

#define RUDP_ASSERT(cond, msg) assert((cond) && __FILE__ ":" RUDP_STRINGIFY(__LINE__) ": " msg)
#define RUDP_UNREACHABLE() RUDP_ASSERT(false, "Unreachable");
#define RUDP_STRINGIFY(x) RUDP_STRINGIFY2(x)
#define RUDP_STRINGIFY2(x) #x

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

using f32 = float;
using f64 = double;

using b8 = bool;

using linuxfd_t = s32;
using rudpfd_t = s32;
