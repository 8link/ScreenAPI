// Display rendering: boot screen, top bar, message screen (PROJECT.md F-005, F-007, F-010).
#pragma once

#include <message_queue.h>
#include <stdint.h>

#include <Print.h>

namespace screen {

// RGB565 colors callers pass in (for example the setup status line).
constexpr uint16_t kColorGreen = 0x07E0;
constexpr uint16_t kColorRed = 0xF800;
constexpr uint16_t kColorLightGrey = 0xD69A;

enum class Link { Up, Pending, Down };

// Contents of the top bar (F-007).
struct StatusBar {
    char network[24];    // IP address or a short status
    Link link;           // colors the network pill's status stripe
    char clock[8];       // "18:27", or "--:--" before the time is known
    bool hasBattery;     // the board has battery sense; otherwise no battery pill
    bool batteryKnown;   // false until the first reading
    bool externalPower;  // on USB power: lightning bolt instead of a level
    int batteryPercent;  // 0..100
};

// Powers on and initializes the panel and the off-screen buffer, and measures
// the board's fonts. Returns false if the buffer could not be allocated.
bool begin();

void showBootScreen();

// Message screen. Redraws when the queue changed, text is scrolling, the
// countdown changed, or the top bar changed. Call every loop while the
// message screen is visible; pass queueChanged = true when returning to it.
void update(const mq::MessageQueue& queue, uint64_t nowMs, bool queueChanged, const StatusBar& bar);

// Wi-Fi setup screen (F-002): QR code for the ESP BLE Provisioning app, the
// device name, the proof-of-possession code, and a status line.
void showSetup(const char* qrPayload, const char* serviceName, const char* pop, const char* status, uint16_t statusColor);

// Welcome screen after connecting (F-009): version, network name, IP address, MCP URL.
void showWelcome(const char* ssid, const char* ip);

// Shows "Queue full, new messages dropped" over the message screen for 3 s (F-005).
void showQueueFullPopup(uint64_t nowMs);

// Writes the last drawn frame as text (F-012): a header line
// "SCREENSHOT <width> <height> indexed565", a line with the 256-color RGB565
// palette (4 hex digits each), one line of palette indices (2 hex digits each)
// per pixel row, then "END". Read by tools/screenshot.py. Blocks for about 6 s
// at 115200 baud on the T-Display.
void sendScreenshot(Print& out);

// Calibration for rounded display corners (D-036): colored arcs of radius 20 to
// 70 px in each corner and a legend. The smallest fully visible arc gives
// board::kCornerRadius.
void showCornerTest();

// One centered line of text on an otherwise empty screen.
void showNotice(const char* text);

}  // namespace screen
