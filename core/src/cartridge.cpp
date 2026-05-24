#include "gba/cartridge.hpp"

#include <algorithm>

namespace gba {

namespace {

std::string asciiField(std::span<const u8> bytes) {
  std::string value;
  value.reserve(bytes.size());
  for (u8 byte : bytes) {
    if (byte == 0) {
      break;
    }
    value.push_back(static_cast<char>(byte));
  }
  while (!value.empty() && value.back() == ' ') {
    value.pop_back();
  }
  return value;
}

}  // namespace

bool Cartridge::load(std::span<const u8> rom) {
  if (rom.size() < 0xc0) {
    return false;
  }

  rom_.assign(rom.begin(), rom.end());
  save_.assign(128 * 1024, 0xff);  // Pokemon Emerald uses 1Mbit flash-style backup.
  parseHeader();
  return header_.fixed_value == 0x96;
}

void Cartridge::reset() {}

u8 Cartridge::readRom8(u32 address) const {
  if (rom_.empty()) {
    return 0xff;
  }
  const u32 offset = address - 0x08000000u;
  if (offset >= rom_.size()) {
    return 0xff;
  }
  return rom_[offset];
}

u16 Cartridge::readRom16(u32 address) const {
  address &= ~1u;
  return static_cast<u16>(readRom8(address) | (readRom8(address + 1) << 8));
}

u32 Cartridge::readRom32(u32 address) const {
  address &= ~3u;
  return static_cast<u32>(readRom16(address) | (readRom16(address + 2) << 16));
}

bool Cartridge::importSave(std::span<const u8> data) {
  if (data.empty() || data.size() > save_.size()) {
    return false;
  }
  std::fill(save_.begin(), save_.end(), 0xff);
  std::copy(data.begin(), data.end(), save_.begin());
  return true;
}

u8 Cartridge::readSave8(u32 address) const {
  if (save_.empty()) {
    return 0xff;
  }
  return save_[(address - 0x0e000000u) % save_.size()];
}

void Cartridge::writeSave8(u32 address, u8 value) {
  if (save_.empty()) {
    return;
  }
  // This is raw byte storage for now. Flash command-state emulation is a follow-up.
  save_[(address - 0x0e000000u) % save_.size()] = value;
}

void Cartridge::parseHeader() {
  header_ = {};
  header_.title = asciiField(std::span<const u8>(&rom_[0xa0], 12));
  header_.game_code = asciiField(std::span<const u8>(&rom_[0xac], 4));
  header_.maker_code = asciiField(std::span<const u8>(&rom_[0xb0], 2));
  header_.fixed_value = rom_[0xb2];
  header_.version = rom_[0xbc];
  header_.complement = rom_[0xbd];
}

}  // namespace gba
