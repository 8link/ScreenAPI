// Board description for the LilyGO TTGO T-Display (BOARDS.md). Each board has
// its own folder under include/boards/ with board.h and pins.h, and its hardware
// layer in src/boards/<board>/hal.cpp (PROJECT.md D-030, D-032).
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
// Rounded display corners in px (0: square). Content near the corners moves
// in accordingly (PROJECT.md D-036).
constexpr int kCornerRadius = 0;

// Name for mDNS (<hostname>.local) and the MCP URL shown on screen. The same on
// every board (D-033), so clients keep one URL whichever board is online.
constexpr const char* kHostname = "screenapi";

}  // namespace board
