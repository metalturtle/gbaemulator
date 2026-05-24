#include "gba/interrupts.hpp"

namespace gba {

void Interrupts::reset() {
  ie_ = 0;
  if_ = 0;
  ime_ = 0;
}

void Interrupts::request(Flag flag) {
  if_ |= static_cast<u16>(flag);
}

bool Interrupts::pending() const {
  return ime_ != 0 && (ie_ & if_) != 0;
}

}  // namespace gba
