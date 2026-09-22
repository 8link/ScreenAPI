// Debounced button with short and long press (PROJECT.md F-006).
// Hardware-independent: the caller passes the raw pressed state and the time.
#pragma once

#include <stdint.h>

namespace ui {

enum class ButtonEvent : uint8_t { None, Short, Long };

class ButtonTracker {
public:
    ButtonTracker(uint32_t debounceMs, uint32_t longPressMs) : debounceMs_(debounceMs), longPressMs_(longPressMs) {}

    // Short fires on release if the press was shorter than longPressMs.
    // Long fires once while still held, when the press reaches longPressMs;
    // the release after it fires nothing.
    ButtonEvent update(bool pressed, uint64_t nowMs);

private:
    uint32_t debounceMs_;
    uint32_t longPressMs_;
    bool raw_ = false;
    bool stable_ = false;
    bool longFired_ = false;
    uint64_t rawChangedAtMs_ = 0;
    uint64_t pressedAtMs_ = 0;
};

}  // namespace ui
