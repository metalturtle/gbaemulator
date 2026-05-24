#pragma once

#include <span>

#include "gba/bus.hpp"
#include "gba/cartridge.hpp"
#include "gba/cpu.hpp"
#include "gba/interrupts.hpp"
#include "gba/keypad.hpp"
#include "gba/ppu.hpp"
#include "gba/scheduler.hpp"
#include "gba/timers.hpp"
#include "gba/types.hpp"

namespace gba {

class Emulator {
 public:
  Emulator();

  bool loadRom(std::span<const u8> rom);
  void reset();
  void runFrame();
  void setButton(Button button, bool pressed);

  [[nodiscard]] u32* framebuffer() { return ppu_.framebufferData(); }
  [[nodiscard]] std::span<const u8> exportSave() const { return cartridge_.save(); }
  bool importSave(std::span<const u8> save) { return cartridge_.importSave(save); }

  [[nodiscard]] const Cartridge& cartridge() const { return cartridge_; }
  [[nodiscard]] const Cpu& cpu() const { return cpu_; }
  [[nodiscard]] Cpu& cpu() { return cpu_; }
  [[nodiscard]] const Ppu& ppu() const { return ppu_; }
  [[nodiscard]] const Interrupts& interrupts() const { return interrupts_; }
  [[nodiscard]] const Keypad& keypad() const { return keypad_; }
  [[nodiscard]] const Scheduler& scheduler() const { return scheduler_; }
  [[nodiscard]] Bus& bus() { return bus_; }

 private:
  Cartridge cartridge_;
  Ppu ppu_;
  Timers timers_;
  Interrupts interrupts_;
  Keypad keypad_;
  Scheduler scheduler_;
  Cpu cpu_;
  Bus bus_;
};

}  // namespace gba
