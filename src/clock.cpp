#include "clock.h"

#include <Arduino.h>
#include <WiFi.h>
#include <clock_logic.h>
#include <time.h>

namespace clock_sync {

namespace {

// Free service without a key, HTTP only, non-commercial use (D-029). It sees the
// device's public IP address, which the lookup needs.
constexpr const char* kTimezoneHost = "ip-api.com";
constexpr const char* kTimezonePath = "/json/?fields=status,message,timezone,offset";
constexpr uint32_t kRefreshMs = 3600000;  // picks up daylight saving changes within an hour
constexpr uint32_t kRetryMs = 60000;
constexpr uint32_t kHttpTimeoutMs = 3000;
constexpr time_t kValidTime = 1700000000;  // any time before this means NTP has not synced yet

bool ntpStarted = false;
bool ntpReported = false;
bool offsetKnown = false;
int32_t offsetSeconds = 0;
bool lookedUp = false;
bool lastLookupOk = false;
uint64_t lastLookupMs = 0;

// Plain HTTP/1.0 GET: the server closes the connection after the body, and no
// chunked encoding needs handling. HTTPClient would add about 150 KB of TLS code.
bool fetchTimezone(char* response, size_t size, size_t& length)
{
    WiFiClient client;
    if (!client.connect(kTimezoneHost, 80, kHttpTimeoutMs)) {
        Serial.println("Clock: timezone lookup failed, no connection");
        return false;
    }
    client.printf("GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", kTimezonePath, kTimezoneHost);
    length = 0;
    const uint64_t deadline = millis() + kHttpTimeoutMs;
    while ((client.connected() || client.available() > 0) && millis() < deadline && length < size - 1) {
        const int read = client.read(reinterpret_cast<uint8_t*>(response + length), size - 1 - length);
        if (read > 0) {
            length += static_cast<size_t>(read);
        } else {
            delay(5);
        }
    }
    client.stop();
    response[length] = '\0';
    return length > 0;
}

bool lookUpTimezone()
{
    static char response[768];
    size_t length = 0;
    if (!fetchTimezone(response, sizeof(response), length)) {
        return false;
    }
    const char* body = nullptr;
    size_t bodyLength = 0;
    int32_t offset = 0;
    char zone[48];
    if (!clk::httpBody(response, length, body, bodyLength) ||
        !clk::parseTimezone(body, bodyLength, offset, zone, sizeof(zone))) {
        Serial.printf("Clock: timezone lookup failed: %.120s\n", response);
        return false;
    }
    if (!offsetKnown || offset != offsetSeconds) {
        Serial.printf("Clock: timezone %s, UTC offset %+ld s\n", zone, static_cast<long>(offset));
    }
    offsetSeconds = offset;
    offsetKnown = true;
    return true;
}

}  // namespace

void poll(uint64_t nowMs, bool connected)
{
    if (!connected) {
        return;
    }
    if (!ntpStarted) {
        configTime(0, 0, "pool.ntp.org", "time.google.com");
        ntpStarted = true;
    }
    if (!ntpReported && time(nullptr) > kValidTime) {
        ntpReported = true;
        Serial.println("Clock: NTP time received");
    }
    const uint32_t interval = lastLookupOk ? kRefreshMs : kRetryMs;
    if (!lookedUp || nowMs - lastLookupMs >= interval) {
        lookedUp = true;
        lastLookupMs = nowMs;
        lastLookupOk = lookUpTimezone();
    }
}

void text(char* out, size_t size)
{
    const time_t now = time(nullptr);
    if (!offsetKnown || now < kValidTime) {
        snprintf(out, size, "--:--");
        return;
    }
    clk::formatClock(static_cast<int64_t>(now), offsetSeconds, out, size);
}

int64_t utcNow()
{
    const time_t now = time(nullptr);
    return now < kValidTime ? 0 : static_cast<int64_t>(now);
}

void arrivalText(int64_t receivedUtc, char* out, size_t size)
{
    if (receivedUtc == 0 || !offsetKnown) {
        snprintf(out, size, "%s", "");
        return;
    }
    clk::formatArrival(receivedUtc, utcNow(), offsetSeconds, out, size);
}

}  // namespace clock_sync
