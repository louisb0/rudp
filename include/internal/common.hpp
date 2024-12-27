#pragma once

#include <assert.h>

#include <cstdint>

#define STRINGIFY(x) #x
#define UNREACHABLE() assert(false && "Unreachable at " __FILE__ ":" STRINGIFY(__LINE__))

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using f32 = float;
using f64 = double;

using b8 = bool;

using linuxfd_t = s32;
using rudpfd_t = s32;
