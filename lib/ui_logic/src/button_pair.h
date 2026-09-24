// Two buttons with single-button presses and a both-held action (PROJECT.md F-006, F-002).
// Hardware-independent: the caller passes the raw pressed states and the time.
#pragma once

#include <stdint.h>

#include "button_tracker.h"

namespace ui {

enum class PairEvent : uint8_t { None, FirstShort, FirstLong, SecondShort, SecondLong, BothLong };

class ButtonPair {
public:
    ButtonPair(uint32_t debounceMs, uint32_t longPressMs, uint32_t bothMs)
        : first_(debounceMs, longPressMs), second_(debounceMs, longPressMs), bothMs_(bothMs)
    {
    }

    // BothLong fires once when both buttons have been held together for bothMs.
    // From the moment both are down until both are released and settled, no
    // single-button events are reported, so a two-button hold never deletes,
    // clears, or scrolls.
    PairEvent update(bool firstPressed, bool secondPressed, uint64_t nowMs);

    // True when both buttons are released and settled. A short press is
    // reported by the update() call after which idle() turns true.
    bool idle() const { return first_.idle() && second_.idle(); }

private:
    ButtonTracker first_;
    ButtonTracker second_;
    uint32_t bothMs_;
    bool combo_ = false;
    bool bothHeld_ = false;
    bool bothFired_ = false;
    uint64_t bothSinceMs_ = 0;
};

}  // namespace ui
