// Board hardware layer (PROJECT.md D-032): everything the firmware needs from
// a board, and nothing else. The rest of the firmware (Wi-Fi, MCP, queue,
// storage, layout, clock) is shared by all boards.
//
// To port ScreenAPI to another ESP32 board with Wi-Fi and a display, copy
// src/boards/template/hal.cpp and include/boards/template/, implement the
// functions below, and add a PlatformIO env (PROJECT.md "Adding a board").
// platformio.ini compiles only the selected board's folder.
//
// Requirements: an ESP32 family chip with Wi-Fi and BLE (BLE for Wi-Fi setup),
// a display supported by Arduino_GFX, at least 240 x 135 pixels after
// rotation, width x height bytes of RAM for the frame buffer (PSRAM is used
// when present), two inputs (a touchscreen can be the second), and an app
// partition of about 1.8 MB. A battery reading is optional.
#pragma once

#include <Arduino_GFX_Library.h>
#include <stdint.h>

namespace hal {

// u8g2 fonts (ASCII, "_tr" variants) for the four text roles on screen. Pick
// sizes for the panel's pixel density; the layout adapts to their metrics.
struct Fonts {
    const uint8_t* bar;    // top bar: IP address, queue position, clock
    const uint8_t* title;  // message title
    const uint8_t* small;  // message text, font_size "small"; also setup and notice screens
    const uint8_t* large;  // message text, font_size "large"; also the IP on the welcome screen
};

struct Battery {
    bool external;        // on USB or other external power
    int percent;          // 0..100; shown only when not external
    uint32_t millivolts;  // for the serial "B" command; 0 if unknown
};

// Called once, first thing in setup() after Serial.begin(): configure pins,
// start buses, release resets, power up the panel and backlight.
void begin();

// Serial writes normally must not block when no host reads the port.
// blocking = true waits for the host, for bulk output such as screenshots.
void setSerialBlocking(bool blocking);

// One line for the boot log naming the detected hardware, for example
// "CO5300 + CST820 (V2), expander ok". Valid after begin().
const char* description();

// The panel driver, configured for rotation board::kScreenRotation so that it
// is board::kScreenWidth x kScreenHeight. Do not start it: the frame buffer
// calls begin(displaySpeedHz()) on it. Called once, after begin().
Arduino_GFX* display();
// Bus clock for the panel, or GFX_NOT_DEFINED for the driver's default.
int32_t displaySpeedHz();
Fonts fonts();

// Raw input state, true while pressed; called every loop (about every 5 ms).
// Debouncing, short and long presses, and the two-input Wi-Fi reset are
// handled in shared code. Delete: short press deletes the shown message, hold
// clears all. Scroll: shows the next message; a touchscreen can report "a
// finger is down" here.
bool deletePressed();
bool scrollPressed();

// false on boards without a battery reading; readBattery() is then never
// called and the top bar shows no battery. readBattery() is called about
// every 10 s.
bool hasBattery();
Battery readBattery();

}  // namespace hal
