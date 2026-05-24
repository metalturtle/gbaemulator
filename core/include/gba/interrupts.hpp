#pragma once

#include "gba/types.hpp"

namespace gba {

class Interrupts {
 public:
  enum Flag : u16 {
    VBlank = 1u << 0,
    HBlank = 1u << 1,
    VCounter = 1u << 2,
    Timer0 = 1u << 3,
    Timer1 = 1u << 4,
    Timer2 = 1u << 5,
    Timer3 = 1u << 6,
    Serial = 1u << 7,
    Dma0 = 1u << 8,
    Dma1 = 1u << 9,
    Dma2 = 1u << 10,
    Dma3 = 1u << 11,
    Keypad = 1u << 12,
    GamePak = 1u << 13,
  };

  void reset();
  void request(Flag flag);
  [[nodiscard]] bool pending() const;

  [[nodiscard]] u16 ie() const { return ie_; }
  [[nodiscard]] u16 flag() const { return if_; }
  [[nodiscard]] u16 ime() const { return ime_; }

  void setIe(u16 value) { ie_ = value & 0x3fff; }
  void acknowledge(u16 value) { if_ &= static_cast<u16>(~value); }
  void setIme(u16 value) { ime_ = value & 1; }

 private:
  u16 ie_ = 0;
  u16 if_ = 0;
  u16 ime_ = 0;
};

}  // namespace gba
