// Battery level for the top bar (PROJECT.md F-007, D-028).
#pragma once

#include <stdint.h>

namespace ui {

// At or above this the sense line sees the USB supply rather than a cell
// (a Li-ion cell tops out at 4.2 V; 4,765 mV was measured on USB, BOARDS.md Power).
constexpr uint32_t kExternalPowerMv = 4400;

struct BatteryLevel {
    bool external;  // on USB power
    int percent;    // 0..100; only meaningful when not external
};

// Maps a measured voltage to a level using a typical single-cell Li-ion
// discharge curve, interpolated linearly. The curve is an approximation.
BatteryLevel batteryLevel(uint32_t millivolts);

}  // namespace ui
