// Board description for the LilyGO TTGO T-Display (BOARDS.md). Each board has
// its own folder under include/boards/ with board.h, pins.h, and display.h;
// the PlatformIO env puts that folder on the include path (PROJECT.md D-030).
#pragma once

#include <stdint.h>

#include "pins.h"

namespace board {

constexpr const char* kName = "LilyGO TTGO T-Display";

// Screen size after rotation. The layout in src/screen.cpp adapts to it; the
// 8-bit frame buffer takes width x height bytes (no PSRAM on this board).
constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr uint8_t kScreenRotation = 1;  // landscape

// Battery voltage sense (PIN_BATTERY_ADC, PIN_ADC_EN in pins.h).
constexpr bool kHasBatterySense = true;

}  // namespace board
