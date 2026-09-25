// Wall clock for the top bar: NTP time and a UTC offset looked up from the
// public IP address (PROJECT.md F-008, D-012, D-029).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace clock_sync {

// Starts NTP on the first call while connected, and refreshes the timezone
// hourly (every minute after a failed lookup). Call every loop.
void poll(uint64_t nowMs, bool connected);

// Writes "HH:MM", or "--:--" until both the time and the offset are known.
void text(char* out, size_t size);

// Writes today's local date as "DD-MM-YYYY", or an empty string until both the
// time and the offset are known.
void dateText(char* out, size_t size);

// Current UTC time in seconds, or 0 until NTP has synced.
int64_t utcNow();

// Writes a message's arrival time: "HH:MM" today, "HH:MM DD-MM-YYYY" on another
// day (F-015). Empty when receivedUtc is 0 or the offset is not known yet.
void arrivalText(int64_t receivedUtc, char* out, size_t size);

}  // namespace clock_sync
