// Clock helpers (PROJECT.md F-008, D-029). Hardware-independent.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace clk {

// Parses an ip-api.com reply such as
// {"status":"success","timezone":"Europe/Warsaw","offset":7200}.
// offsetSeconds is the current UTC offset, daylight saving included.
// Returns false for a failed lookup or an offset outside -14 h to +14 h.
bool parseTimezone(const char* json, size_t length, int32_t& offsetSeconds, char* zone, size_t zoneSize);

// Finds the body of an HTTP/1.x response with status 200. Returns false for
// any other status or a response without the header terminator.
bool httpBody(const char* response, size_t length, const char*& body, size_t& bodyLength);

// Writes local time as "HH:MM" (24-hour) for the given UTC time and offset.
void formatClock(int64_t utcSeconds, int32_t offsetSeconds, char* out, size_t size);

}  // namespace clk
