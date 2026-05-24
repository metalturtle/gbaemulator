#pragma once

#include <array>

#include "gba/interrupts.hpp"
#include "gba/types.hpp"

namespace gba {

class Timers {
 public:
  void reset();
  void advance(u32 cycles, Interrupts& interrupts);

  [[nodiscard]] u16 readCounter(int index) const;
  void writeReload(int index, u16 value);
  [[nodiscard]] u16 readControl(int index) const;
  void writeControl(int index, u16 value);

 private:
  struct Timer {
    u16 counter = 0;
    u16 reload = 0;
    u16 control = 0;
    u32 ticks = 0;
  };

  static u32 prescaler(u16 control);
  void overflow(int index, Interrupts& interrupts);

  std::array<Timer, 4> timers_{};
};

}  // namespace gba
