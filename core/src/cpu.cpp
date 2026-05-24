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

void setNz(u32& cpsr, u32 value) {
  cpsr &= ~((1u << 31) | (1u << 30));
  if ((value & (1u << 31)) != 0) {
    cpsr |= 1u << 31;
  }
  if (value == 0) {
    cpsr |= 1u << 30;
  }
}

void setAddFlags(u32& cpsr, u32 lhs, u32 rhs, u32 result) {
  setNz(cpsr, result);
  cpsr &= ~((1u << 29) | (1u << 28));
  if (static_cast<u64>(lhs) + static_cast<u64>(rhs) > 0xffffffffull) {
    cpsr |= 1u << 29;
  }
  if (((~(lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) != 0) {
    cpsr |= 1u << 28;
  }
}

void setSubFlags(u32& cpsr, u32 lhs, u32 rhs, u32 result) {
  setNz(cpsr, result);
  cpsr &= ~((1u << 29) | (1u << 28));
  if (lhs >= rhs) {
    cpsr |= 1u << 29;
  }
  if ((((lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) != 0) {
    cpsr |= 1u << 28;
  }
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
    enterIrq(bus);
    return 3;
  }

  const u32 cycles = thumb() ? stepThumb(bus) : stepArm(bus);
  ++instructions_executed_;
  return cycles;
}

void Cpu::enterIrq(Bus& bus) {
  // Without a real BIOS, dispatch through the BIOS IRQ handler pointer that
  // commercial games install at 0x03007FFC.
  const u32 handler = bus.read32(0x03007ffcu);
  regs_[14] = regs_[15] + (thumb() ? 2 : 4);
  cpsr_ = static_cast<u32>(CpuMode::Irq) | kIrqDisableBit;
  if ((handler & 1u) != 0) {
    cpsr_ |= kThumbBit;
  }
  regs_[15] = handler == 0 ? 0x00000018u : (handler & ~1u);
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

  if ((instruction & 0x0fbf0fffu) == 0x010f0000u) {
    const int rd = static_cast<int>((instruction >> 12) & 0x0f);
    regs_[rd] = cpsr_;
    return 1;
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

    if (!immediate_offset) {
      return 1;
    }

    const u32 base = readRegisterForOperand(regs_, rn, pc);
    const u32 offset = instruction & 0x0fffu;
    const u32 effective = pre_index ? (add_offset ? base + offset : base - offset) : base;

    if (load) {
      regs_.at(rd) = byte_transfer ? bus.read8(effective) : bus.read32(effective);
    } else if (byte_transfer) {
      bus.write8(effective, static_cast<u8>(readRegisterForOperand(regs_, rd, pc)));
    } else {
      const u32 value = readRegisterForOperand(regs_, rd, pc);
      bus.write32(effective, value);
    }

    if (write_back || !pre_index) {
      regs_.at(rn) = add_offset ? base + offset : base - offset;
    }
    return 3;
  }

  if ((instruction & 0x0e000090u) == 0x00000090u) {
    const bool pre_index = (instruction & (1u << 24)) != 0;
    const bool add_offset = (instruction & (1u << 23)) != 0;
    const bool immediate_offset = (instruction & (1u << 22)) != 0;
    const bool write_back = (instruction & (1u << 21)) != 0;
    const bool load = (instruction & (1u << 20)) != 0;
    const int rn = static_cast<int>((instruction >> 16) & 0x0f);
    const int rd = static_cast<int>((instruction >> 12) & 0x0f);
    const bool signed_transfer = (instruction & (1u << 6)) != 0;
    const bool halfword_transfer = (instruction & (1u << 5)) != 0;

    if (!immediate_offset || signed_transfer || !halfword_transfer) {
      ++unimplemented_instructions_;
      return 1;
    }

    const u32 offset = ((instruction >> 4) & 0xf0u) | (instruction & 0x0fu);
    const u32 base = readRegisterForOperand(regs_, rn, pc);
    const u32 effective = pre_index ? (add_offset ? base + offset : base - offset) : base;

    if (load) {
      regs_[rd] = bus.read16(effective);
    } else {
      bus.write16(effective, static_cast<u16>(readRegisterForOperand(regs_, rd, pc)));
    }

    if (write_back || !pre_index) {
      regs_[rn] = add_offset ? base + offset : base - offset;
    }
    return 3;
  }

  if ((instruction & 0x0e000000u) == 0x08000000u) {
    const bool pre_index = (instruction & (1u << 24)) != 0;
    const bool add_offset = (instruction & (1u << 23)) != 0;
    const bool write_back = (instruction & (1u << 21)) != 0;
    const bool load = (instruction & (1u << 20)) != 0;
    const int rn = static_cast<int>((instruction >> 16) & 0x0f);
    const u32 list = instruction & 0xffffu;
    u32 count = 0;
    for (int reg = 0; reg < 16; ++reg) {
      if ((list & (1u << reg)) != 0) {
        ++count;
      }
    }
    if (count == 0) {
      count = 16;
    }

    const u32 base = readRegisterForOperand(regs_, rn, pc);
    u32 address = base;
    if (add_offset) {
      address = pre_index ? base + 4 : base;
    } else {
      address = pre_index ? base - count * 4 : base - count * 4 + 4;
    }

    for (int reg = 0; reg < 16; ++reg) {
      if ((list & (1u << reg)) == 0) {
        continue;
      }
      if (load) {
        regs_[reg] = bus.read32(address);
      } else {
        bus.write32(address, readRegisterForOperand(regs_, reg, pc));
      }
      address += 4;
    }

    if (write_back) {
      regs_[rn] = add_offset ? base + count * 4 : base - count * 4;
    }
    if (load && (list & (1u << 15)) != 0) {
      regs_[15] &= ~3u;
    }
    return 1 + count;
  }

  if ((instruction & 0x0c000000u) == 0x00000000u) {
    const bool immediate = (instruction & (1u << 25)) != 0;
    const u32 opcode = (instruction >> 21) & 0x0f;
    const bool set_flags = (instruction & (1u << 20)) != 0;
    const int rn = static_cast<int>((instruction >> 16) & 0x0f);
    const int rd = static_cast<int>((instruction >> 12) & 0x0f);
    u32 operand2 = 0;

    if (immediate) {
      const u32 rotate = ((instruction >> 8) & 0x0f) * 2;
      const u32 imm8 = instruction & 0xffu;
      operand2 = rotate == 0 ? imm8 : ((imm8 >> rotate) | (imm8 << (32 - rotate)));
    } else {
      const int rm = static_cast<int>(instruction & 0x0f);
      const bool register_shift = (instruction & (1u << 4)) != 0;

      const u32 value = readRegisterForOperand(regs_, rm, pc);
      const u32 shift = register_shift ? (regs_[(instruction >> 8) & 0x0f] & 0xffu) : ((instruction >> 7) & 0x1fu);
      const u32 shift_type = (instruction >> 5) & 0x3u;
      switch (shift_type) {
        case 0:
          operand2 = shift >= 32 ? 0 : value << shift;
          break;
        case 1:
          operand2 = shift == 0 ? value : (shift >= 32 ? 0 : value >> shift);
          break;
        case 2:
          operand2 = shift == 0 ? value : (shift >= 32 ? (value & 0x80000000u ? 0xffffffffu : 0) : static_cast<u32>(static_cast<int32_t>(value) >> shift));
          break;
        default:
          operand2 = shift == 0 ? value : ((value >> (shift & 31u)) | (value << ((32 - shift) & 31u)));
          break;
      }
    }

    const u32 lhs = readRegisterForOperand(regs_, rn, pc);
    u32 result = 0;
    switch (opcode) {
      case 0x0:
        result = lhs & operand2;
        regs_[rd] = result;
        if (set_flags) {
          setNz(cpsr_, result);
        }
        return 1;
      case 0x1:
        result = lhs ^ operand2;
        regs_[rd] = result;
        if (set_flags) {
          setNz(cpsr_, result);
        }
        return 1;
      case 0x2:
        result = lhs - operand2;
        regs_[rd] = result;
        if (set_flags) {
          setSubFlags(cpsr_, lhs, operand2, result);
        }
        return 1;
      case 0x4:
        result = lhs + operand2;
        regs_[rd] = result;
        if (set_flags) {
          setAddFlags(cpsr_, lhs, operand2, result);
        }
        return 1;
      case 0xc:
        result = lhs | operand2;
        regs_[rd] = result;
        if (set_flags) {
          setNz(cpsr_, result);
        }
        return 1;
      case 0xd:
        regs_[rd] = operand2;
        if (set_flags) {
          setNz(cpsr_, operand2);
        }
        return 1;
      case 0xe:
        result = lhs & ~operand2;
        regs_[rd] = result;
        if (set_flags) {
          setNz(cpsr_, result);
        }
        return 1;
      default:
        break;
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

  if ((instruction & 0xfe00u) == 0xbc00u) {
    const bool include_pc = (instruction & (1u << 8)) != 0;
    const u16 list = instruction & 0xffu;
    u32 count = include_pc ? 1 : 0;
    for (int reg = 0; reg < 8; ++reg) {
      if ((list & (1u << reg)) != 0) {
        ++count;
      }
    }

    u32 address = regs_[13];
    for (int reg = 0; reg < 8; ++reg) {
      if ((list & (1u << reg)) != 0) {
        regs_[reg] = bus.read32(address);
        address += 4;
      }
    }
    if (include_pc) {
      regs_[15] = bus.read32(address) & ~1u;
      address += 4;
    }
    regs_[13] += count * 4;
    return 1 + count;
  }

  if ((instruction & 0xff00u) == 0xb000u) {
    const u32 offset = (instruction & 0x7fu) << 2;
    if ((instruction & 0x0080u) != 0) {
      regs_[13] -= offset;
    } else {
      regs_[13] += offset;
    }
    return 1;
  }

  if ((instruction & 0xe000u) == 0x0000u && ((instruction >> 11) & 0x03) != 0x03) {
    const u32 op = (instruction >> 11) & 0x03;
    const u32 offset = (instruction >> 6) & 0x1f;
    const int rs = static_cast<int>((instruction >> 3) & 0x07);
    const int rd = static_cast<int>(instruction & 0x07);
    const u32 value = regs_[rs];

    if (op == 0x0) {
      regs_[rd] = value << offset;
    } else if (op == 0x1) {
      regs_[rd] = offset == 0 ? 0 : value >> offset;
    } else {
      regs_[rd] = offset == 0 ? (value & 0x80000000u ? 0xffffffffu : 0) : static_cast<u32>(static_cast<int32_t>(value) >> offset);
    }
    setNz(cpsr_, regs_[rd]);
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
    setNz(cpsr_, regs_[rd]);
    return 1;
  }

  if ((instruction & 0xe000u) == 0x2000u) {
    const u32 op = (instruction >> 11) & 0x03;
    const int rd = static_cast<int>((instruction >> 8) & 0x07);
    const u32 imm = instruction & 0xffu;

    if (op == 0x1) {
      setSubFlags(cpsr_, regs_[rd], imm, regs_[rd] - imm);
      return 1;
    }
    if (op == 0x2) {
      const u32 lhs = regs_[rd];
      regs_[rd] += imm;
      setAddFlags(cpsr_, lhs, imm, regs_[rd]);
      return 1;
    }
    if (op == 0x3) {
      const u32 lhs = regs_[rd];
      regs_[rd] -= imm;
      setSubFlags(cpsr_, lhs, imm, regs_[rd]);
      return 1;
    }

    regs_[rd] = imm;
    setNz(cpsr_, regs_[rd]);
    return 1;
  }

  if ((instruction & 0xf800u) == 0x4800u) {
    const int rd = static_cast<int>((instruction >> 8) & 0x07);
    const u32 offset = (instruction & 0xffu) << 2;
    regs_[rd] = bus.read32(((pc + 4) & ~3u) + offset);
    return 3;
  }

  if ((instruction & 0xf000u) == 0x9000u) {
    const bool load = (instruction & (1u << 11)) != 0;
    const int rd = static_cast<int>((instruction >> 8) & 0x07);
    const u32 address = regs_[13] + ((instruction & 0xffu) << 2);
    if (load) {
      regs_[rd] = bus.read32(address);
    } else {
      bus.write32(address, regs_[rd]);
    }
    return 2;
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

  if ((instruction & 0xfc00u) == 0x4000u) {
    const u32 op = (instruction >> 6) & 0x0f;
    const int rs = static_cast<int>((instruction >> 3) & 0x07);
    const int rd = static_cast<int>(instruction & 0x07);
    const u32 lhs = regs_[rd];
    const u32 rhs = regs_[rs];
    u32 result = lhs;

    switch (op) {
      case 0x0:
        result = lhs & rhs;
        regs_[rd] = result;
        setNz(cpsr_, result);
        return 1;
      case 0x1:
        result = lhs ^ rhs;
        regs_[rd] = result;
        setNz(cpsr_, result);
        return 1;
      case 0x2:
        result = rhs >= 32 ? 0 : lhs << rhs;
        regs_[rd] = result;
        setNz(cpsr_, result);
        return 1;
      case 0x3:
        result = rhs >= 32 ? 0 : lhs >> rhs;
        regs_[rd] = result;
        setNz(cpsr_, result);
        return 1;
      case 0x4:
        result = rhs >= 32 ? (lhs & 0x80000000u ? 0xffffffffu : 0) : static_cast<u32>(static_cast<int32_t>(lhs) >> rhs);
        regs_[rd] = result;
        setNz(cpsr_, result);
        return 1;
      case 0x8:
        setNz(cpsr_, lhs & rhs);
        return 1;
      case 0x9:
        result = 0 - rhs;
        regs_[rd] = result;
        setSubFlags(cpsr_, 0, rhs, result);
        return 1;
      case 0xa:
        setSubFlags(cpsr_, lhs, rhs, lhs - rhs);
        return 1;
      case 0xb:
        setAddFlags(cpsr_, lhs, rhs, lhs + rhs);
        return 1;
      case 0xc:
        result = lhs | rhs;
        regs_[rd] = result;
        setNz(cpsr_, result);
        return 1;
      case 0xd:
        result = lhs * rhs;
        regs_[rd] = result;
        setNz(cpsr_, result);
        return 1;
      case 0xe:
        result = lhs & ~rhs;
        regs_[rd] = result;
        setNz(cpsr_, result);
        return 1;
      case 0xf:
        result = ~rhs;
        regs_[rd] = result;
        setNz(cpsr_, result);
        return 1;
      default:
        break;
    }
  }

  if ((instruction & 0xff00u) == 0xdf00u) {
    last_swi_ = instruction & 0xffu;
    return 3;
  }

  if ((instruction & 0xf000u) == 0xc000u) {
    const bool load = (instruction & (1u << 11)) != 0;
    const int rb = static_cast<int>((instruction >> 8) & 0x07);
    const u16 list = instruction & 0xffu;
    u32 address = regs_[rb];
    u32 count = 0;

    for (int reg = 0; reg < 8; ++reg) {
      if ((list & (1u << reg)) == 0) {
        continue;
      }
      if (load) {
        regs_[reg] = bus.read32(address);
      } else {
        bus.write32(address, regs_[reg]);
      }
      address += 4;
      ++count;
    }

    if (count == 0) {
      if (load) {
        regs_[15] = bus.read32(address) & ~1u;
      } else {
        bus.write32(address, regs_[15] + 2);
      }
      address += 0x40;
      count = 16;
    }

    regs_[rb] += count * 4;
    return 1 + count;
  }

  if ((instruction & 0xf000u) == 0xd000u && (instruction & 0x0f00u) != 0x0f00u) {
    const u32 condition = (instruction >> 8) & 0x0f;
    if (conditionPassed(condition << 28, cpsr_)) {
      u32 offset = (instruction & 0xffu) << 1;
      if ((offset & 0x100u) != 0) {
        offset |= 0xfffffe00u;
      }
      regs_[15] = pc + 4 + offset;
      return 3;
    }
    return 1;
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

  if ((instruction & 0xf800u) == 0xe000u) {
    u32 offset = (instruction & 0x07ffu) << 1;
    if ((offset & 0x0800u) != 0) {
      offset |= 0xfffff000u;
    }
    regs_[15] = pc + 4 + offset;
    return 3;
  }

  if ((instruction & 0xf000u) == 0x8000u) {
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
