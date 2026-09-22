// Auto-scroll timing for text larger than its box (PROJECT.md F-005):
// hold at the start, move at a constant speed, hold at the end, jump back.
#pragma once

#include <stdint.h>

namespace ui {

// Returns the scroll offset in pixels, from 0 to contentSize - viewportSize.
// Returns 0 when the content fits or speed is 0.
int scrollOffset(int contentSize, int viewportSize, uint64_t elapsedMs, uint32_t pixelsPerSecond, uint32_t pauseMs);

}  // namespace ui
