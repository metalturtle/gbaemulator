#pragma once

#include <array>
#include <span>

#include "gba/interrupts.hpp"
#include "gba/types.hpp"

namespace gba {

class Ppu {
 public:
  void reset();
  void advance(u32 cycles, Interrupts& interrupts);

  [[nodiscard]] u16 dispcnt() const { return dispcnt_; }
  void setDispcnt(u16 value) { dispcnt_ = value; }

  [[nodiscard]] u16 dispstat() const { return dispstat_; }
  void setDispstat(u16 value);

  [[nodiscard]] u16 vcount() const { return vcount_; }

  [[nodiscard]] std::span<const u32> framebuffer() const { return framebuffer_; }
  [[nodiscard]] u32* framebufferData() { return framebuffer_.data(); }

 private:
  void updateStatus();
  void beginNewFrame();

  std::array<u32, kFramebufferPixels> framebuffer_{};
  u32 line_cycles_ = 0;
  u16 vcount_ = 0;
  u16 dispcnt_ = 0;
  u16 dispstat_ = 0;
};

}  // namespace gba
