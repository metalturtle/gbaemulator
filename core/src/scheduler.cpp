#include "gba/scheduler.hpp"

namespace gba {

void Scheduler::reset() {
  cycles_ = 0;
}

void Scheduler::advance(u32 cycles) {
  cycles_ += cycles;
}

}  // namespace gba
