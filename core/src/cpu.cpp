#include "gba/cpu.hpp"

#include "gba/bus.hpp"

namespace gba {

void Cpu::reset() {
  regs_ = {};
  regs_[15] = 0x08000000;
  cpsr_ = static_cast<u32>(CpuMode::System) | kIrqDisableBit;
  irq_line_ = false;
  instructions_executed_ = 0;
}

u32 Cpu::step(Bus& bus) {
  if (irq_line_ && (cpsr_ & kIrqDisableBit) == 0) {
    enterIrq();
    return 3;
  }

  const u32 cycles = thumb() ? stepThumb(bus) : stepArm(bus);
  ++instructions_executed_;
  return cycles;
}

void Cpu::enterIrq() {
  // Banked registers and SPSR are not implemented yet. This is enough to expose
  // the control-flow event for early integration tests.
  cpsr_ = static_cast<u32>(CpuMode::Irq) | kIrqDisableBit;
  regs_[14] = regs_[15] + 4;
  regs_[15] = 0x00000018;
  irq_line_ = false;
}

u32 Cpu::stepArm(Bus& bus) {
  const u32 pc = regs_[15];
  (void)bus.read32(pc);
  regs_[15] = pc + 4;
  return 1;
}

u32 Cpu::stepThumb(Bus& bus) {
  const u32 pc = regs_[15];
  (void)bus.read16(pc);
  regs_[15] = pc + 2;
  return 1;
}

}  // namespace gba
