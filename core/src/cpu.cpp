#include "gba/cpu.hpp"

#include "gba/bus.hpp"

namespace gba {

namespace {

bool conditionPassed(u32 instruction, u32 cpsr) {
  const u32 condition = instruction >> 28;
  const bool n = (cpsr & (1u << 31)) != 0;
  const bool z = (cpsr & (1u << 30)) != 0;
  const bool c = (cpsr & (1u << 29)) != 0;
  const bool v = (cpsr & (1u << 28)) != 0;

  switch (condition) {
    case 0x0:
      return z;
    case 0x1:
      return !z;
    case 0x2:
      return c;
    case 0x3:
      return !c;
    case 0x4:
      return n;
    case 0x5:
      return !n;
    case 0x6:
      return v;
    case 0x7:
      return !v;
    case 0x8:
      return c && !z;
    case 0x9:
      return !c || z;
    case 0xa:
      return n == v;
    case 0xb:
      return n != v;
    case 0xc:
      return !z && n == v;
    case 0xd:
      return z || n != v;
    case 0xe:
      return true;
    default:
      return false;
  }
}

u32 signExtendBranchOffset(u32 instruction) {
  u32 offset = (instruction & 0x00ffffffu) << 2;
  if ((offset & 0x02000000u) != 0) {
    offset |= 0xfc000000u;
  }
  return offset;
}

}  // namespace

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
  const u32 instruction = bus.read32(pc);
  regs_[15] = pc + 4;

  if (!conditionPassed(instruction, cpsr_)) {
    return 1;
  }

  if ((instruction & 0x0e000000u) == 0x0a000000u) {
    const bool link = (instruction & (1u << 24)) != 0;
    if (link) {
      regs_[14] = pc + 4;
    }
    regs_[15] = pc + 8 + signExtendBranchOffset(instruction);
    return 3;
  }

  return 1;
}

u32 Cpu::stepThumb(Bus& bus) {
  const u32 pc = regs_[15];
  (void)bus.read16(pc);
  regs_[15] = pc + 2;
  return 1;
}

}  // namespace gba
