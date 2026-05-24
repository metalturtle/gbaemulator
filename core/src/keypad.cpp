#include "gba/keypad.hpp"

namespace gba {

void Keypad::reset() {
  key_input_ = 0x03ff;
}

void Keypad::setButton(Button button, bool pressed) {
  const u16 mask = static_cast<u16>(1u << static_cast<u32>(button));
  if (pressed) {
    key_input_ &= static_cast<u16>(~mask);
  } else {
    key_input_ |= mask;
  }
  key_input_ &= 0x03ff;
}

}  // namespace gba
