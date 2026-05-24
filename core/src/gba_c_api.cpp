#include "gba/gba.h"

#include <span>

#include "gba/emulator.hpp"

namespace {

gba::Emulator& instance() {
  static gba::Emulator emulator;
  return emulator;
}

}  // namespace

extern "C" {

bool gba_load_rom(const uint8_t* data, uint32_t size) {
  if (data == nullptr || size == 0) {
    return false;
  }
  return instance().loadRom(std::span<const gba::u8>(data, size));
}

void gba_reset(void) {
  instance().reset();
}

void gba_run_frame(void) {
  instance().runFrame();
}

const uint32_t* gba_framebuffer(void) {
  return instance().framebuffer();
}

void gba_set_button(uint32_t button, bool pressed) {
  if (button > static_cast<uint32_t>(gba::Button::L)) {
    return;
  }
  instance().setButton(static_cast<gba::Button>(button), pressed);
}

const uint8_t* gba_export_save(uint32_t* size) {
  const auto save = instance().exportSave();
  if (size != nullptr) {
    *size = static_cast<uint32_t>(save.size());
  }
  return save.data();
}

bool gba_import_save(const uint8_t* data, uint32_t size) {
  if (data == nullptr || size == 0) {
    return false;
  }
  return instance().importSave(std::span<const gba::u8>(data, size));
}

}
