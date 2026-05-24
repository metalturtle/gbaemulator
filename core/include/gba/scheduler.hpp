#pragma once

#include "gba/types.hpp"

namespace gba {

class Scheduler {
 public:
  void reset();
  void advance(u32 cycles);
  [[nodiscard]] u64 cycles() const { return cycles_; }

 private:
  u64 cycles_ = 0;
};

}  // namespace gba
