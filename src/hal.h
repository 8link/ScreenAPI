// Board hardware layer (PROJECT.md D-032). Each board implements it in
// src/boards/<board>/hal.cpp; platformio.ini compiles only that board's folder.
#pragma once

#include <Arduino_GFX_Library.h>
#include <stdint.h>

namespace hal {

// u8g2 fonts for the four text roles on screen.
struct Fonts {
    const uint8_t* bar;
    const uint8_t* title;
    const uint8_t* small;
    const uint8_t* large;
};

struct Battery {
    bool external;       // on USB power
    int percent;         // 0..100; meaningful when not external
    uint32_t millivolts;
};

// Powers up the board: pins, buses, expanders, power chip, touch. Call first in setup().
void begin();

// Serial writes normally never block (output is dropped when no host reads).
// Blocking mode waits for the host, for bulk output such as screenshots.
void setSerialBlocking(bool blocking);

// One line for the boot log naming the detected hardware.
const char* description();

// The panel, rotated to board::kScreenWidth x kScreenHeight; the canvas starts it.
Arduino_GFX* display();
int32_t displaySpeedHz();
Fonts fonts();

// True while pressed. Delete: a button. Scroll: a button or the touchscreen.
bool deletePressed();
bool scrollPressed();

// false on boards without battery measurement; readBattery() is then not called.
bool hasBattery();
Battery readBattery();

}  // namespace hal
