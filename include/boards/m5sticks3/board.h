// Board description for the M5Stack M5StickS3 (BOARDS.md). Each board has its
// own folder under include/boards/ with board.h and pins.h, and its hardware
// layer in src/boards/<board>/hal.cpp (PROJECT.md D-030, D-032).
#pragma once

#include <stdint.h>

#include "pins.h"

namespace board {

constexpr const char* kName = "M5Stack M5StickS3";

// Screen size after rotation. The layout in src/screen.cpp adapts to it; the
// 8-bit frame buffer takes width x height bytes (in PSRAM).
constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr uint8_t kScreenRotation = 1;  // landscape
// Rounded or covered display corners: how far from a corner, along the
// diagonal, content is fully visible; 0 for square panels. Measure it with the
// corner test (serial command C, PROJECT.md D-036).
constexpr int kCornerInset = 0;

// Name for mDNS (<hostname>.local) and the MCP URL shown on screen. The same on
// every board (D-033), so clients keep one URL whichever board is online.
constexpr const char* kHostname = "mcpvue";

}  // namespace board
