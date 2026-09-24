// Template board (PROJECT.md "Adding a board"): a starting point for porting
// ScreenAPI to another ESP32 with Wi-Fi and a display. Copy this folder and
// src/boards/template/ to <your_board>, then change every value marked PORT.
// The "template" env builds it for a generic ESP32 so the template stays
// compilable; it is not meant to be flashed as is.
#pragma once

#include <stdint.h>

#include "pins.h"

namespace board {

constexpr const char* kName = "Template board";  // PORT: your board's name, printed at boot

// PORT: the screen size after rotation, and the rotation that gives it.
// At least 240 x 135. The frame buffer takes width x height bytes.
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 240;
constexpr uint8_t kScreenRotation = 1;  // landscape

// PORT: a name unique on your network; the MCP URL is http://<hostname>.local/mcp.
constexpr const char* kHostname = "screenapi-template";

}  // namespace board
