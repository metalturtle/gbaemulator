#include "gba/timers.hpp"

namespace gba {

void Timers::reset() {
  timers_ = {};
}

void Timers::advance(u32 cycles, Interrupts& interrupts) {
  for (int index = 0; index < 4; ++index) {
    auto& timer = timers_[index];
    const bool enabled = (timer.control & 0x0080) != 0;
    const bool cascade = (timer.control & 0x0004) != 0;
    if (!enabled || cascade) {
      continue;
    }

    timer.ticks += cycles;
    const u32 divider = prescaler(timer.control);
    while (timer.ticks >= divider) {
      timer.ticks -= divider;
      ++timer.counter;
      if (timer.counter == 0) {
        overflow(index, interrupts);
      }
    }
  }
}

u16 Timers::readCounter(int index) const {
  return timers_.at(index).counter;
}

void Timers::writeReload(int index, u16 value) {
  timers_.at(index).reload = value;
}

u16 Timers::readControl(int index) const {
  return timers_.at(index).control;
}

void Timers::writeControl(int index, u16 value) {
  auto& timer = timers_.at(index);
  const bool was_enabled = (timer.control & 0x0080) != 0;
  const bool now_enabled = (value & 0x0080) != 0;
  timer.control = static_cast<u16>(value & 0x00c7);
  if (!was_enabled && now_enabled) {
    timer.counter = timer.reload;
    timer.ticks = 0;
  }
}

u32 Timers::prescaler(u16 control) {
  switch (control & 0x0003) {
    case 0:
      return 1;
    case 1:
      return 64;
    case 2:
      return 256;
    default:
      return 1024;
  }
}

void Timers::overflow(int index, Interrupts& interrupts) {
  auto& timer = timers_.at(index);
  timer.counter = timer.reload;

  if ((timer.control & 0x0040) != 0) {
    interrupts.request(static_cast<Interrupts::Flag>(Interrupts::Timer0 << index));
  }

  const int next = index + 1;
  if (next < 4) {
    auto& cascade = timers_[next];
    if ((cascade.control & 0x0080) != 0 && (cascade.control & 0x0004) != 0) {
      ++cascade.counter;
      if (cascade.counter == 0) {
        overflow(next, interrupts);
      }
    }
  }
}

}  // namespace gba
