#include "clock_logic.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace clk {

namespace {

constexpr int32_t kMaxOffsetSeconds = 14 * 3600;
constexpr int64_t kSecondsPerDay = 86400;

// Rounds toward minus infinity, so times before 1970 fall on the right day.
int64_t floorDiv(int64_t a, int64_t b)
{
    return a / b - (a % b != 0 && (a < 0) != (b < 0) ? 1 : 0);
}

struct Date {
    int64_t year;
    unsigned month;  // 1..12
    unsigned day;    // 1..31
};

// Gregorian date of a day count since 1970-01-01 (Howard Hinnant's civil_from_days).
Date dateFromDays(int64_t days)
{
    days += 719468;
    const int64_t era = floorDiv(days, 146097);
    const int64_t dayOfEra = days - era * 146097;
    const int64_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const int64_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const int64_t monthIndex = (5 * dayOfYear + 2) / 153;  // March = 0
    Date date;
    date.day = static_cast<unsigned>(dayOfYear - (153 * monthIndex + 2) / 5 + 1);
    date.month = static_cast<unsigned>(monthIndex < 10 ? monthIndex + 3 : monthIndex - 9);
    date.year = yearOfEra + era * 400 + (date.month <= 2 ? 1 : 0);
    return date;
}

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

void formatArrival(int64_t receivedUtc, int64_t nowUtc, int32_t offsetSeconds, char* out, size_t size)
{
    const int64_t local = receivedUtc + offsetSeconds;
    const int64_t day = floorDiv(local, kSecondsPerDay);
    const int64_t secondsOfDay = local - day * kSecondsPerDay;
    const int hour = static_cast<int>(secondsOfDay / 3600);
    const int minute = static_cast<int>(secondsOfDay / 60 % 60);
    if (nowUtc != 0 && floorDiv(nowUtc + offsetSeconds, kSecondsPerDay) == day) {
        snprintf(out, size, "%02d:%02d", hour, minute);
        return;
    }
    const Date date = dateFromDays(day);
    snprintf(out, size, "%02d:%02d %02u-%02u-%04lld", hour, minute, date.day, date.month,
             static_cast<long long>(date.year));
}

}  // namespace clk
