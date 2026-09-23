// Battery voltage on GPIO34 (PROJECT.md F-007; BOARDS.md Power, Q-004).
#pragma once

#include <stdint.h>

namespace battery {

// False on boards without battery sense (board::kHasBatterySense).
bool available();

void begin();

// Battery voltage in millivolts, averaged over several samples; 0 when not available.
uint32_t readMillivolts();

}  // namespace battery
