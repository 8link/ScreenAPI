#include "battery_level.h"

namespace ui {

namespace {

struct Point {
    uint32_t millivolts;
    int percent;
};

// Typical 3.7 V Li-ion curve at light load, highest voltage first.
const Point kCurve[] = {
    {4200, 100}, {4100, 90}, {4000, 78}, {3900, 62}, {3800, 42}, {3700, 20}, {3600, 8}, {3500, 3}, {3300, 0},
};
constexpr int kPoints = sizeof(kCurve) / sizeof(kCurve[0]);

}  // namespace

BatteryLevel batteryLevel(uint32_t millivolts)
{
    if (millivolts >= kExternalPowerMv) {
        return BatteryLevel{true, 100};
    }
    if (millivolts >= kCurve[0].millivolts) {
        return BatteryLevel{false, 100};
    }
    for (int i = 1; i < kPoints; i++) {
        const Point& high = kCurve[i - 1];
        const Point& low = kCurve[i];
        if (millivolts >= low.millivolts) {
            const int span = static_cast<int>(high.millivolts - low.millivolts);
            const int above = static_cast<int>(millivolts - low.millivolts);
            return BatteryLevel{false, low.percent + (high.percent - low.percent) * above / span};
        }
    }
    return BatteryLevel{false, 0};
}

}  // namespace ui
