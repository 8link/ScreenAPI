// Board description for the Waveshare ESP32-S3-Touch-AMOLED-1.8 (BOARDS.md).
// Hardware layer: src/boards/waveshare_amoled18/hal.cpp (PROJECT.md D-030, D-032).
#pragma once

#include <stdint.h>

#include "pins.h"

namespace board {

constexpr const char* kName = "Waveshare ESP32-S3-Touch-AMOLED-1.8";

// Screen size after rotation: portrait, the panel's native orientation. The
// 8-bit frame buffer (164,864 bytes) goes to PSRAM.
constexpr int kScreenWidth = 368;
constexpr int kScreenHeight = 448;
constexpr uint8_t kScreenRotation = 0;
// Rounded display corners: how far from a corner, along the diagonal, content
// is fully visible. Not documented by Waveshare; measured with the corner test
// (serial command C, PROJECT.md D-036): the square at 24 px is the first one
// complete in all four corners. The outermost pixel row and column on every
// edge are not visible either.
constexpr int kCornerInset = 24;

// Name for mDNS (<hostname>.local) and the MCP URL shown on screen. The same on
// every board (D-033), so clients keep one URL whichever board is online.
constexpr const char* kHostname = "mcpvue";

}  // namespace board
