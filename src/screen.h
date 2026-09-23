// Display rendering: boot screen, top bar, message screen (PROJECT.md F-005, F-007, F-010).
#pragma once

#include <message_queue.h>
#include <stdint.h>

namespace screen {

// Initializes the panel and the off-screen buffer. Returns false if the
// buffer could not be allocated.
bool begin();

void showBootScreen();

// Redraws when the queue changed, text is scrolling, or the countdown changed. Call every loop.
void update(const mq::MessageQueue& queue, uint64_t nowMs, bool queueChanged);

}  // namespace screen
