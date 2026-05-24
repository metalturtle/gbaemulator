#include "gba/ppu.hpp"

#include <algorithm>

namespace gba {

namespace {

constexpr u32 kCyclesPerScanline = 1232;
constexpr u16 kVisibleLines = 160;
constexpr u16 kTotalLines = 228;

}  // namespace

void Ppu::reset() {
  line_cycles_ = 0;
  vcount_ = 0;
  dispcnt_ = 0;
  dispstat_ = 0;
  std::fill(framebuffer_.begin(), framebuffer_.end(), 0xff000000u);
  updateStatus();
}

void Ppu::advance(u32 cycles, Interrupts& interrupts) {
  line_cycles_ += cycles;
  while (line_cycles_ >= kCyclesPerScanline) {
    line_cycles_ -= kCyclesPerScanline;
    ++vcount_;

    if (vcount_ == kVisibleLines) {
      interrupts.request(Interrupts::VBlank);
    }

    if (vcount_ >= kTotalLines) {
      beginNewFrame();
    }

    updateStatus();
  }
}

void Ppu::setDispstat(u16 value) {
  // Status bits 0-2 are read-only; keep interrupt enables and VCOUNT compare bits.
  dispstat_ = static_cast<u16>((dispstat_ & 0x0007) | (value & 0xfff8));
}

void Ppu::updateStatus() {
  dispstat_ &= static_cast<u16>(~0x0007u);
  if (vcount_ >= kVisibleLines) {
    dispstat_ |= 0x0001;  // VBlank
  }
  if (line_cycles_ >= 1006) {
    dispstat_ |= 0x0002;  // Approximate HBlank window.
  }
  const u16 vcompare = static_cast<u16>((dispstat_ >> 8) & 0xff);
  if (vcount_ == vcompare) {
    dispstat_ |= 0x0004;
  }
}

void Ppu::beginNewFrame() {
  vcount_ = 0;
}

}  // namespace gba
