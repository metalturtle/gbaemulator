#pragma once

#include <array>

#include "gba/cartridge.hpp"
#include "gba/interrupts.hpp"
#include "gba/keypad.hpp"
#include "gba/ppu.hpp"
#include "gba/timers.hpp"
#include "gba/types.hpp"

namespace gba {

class Bus {
 public:
  Bus(Cartridge& cartridge, Ppu& ppu, Timers& timers, Interrupts& interrupts, Keypad& keypad);

  void reset();

  [[nodiscard]] u8 read8(u32 address);
  [[nodiscard]] u16 read16(u32 address);
  [[nodiscard]] u32 read32(u32 address);

  void write8(u32 address, u8 value);
  void write16(u32 address, u16 value);
  void write32(u32 address, u32 value);

 private:
  enum class Region {
    Bios,
    Ewram,
    Iwram,
    Io,
    Palette,
    Vram,
    Oam,
    Rom,
    Save,
    Unmapped,
  };

  [[nodiscard]] static Region region(u32 address);
  [[nodiscard]] u8 readIo8(u32 address);
  [[nodiscard]] u16 readIo16(u32 address);
  void writeIo8(u32 address, u8 value);
  void writeIo16(u32 address, u16 value);

  Cartridge& cartridge_;
  Ppu& ppu_;
  Timers& timers_;
  Interrupts& interrupts_;
  Keypad& keypad_;

  std::array<u8, 16 * 1024> bios_{};
  std::array<u8, 256 * 1024> ewram_{};
  std::array<u8, 32 * 1024> iwram_{};
  std::array<u8, 1024> palette_{};
  std::array<u8, 96 * 1024> vram_{};
  std::array<u8, 1024> oam_{};
};

}  // namespace gba
