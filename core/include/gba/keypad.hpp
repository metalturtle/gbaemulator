#pragma once

#include "gba/types.hpp"

namespace gba {

class Keypad {
 public:
  void reset();
  void setButton(Button button, bool pressed);
  [[nodiscard]] u16 keyInput() const { return key_input_; }

 private:
  u16 key_input_ = 0x03ff;
};

}  // namespace gba
