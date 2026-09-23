// Battery voltage on GPIO34 (PROJECT.md F-007; BOARDS.md Power, Q-004).
#pragma once

#include <stdint.h>

namespace battery {

void begin();

// Battery voltage in millivolts, averaged over several samples.
uint32_t readMillivolts();

}  // namespace battery
