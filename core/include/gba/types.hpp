#pragma once

#include <cstdint>

namespace gba {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

enum class Button : u32 {
  A = 0,
  B = 1,
  Select = 2,
  Start = 3,
  Right = 4,
  Left = 5,
  Up = 6,
  Down = 7,
  R = 8,
  L = 9,
};

constexpr u32 kScreenWidth = 240;
constexpr u32 kScreenHeight = 160;
constexpr u32 kFramebufferPixels = kScreenWidth * kScreenHeight;
constexpr u32 kCyclesPerFrame = 280896;

}  // namespace gba
