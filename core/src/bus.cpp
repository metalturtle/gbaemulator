#include "gba/bus.hpp"

#include <algorithm>

namespace gba {

Bus::Bus(Cartridge& cartridge, Ppu& ppu, Timers& timers, Interrupts& interrupts, Keypad& keypad)
    : cartridge_(cartridge), ppu_(ppu), timers_(timers), interrupts_(interrupts), keypad_(keypad) {}

void Bus::reset() {
  std::fill(bios_.begin(), bios_.end(), 0);
  std::fill(ewram_.begin(), ewram_.end(), 0);
  std::fill(iwram_.begin(), iwram_.end(), 0);
  std::fill(palette_.begin(), palette_.end(), 0);
  std::fill(vram_.begin(), vram_.end(), 0);
  std::fill(oam_.begin(), oam_.end(), 0);
}

u8 Bus::read8(u32 address) {
  switch (region(address)) {
    case Region::Bios:
      return bios_[address & 0x3fff];
    case Region::Ewram:
      return ewram_[(address - 0x02000000u) & 0x3ffff];
    case Region::Iwram:
      return iwram_[(address - 0x03000000u) & 0x7fff];
    case Region::Io:
      return readIo8(address);
    case Region::Palette:
      return palette_[(address - 0x05000000u) & 0x3ff];
    case Region::Vram:
      return vram_[(address - 0x06000000u) % vram_.size()];
    case Region::Oam:
      return oam_[(address - 0x07000000u) & 0x3ff];
    case Region::Rom:
      return cartridge_.readRom8(0x08000000u + ((address - 0x08000000u) % 0x02000000u));
    case Region::Save:
      return cartridge_.readSave8(address);
    case Region::Unmapped:
      return 0xff;
  }
  return 0xff;
}

u16 Bus::read16(u32 address) {
  address &= ~1u;
  switch (region(address)) {
    case Region::Io:
      return readIo16(address);
    case Region::Rom:
      return cartridge_.readRom16(0x08000000u + ((address - 0x08000000u) % 0x02000000u));
    default:
      return static_cast<u16>(read8(address) | (read8(address + 1) << 8));
  }
}

u32 Bus::read32(u32 address) {
  address &= ~3u;
  return static_cast<u32>(read16(address) | (read16(address + 2) << 16));
}

void Bus::write8(u32 address, u8 value) {
  switch (region(address)) {
    case Region::Ewram:
      ewram_[(address - 0x02000000u) & 0x3ffff] = value;
      break;
    case Region::Iwram:
      iwram_[(address - 0x03000000u) & 0x7fff] = value;
      break;
    case Region::Io:
      writeIo8(address, value);
      break;
    case Region::Palette:
      palette_[(address - 0x05000000u) & 0x3ff] = value;
      break;
    case Region::Vram:
      vram_[(address - 0x06000000u) % vram_.size()] = value;
      break;
    case Region::Oam:
      oam_[(address - 0x07000000u) & 0x3ff] = value;
      break;
    case Region::Save:
      cartridge_.writeSave8(address, value);
      break;
    default:
      break;
  }
}

void Bus::write16(u32 address, u16 value) {
  address &= ~1u;
  if (region(address) == Region::Io) {
    writeIo16(address, value);
    return;
  }
  write8(address, static_cast<u8>(value & 0xff));
  write8(address + 1, static_cast<u8>(value >> 8));
}

void Bus::write32(u32 address, u32 value) {
  address &= ~3u;
  write16(address, static_cast<u16>(value & 0xffff));
  write16(address + 2, static_cast<u16>(value >> 16));
}

Bus::Region Bus::region(u32 address) {
  switch (address >> 24) {
    case 0x00:
      return Region::Bios;
    case 0x02:
      return Region::Ewram;
    case 0x03:
      return Region::Iwram;
    case 0x04:
      return Region::Io;
    case 0x05:
      return Region::Palette;
    case 0x06:
      return Region::Vram;
    case 0x07:
      return Region::Oam;
    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0d:
      return Region::Rom;
    case 0x0e:
    case 0x0f:
      return Region::Save;
    default:
      return Region::Unmapped;
  }
}

u8 Bus::readIo8(u32 address) {
  const u16 value = readIo16(address & ~1u);
  return (address & 1u) != 0 ? static_cast<u8>(value >> 8) : static_cast<u8>(value & 0xff);
}

u16 Bus::readIo16(u32 address) {
  switch (address & 0x03fe) {
    case 0x0000:
      return ppu_.dispcnt();
    case 0x0004:
      return ppu_.dispstat();
    case 0x0006:
      return ppu_.vcount();
    case 0x0100:
    case 0x0104:
    case 0x0108:
    case 0x010c:
      return timers_.readCounter(static_cast<int>(((address & 0x000c) >> 2)));
    case 0x0102:
    case 0x0106:
    case 0x010a:
    case 0x010e:
      return timers_.readControl(static_cast<int>(((address & 0x000c) >> 2)));
    case 0x0130:
      return keypad_.keyInput();
    case 0x0200:
      return interrupts_.ie();
    case 0x0202:
      return interrupts_.flag();
    case 0x0208:
      return interrupts_.ime();
    default:
      return 0;
  }
}

void Bus::writeIo8(u32 address, u8 value) {
  const u32 aligned = address & ~1u;
  u16 current = readIo16(aligned);
  if ((address & 1u) != 0) {
    current = static_cast<u16>((current & 0x00ff) | (value << 8));
  } else {
    current = static_cast<u16>((current & 0xff00) | value);
  }
  writeIo16(aligned, current);
}

void Bus::writeIo16(u32 address, u16 value) {
  switch (address & 0x03fe) {
    case 0x0000:
      ppu_.setDispcnt(value);
      break;
    case 0x0004:
      ppu_.setDispstat(value);
      break;
    case 0x0100:
    case 0x0104:
    case 0x0108:
    case 0x010c:
      timers_.writeReload(static_cast<int>(((address & 0x000c) >> 2)), value);
      break;
    case 0x0102:
    case 0x0106:
    case 0x010a:
    case 0x010e:
      timers_.writeControl(static_cast<int>(((address & 0x000c) >> 2)), value);
      break;
    case 0x0200:
      interrupts_.setIe(value);
      break;
    case 0x0202:
      interrupts_.acknowledge(value);
      break;
    case 0x0208:
      interrupts_.setIme(value);
      break;
    default:
      break;
  }
}

}  // namespace gba
