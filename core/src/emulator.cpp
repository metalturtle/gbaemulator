#include "gba/emulator.hpp"

namespace gba {

Emulator::Emulator() : bus_(cartridge_, ppu_, timers_, interrupts_, keypad_) {
  reset();
}

bool Emulator::loadRom(std::span<const u8> rom) {
  const bool ok = cartridge_.load(rom);
  if (ok) {
    reset();
  }
  return ok;
}

void Emulator::reset() {
  cartridge_.reset();
  ppu_.reset();
  timers_.reset();
  interrupts_.reset();
  keypad_.reset();
  scheduler_.reset();
  bus_.reset();
  cpu_.reset();
}

void Emulator::runFrame() {
  const u64 target = scheduler_.cycles() + kCyclesPerFrame;
  while (scheduler_.cycles() < target) {
    const u32 cycles = cpu_.step(bus_);
    scheduler_.advance(cycles);
    timers_.advance(cycles, interrupts_);
    ppu_.advance(cycles, interrupts_);
    if (interrupts_.pending()) {
      cpu_.requestIrq();
    }
  }
}

void Emulator::setButton(Button button, bool pressed) {
  keypad_.setButton(button, pressed);
}

}  // namespace gba
