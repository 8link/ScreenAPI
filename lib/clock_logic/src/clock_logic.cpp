#include "clock_logic.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace clk {

namespace {

constexpr int32_t kMaxOffsetSeconds = 14 * 3600;
constexpr int64_t kSecondsPerDay = 86400;

}  // namespace

bool parseTimezone(const char* json, size_t length, int32_t& offsetSeconds, char* zone, size_t zoneSize)
{
    JsonDocument doc;
    if (deserializeJson(doc, json, length) != DeserializationError::Ok) {
        return false;
    }
    if (doc["status"] != "success" || !doc["offset"].is<long>()) {
        return false;
    }
    const long offset = doc["offset"].as<long>();
    if (offset < -kMaxOffsetSeconds || offset > kMaxOffsetSeconds) {
        return false;
    }
    offsetSeconds = static_cast<int32_t>(offset);
    snprintf(zone, zoneSize, "%s", doc["timezone"] | "");
    return true;
}

bool httpBody(const char* response, size_t length, const char*& body, size_t& bodyLength)
{
    // Status line: "HTTP/1.x 200 ..."
    if (length < 12 || strncmp(response, "HTTP/1.", 7) != 0 || strncmp(response + 8, " 200", 4) != 0) {
        return false;
    }
    for (size_t i = 0; i + 4 <= length; i++) {
        if (memcmp(response + i, "\r\n\r\n", 4) == 0) {
            body = response + i + 4;
            bodyLength = length - i - 4;
            return true;
        }
    }
    return false;
}

void formatClock(int64_t utcSeconds, int32_t offsetSeconds, char* out, size_t size)
{
    int64_t secondsOfDay = (utcSeconds + offsetSeconds) % kSecondsPerDay;
    if (secondsOfDay < 0) {
        secondsOfDay += kSecondsPerDay;
    }
    snprintf(out, size, "%02d:%02d", static_cast<int>(secondsOfDay / 3600), static_cast<int>(secondsOfDay / 60 % 60));
}

}  // namespace clk
