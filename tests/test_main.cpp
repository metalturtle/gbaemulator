#include <fstream>
#include <sstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "gba/emulator.hpp"

namespace {

using gba::u8;
using gba::u16;
using gba::u32;

struct TestFailure : std::runtime_error {
  using std::runtime_error::runtime_error;
};

#define EXPECT_TRUE(expr)                                                                  \
  do {                                                                                     \
    if (!(expr)) {                                                                         \
      throw TestFailure(std::string("EXPECT_TRUE failed: ") + #expr);                    \
    }                                                                                      \
  } while (false)

#define EXPECT_EQ(lhs, rhs)                                                                \
  do {                                                                                     \
    const auto lhs_value = (lhs);                                                          \
    const auto rhs_value = (rhs);                                                          \
    if (!(lhs_value == rhs_value)) {                                                       \
      std::ostringstream message;                                                          \
      message << "EXPECT_EQ failed: " << #lhs << " != " << #rhs << " (" << lhs_value    \
              << " vs " << rhs_value << ")";                                             \
      throw TestFailure(message.str());                                                     \
    }                                                                                      \
  } while (false)

std::vector<u8> loadFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw TestFailure("failed to open " + path);
  }
  return std::vector<u8>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::vector<u8> makeSyntheticRom() {
  std::vector<u8> rom(1024, 0);
  rom[0] = 0x7f;
  rom[1] = 0x00;
  rom[2] = 0x00;
  rom[3] = 0xea;

  const std::string title = "TEST ROM";
  for (size_t i = 0; i < title.size(); ++i) {
    rom[0xa0 + i] = static_cast<u8>(title[i]);
  }
  const std::string code = "TEST";
  for (size_t i = 0; i < code.size(); ++i) {
    rom[0xac + i] = static_cast<u8>(code[i]);
  }
  rom[0xb0] = '0';
  rom[0xb1] = '1';
  rom[0xb2] = 0x96;
  rom[0xbc] = 0;
  rom[0xbd] = 0;
  return rom;
}

void testCartridgeHeader() {
  gba::Cartridge cartridge;
  const auto rom = makeSyntheticRom();
  EXPECT_TRUE(cartridge.load(rom));
  EXPECT_EQ(cartridge.header().title, std::string("TEST ROM"));
  EXPECT_EQ(cartridge.header().game_code, std::string("TEST"));
  EXPECT_EQ(cartridge.header().maker_code, std::string("01"));
  EXPECT_EQ(cartridge.header().fixed_value, static_cast<u8>(0x96));
  EXPECT_EQ(cartridge.readRom32(0x08000000), static_cast<u32>(0xea00007f));
}

void testBusMemoryMap() {
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(makeSyntheticRom()));

  auto& bus = emulator.bus();
  bus.write32(0x02000000, 0x11223344);
  EXPECT_EQ(bus.read32(0x02000000), static_cast<u32>(0x11223344));
  EXPECT_EQ(bus.read8(0x02000000), static_cast<u8>(0x44));
  EXPECT_EQ(bus.read16(0x02000002), static_cast<u16>(0x1122));

  bus.write16(0x03007ffe, 0xabcd);
  EXPECT_EQ(bus.read16(0x03007ffe), static_cast<u16>(0xabcd));

  bus.write16(0x04000000, 0x0403);
  EXPECT_EQ(bus.read16(0x04000000), static_cast<u16>(0x0403));
}

void testResetAndFrameProgress() {
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(makeSyntheticRom()));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x08000000));
  EXPECT_EQ(static_cast<u32>(emulator.cpu().mode()), static_cast<u32>(gba::CpuMode::System));
  EXPECT_EQ(emulator.scheduler().cycles(), static_cast<gba::u64>(0));

  emulator.runFrame();
  EXPECT_EQ(emulator.scheduler().cycles(), static_cast<gba::u64>(gba::kCyclesPerFrame));
  EXPECT_TRUE(emulator.cpu().instructionsExecuted() > 0);
  EXPECT_TRUE((emulator.interrupts().flag() & gba::Interrupts::VBlank) != 0);
}

void testArmBranchExecution() {
  auto rom = makeSyntheticRom();
  // B +0x10 from 0x08000000. ARM branch target is PC+8+offset.
  rom[0] = 0x04;
  rom[1] = 0x00;
  rom[2] = 0x00;
  rom[3] = 0xea;

  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));
  const u32 cycles = emulator.cpu().step(emulator.bus());
  EXPECT_EQ(cycles, static_cast<u32>(3));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x08000018));
}

void testPokemonEntryBranch() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));
  const u32 cycles = emulator.cpu().step(emulator.bus());
  EXPECT_EQ(cycles, static_cast<u32>(3));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x08000204));
}

void testPokemonStartupReachesThumbEntry() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));

  for (int i = 0; i < 32 && !emulator.cpu().thumb(); ++i) {
    emulator.cpu().step(emulator.bus());
  }

  EXPECT_TRUE(emulator.cpu().thumb());
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x080003a4));
  EXPECT_EQ(emulator.cpu().reg(14), static_cast<u32>(0x08000234));
  EXPECT_EQ(emulator.bus().read32(0x03007ffc), static_cast<u32>(0x08000248));
}

void testPokemonFirstThumbCall() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));

  for (int i = 0; i < 32 && !emulator.cpu().thumb(); ++i) {
    emulator.cpu().step(emulator.bus());
  }

  EXPECT_EQ(emulator.cpu().unimplementedInstructions(), static_cast<gba::u64>(0));
  for (int i = 0; i < 6; ++i) {
    emulator.cpu().step(emulator.bus());
  }

  EXPECT_EQ(emulator.cpu().unimplementedInstructions(), static_cast<gba::u64>(0));
  EXPECT_EQ(emulator.cpu().reg(0), static_cast<u32>(0xff));
  EXPECT_EQ(emulator.cpu().reg(13), static_cast<u32>(0x03007e28));
  EXPECT_EQ(emulator.cpu().reg(14), static_cast<u32>(0x080003b1));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x082e70a8));
}

void testPokemonFirstBiosWrapperReturns() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));

  for (int i = 0; i < 32 && !emulator.cpu().thumb(); ++i) {
    emulator.cpu().step(emulator.bus());
  }
  for (int i = 0; i < 8 && emulator.cpu().reg(15) != 0x080003b0; ++i) {
    emulator.cpu().step(emulator.bus());
  }

  EXPECT_EQ(emulator.cpu().unimplementedInstructions(), static_cast<gba::u64>(0));
  EXPECT_EQ(emulator.cpu().lastSwi(), static_cast<u32>(1));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x080003b0));
  EXPECT_TRUE(emulator.cpu().thumb());
}

void testPokemonIoSetupAndSecondThumbCall() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));

  for (int i = 0; i < 64 && emulator.cpu().reg(15) != 0x080003bc; ++i) {
    emulator.cpu().step(emulator.bus());
  }

  EXPECT_EQ(emulator.cpu().unimplementedInstructions(), static_cast<gba::u64>(0));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x080003bc));
  EXPECT_EQ(emulator.cpu().reg(1), static_cast<u32>(0x05000000));
  EXPECT_EQ(emulator.cpu().reg(2), static_cast<u32>(0x00007fff));
  EXPECT_EQ(emulator.bus().read16(0x05000000), static_cast<u16>(0x7fff));
}

void testPokemonStartupWritesEwramByte() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));

  for (int i = 0; i < 128 && emulator.cpu().reg(15) != 0x08001002; ++i) {
    emulator.cpu().step(emulator.bus());
  }

  EXPECT_EQ(emulator.cpu().unimplementedInstructions(), static_cast<gba::u64>(0));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x08001002));
  EXPECT_EQ(emulator.bus().read8(0x03000818), static_cast<u8>(0));
}

void testPokemonStartupThumbAluOrsByte() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));

  for (int i = 0; i < 160 && emulator.cpu().reg(15) != 0x08001008; ++i) {
    emulator.cpu().step(emulator.bus());
  }

  EXPECT_EQ(emulator.cpu().unimplementedInstructions(), static_cast<gba::u64>(0));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x08001008));
  EXPECT_EQ(emulator.cpu().reg(0), static_cast<u32>(0x000000ff));
}

void testPokemonStartupThumbImmediateIncrement() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));

  for (int i = 0; i < 180 && emulator.cpu().reg(15) != 0x0800100c; ++i) {
    emulator.cpu().step(emulator.bus());
  }

  EXPECT_EQ(emulator.cpu().unimplementedInstructions(), static_cast<gba::u64>(0));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x0800100c));
  EXPECT_EQ(emulator.cpu().reg(2), static_cast<u32>(1));
  EXPECT_EQ(emulator.bus().read8(0x03000878), static_cast<u8>(0xff));
}

void testPokemonStartupConditionalLoopCompletes() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));

  for (int i = 0; i < 1000 && emulator.cpu().reg(15) != 0x08001010; ++i) {
    emulator.cpu().step(emulator.bus());
  }

  EXPECT_EQ(emulator.cpu().unimplementedInstructions(), static_cast<gba::u64>(0));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x08001010));
  EXPECT_EQ(emulator.cpu().reg(2), static_cast<u32>(0x60));
  EXPECT_EQ(emulator.bus().read8(0x03000878), static_cast<u8>(0xff));
  EXPECT_EQ(emulator.bus().read8(0x030008d7), static_cast<u8>(0xff));
}

void testPokemonStartupHelperReturns() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));

  for (int i = 0; i < 1100 && emulator.cpu().reg(15) != 0x080003be; ++i) {
    emulator.cpu().step(emulator.bus());
  }

  EXPECT_EQ(emulator.cpu().unimplementedInstructions(), static_cast<gba::u64>(0));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x080003be));
  EXPECT_EQ(emulator.cpu().reg(13), static_cast<u32>(0x03007e28));
  EXPECT_TRUE(emulator.cpu().thumb());
}

void testPokemonStartupHalfwordIoClear() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));

  for (int i = 0; i < 1300 && emulator.cpu().reg(15) != 0x080005d2; ++i) {
    emulator.cpu().step(emulator.bus());
  }

  EXPECT_EQ(emulator.cpu().unimplementedInstructions(), static_cast<gba::u64>(0));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x080005d2));
  EXPECT_EQ(emulator.bus().read16(0x030022d8), static_cast<u16>(0));
}

void testKeypadActiveLow() {
  gba::Emulator emulator;
  EXPECT_EQ(emulator.keypad().keyInput(), static_cast<u16>(0x03ff));
  emulator.setButton(gba::Button::A, true);
  EXPECT_EQ(emulator.keypad().keyInput() & 0x0001, static_cast<u16>(0));
  emulator.setButton(gba::Button::A, false);
  EXPECT_EQ(emulator.keypad().keyInput() & 0x0001, static_cast<u16>(1));
}

void testPokemonEmeraldSmokeLoad() {
  const auto rom = loadFile("pokemonemerald.gba");
  gba::Emulator emulator;
  EXPECT_TRUE(emulator.loadRom(rom));
  EXPECT_EQ(emulator.cartridge().header().title, std::string("POKEMON EMER"));
  EXPECT_EQ(emulator.cartridge().header().game_code, std::string("BPEE"));
  EXPECT_EQ(emulator.cartridge().header().maker_code, std::string("01"));
  EXPECT_EQ(emulator.cartridge().header().fixed_value, static_cast<u8>(0x96));
  EXPECT_EQ(emulator.cpu().reg(15), static_cast<u32>(0x08000000));
}

using TestFn = void (*)();

struct TestCase {
  const char* name;
  TestFn fn;
};

}  // namespace

int main() {
  const TestCase tests[] = {
      {"cartridge header", testCartridgeHeader},
      {"bus memory map", testBusMemoryMap},
      {"reset and frame progress", testResetAndFrameProgress},
      {"ARM branch execution", testArmBranchExecution},
      {"Pokemon entry branch", testPokemonEntryBranch},
      {"Pokemon startup reaches Thumb entry", testPokemonStartupReachesThumbEntry},
      {"Pokemon first Thumb call", testPokemonFirstThumbCall},
      {"Pokemon first BIOS wrapper returns", testPokemonFirstBiosWrapperReturns},
      {"Pokemon IO setup and second Thumb call", testPokemonIoSetupAndSecondThumbCall},
      {"Pokemon startup writes EWRAM byte", testPokemonStartupWritesEwramByte},
      {"Pokemon startup Thumb ALU ORs byte", testPokemonStartupThumbAluOrsByte},
      {"Pokemon startup Thumb immediate increment", testPokemonStartupThumbImmediateIncrement},
      {"Pokemon startup conditional loop completes", testPokemonStartupConditionalLoopCompletes},
      {"Pokemon startup helper returns", testPokemonStartupHelperReturns},
      {"Pokemon startup halfword IO clear", testPokemonStartupHalfwordIoClear},
      {"keypad active-low", testKeypadActiveLow},
      {"pokemon emerald smoke load", testPokemonEmeraldSmokeLoad},
  };

  int failures = 0;
  for (const auto& test : tests) {
    try {
      test.fn();
      std::cout << "[PASS] " << test.name << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
    }
  }

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }

  std::cout << "All tests passed\n";
  return 0;
}
