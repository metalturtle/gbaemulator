#pragma once

#include <array>

#include "gba/types.hpp"

namespace gba {

class Bus;

enum class CpuMode : u32 {
  User = 0x10,
  Fiq = 0x11,
  Irq = 0x12,
  Supervisor = 0x13,
  Abort = 0x17,
  Undefined = 0x1b,
  System = 0x1f,
};

class Cpu {
 public:
  void reset();
  u32 step(Bus& bus);
  void requestIrq() { irq_line_ = true; }

  [[nodiscard]] u32 reg(int index) const { return regs_.at(index); }
  [[nodiscard]] u32 cpsr() const { return cpsr_; }
  [[nodiscard]] bool thumb() const { return (cpsr_ & kThumbBit) != 0; }
  [[nodiscard]] CpuMode mode() const { return static_cast<CpuMode>(cpsr_ & 0x1f); }
  [[nodiscard]] u64 instructionsExecuted() const { return instructions_executed_; }

 private:
  static constexpr u32 kThumbBit = 1u << 5;
  static constexpr u32 kIrqDisableBit = 1u << 7;

  void enterIrq();
  u32 stepArm(Bus& bus);
  u32 stepThumb(Bus& bus);

  std::array<u32, 16> regs_{};
  u32 cpsr_ = 0;
  bool irq_line_ = false;
  u64 instructions_executed_ = 0;
};

}  // namespace gba
