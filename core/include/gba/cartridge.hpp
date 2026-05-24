#pragma once

#include <span>
#include <string>
#include <vector>

#include "gba/types.hpp"

namespace gba {

struct RomHeader {
  std::string title;
  std::string game_code;
  std::string maker_code;
  u8 fixed_value = 0;
  u8 complement = 0;
  u8 version = 0;
};

class Cartridge {
 public:
  bool load(std::span<const u8> rom);
  void reset();

  [[nodiscard]] bool loaded() const { return !rom_.empty(); }
  [[nodiscard]] const RomHeader& header() const { return header_; }
  [[nodiscard]] std::span<const u8> rom() const { return rom_; }
  [[nodiscard]] u8 readRom8(u32 address) const;
  [[nodiscard]] u16 readRom16(u32 address) const;
  [[nodiscard]] u32 readRom32(u32 address) const;

  [[nodiscard]] std::span<const u8> save() const { return save_; }
  bool importSave(std::span<const u8> data);
  [[nodiscard]] u8 readSave8(u32 address) const;
  void writeSave8(u32 address, u8 value);

 private:
  void parseHeader();

  std::vector<u8> rom_;
  std::vector<u8> save_;
  RomHeader header_;
};

}  // namespace gba
