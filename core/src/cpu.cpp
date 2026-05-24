#include "gba/cpu.hpp"

#include "gba/bus.hpp"

namespace gba {

namespace {

u32 readRegisterForOperand(const std::array<u32, 16>& regs, int index, u32 instruction_pc) {
  if (index == 15) {
    return instruction_pc + 8;
  }
  return regs.at(index);
}

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
  unimplemented_instructions_ = 0;
  last_pc_ = 0;
  last_instruction_ = 0;
  last_swi_ = 0xffffffffu;
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
  last_pc_ = pc;
  last_instruction_ = instruction;
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

  if ((instruction & 0x0ffffff0u) == 0x012fff10u) {
    const int rm = static_cast<int>(instruction & 0x0f);
    const u32 target = regs_.at(rm);
    if ((target & 1u) != 0) {
      cpsr_ |= kThumbBit;
    } else {
      cpsr_ &= ~kThumbBit;
    }
    regs_[15] = target & ~1u;
    return 3;
  }

  if ((instruction & 0x0fbffff0u) == 0x0129f000u) {
    const int rm = static_cast<int>(instruction & 0x0f);
    cpsr_ = (cpsr_ & ~0xffu) | (regs_.at(rm) & 0xffu);
    return 1;
  }

  if ((instruction & 0x0c000000u) == 0x04000000u) {
    const bool immediate_offset = (instruction & (1u << 25)) == 0;
    const bool pre_index = (instruction & (1u << 24)) != 0;
    const bool add_offset = (instruction & (1u << 23)) != 0;
    const bool byte_transfer = (instruction & (1u << 22)) != 0;
    const bool write_back = (instruction & (1u << 21)) != 0;
    const bool load = (instruction & (1u << 20)) != 0;
    const int rn = static_cast<int>((instruction >> 16) & 0x0f);
    const int rd = static_cast<int>((instruction >> 12) & 0x0f);

    if (!immediate_offset || byte_transfer) {
      return 1;
    }

    const u32 base = readRegisterForOperand(regs_, rn, pc);
    const u32 offset = instruction & 0x0fffu;
    const u32 effective = pre_index ? (add_offset ? base + offset : base - offset) : base;

    if (load) {
      regs_.at(rd) = bus.read32(effective);
    } else {
      const u32 value = readRegisterForOperand(regs_, rd, pc);
      bus.write32(effective, value);
    }

    if (write_back || !pre_index) {
      regs_.at(rn) = add_offset ? base + offset : base - offset;
    }
    return 3;
  }

  if ((instruction & 0x0c000000u) == 0x00000000u) {
    const bool immediate = (instruction & (1u << 25)) != 0;
    const u32 opcode = (instruction >> 21) & 0x0f;
    const int rn = static_cast<int>((instruction >> 16) & 0x0f);
    const int rd = static_cast<int>((instruction >> 12) & 0x0f);

    if (immediate) {
      const u32 rotate = ((instruction >> 8) & 0x0f) * 2;
      const u32 imm8 = instruction & 0xffu;
      const u32 operand2 = rotate == 0 ? imm8 : ((imm8 >> rotate) | (imm8 << (32 - rotate)));

      if (opcode == 0x4) {
        regs_.at(rd) = readRegisterForOperand(regs_, rn, pc) + operand2;
        return 1;
      }
      if (opcode == 0xd) {
        regs_.at(rd) = operand2;
        return 1;
      }
    } else {
      const int rm = static_cast<int>(instruction & 0x0f);
      const bool no_shift = (instruction & 0x00000ff0u) == 0;
      if (opcode == 0xd && no_shift) {
        regs_.at(rd) = readRegisterForOperand(regs_, rm, pc);
        return 1;
      }
    }
  }

  ++unimplemented_instructions_;
  return 1;
}

u32 Cpu::stepThumb(Bus& bus) {
  const u32 pc = regs_[15];
  const u16 instruction = bus.read16(pc);
  last_pc_ = pc;
  last_instruction_ = instruction;
  regs_[15] = pc + 2;

  if ((instruction & 0xfe00u) == 0xb400u) {
    const bool include_lr = (instruction & (1u << 8)) != 0;
    const u16 list = instruction & 0xffu;
    u32 count = include_lr ? 1 : 0;
    for (int reg = 0; reg < 8; ++reg) {
      if ((list & (1u << reg)) != 0) {
        ++count;
      }
    }

    u32 address = regs_[13] - count * 4;
    regs_[13] = address;
    for (int reg = 0; reg < 8; ++reg) {
      if ((list & (1u << reg)) != 0) {
        bus.write32(address, regs_[reg]);
        address += 4;
      }
    }
    if (include_lr) {
      bus.write32(address, regs_[14]);
    }
    return 1 + count;
  }

  if ((instruction & 0xf800u) == 0x0000u) {
    const u32 offset = (instruction >> 6) & 0x1f;
    const int rs = static_cast<int>((instruction >> 3) & 0x07);
    const int rd = static_cast<int>(instruction & 0x07);
    regs_[rd] = regs_[rs] << offset;
    return 1;
  }

  if ((instruction & 0xf800u) == 0x1800u) {
    const bool subtract = (instruction & (1u << 9)) != 0;
    const bool immediate = (instruction & (1u << 10)) != 0;
    const u32 operand = immediate ? ((instruction >> 6) & 0x07) : regs_[(instruction >> 6) & 0x07];
    const int rs = static_cast<int>((instruction >> 3) & 0x07);
    const int rd = static_cast<int>(instruction & 0x07);
    regs_[rd] = subtract ? regs_[rs] - operand : regs_[rs] + operand;
    return 1;
  }

  if ((instruction & 0xf800u) == 0x2000u) {
    const int rd = static_cast<int>((instruction >> 8) & 0x07);
    regs_[rd] = instruction & 0xffu;
    return 1;
  }

  if ((instruction & 0xf800u) == 0x4800u) {
    const int rd = static_cast<int>((instruction >> 8) & 0x07);
    const u32 offset = (instruction & 0xffu) << 2;
    regs_[rd] = bus.read32(((pc + 4) & ~3u) + offset);
    return 3;
  }

  if ((instruction & 0xe000u) == 0x6000u) {
    const bool byte = (instruction & (1u << 12)) != 0;
    const bool load = (instruction & (1u << 11)) != 0;
    const int rb = static_cast<int>((instruction >> 3) & 0x07);
    const int rd = static_cast<int>(instruction & 0x07);
    const u32 scale = byte ? 1u : 4u;
    const u32 address = regs_[rb] + (((instruction >> 6) & 0x1fu) * scale);
    if (load) {
      regs_[rd] = byte ? bus.read8(address) : bus.read32(address);
    } else if (byte) {
      bus.write8(address, static_cast<u8>(regs_[rd]));
    } else {
      bus.write32(address, regs_[rd]);
    }
    return 2;
  }

  if ((instruction & 0xfc00u) == 0x4400u) {
    const u32 op = (instruction >> 8) & 0x03;
    const int source = static_cast<int>(((instruction >> 3) & 0x07) | ((instruction & 0x0040u) != 0 ? 8 : 0));
    const int dest = static_cast<int>((instruction & 0x07) | ((instruction & 0x0080u) != 0 ? 8 : 0));
    if (op == 0x2) {
      regs_[dest] = readRegisterForOperand(regs_, source, pc);
      if (dest == 15) {
        regs_[15] &= ~1u;
      }
      return 1;
    }
    if (op == 0x3) {
      const u32 target = regs_[source];
      if ((target & 1u) != 0) {
        cpsr_ |= kThumbBit;
      } else {
        cpsr_ &= ~kThumbBit;
      }
      regs_[15] = target & ~1u;
      return 3;
    }
  }

  if ((instruction & 0xff00u) == 0xdf00u) {
    last_swi_ = instruction & 0xffu;
    return 3;
  }

  if ((instruction & 0xf800u) == 0xf000u) {
    u32 offset = instruction & 0x07ffu;
    if ((offset & 0x0400u) != 0) {
      offset |= 0xfffff800u;
    }
    regs_[14] = (pc + 4) + (offset << 12);
    return 1;
  }

  if ((instruction & 0xf800u) == 0xf800u) {
    const u32 target = regs_[14] + ((instruction & 0x07ffu) << 1);
    regs_[14] = (pc + 2) | 1u;
    regs_[15] = target;
    return 3;
  }

  if ((instruction & 0xf200u) == 0x8000u) {
    const bool load = (instruction & (1u << 11)) != 0;
    const int rb = static_cast<int>((instruction >> 3) & 0x07);
    const int rd = static_cast<int>(instruction & 0x07);
    const u32 address = regs_[rb] + (((instruction >> 6) & 0x1fu) << 1);
    if (load) {
      regs_[rd] = bus.read16(address);
    } else {
      bus.write16(address, static_cast<u16>(regs_[rd]));
    }
    return 2;
  }

  ++unimplemented_instructions_;
  return 1;
}

}  // namespace gba
