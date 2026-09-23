// Countdown text for timed messages (PROJECT.md F-005).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ui {

// Whole seconds left, rounded up, so the display never shows 0 before the message is removed.
uint32_t countdownSeconds(uint32_t remainingMs);

// Writes "45s", "4:05", or "1:02:03" for the given seconds.
void formatCountdown(uint32_t seconds, char* out, size_t size);

}  // namespace ui
