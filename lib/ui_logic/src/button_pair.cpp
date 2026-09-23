#include "button_pair.h"

namespace ui {

PairEvent ButtonPair::update(bool firstPressed, bool secondPressed, uint64_t nowMs)
{
    const ButtonEvent first = first_.update(firstPressed, nowMs);
    const ButtonEvent second = second_.update(secondPressed, nowMs);

    if (firstPressed && secondPressed) {
        if (!bothHeld_) {
            bothHeld_ = true;
            bothSinceMs_ = nowMs;
        }
        combo_ = true;
        if (!bothFired_ && nowMs - bothSinceMs_ >= bothMs_) {
            bothFired_ = true;
            return PairEvent::BothLong;
        }
        return PairEvent::None;
    }
    bothHeld_ = false;

    if (combo_) {
        // Swallow events, including the release that settles in this call.
        if (first_.idle() && second_.idle()) {
            combo_ = false;
            bothFired_ = false;
        }
        return PairEvent::None;
    }

    if (first == ButtonEvent::Short) {
        return PairEvent::FirstShort;
    }
    if (first == ButtonEvent::Long) {
        return PairEvent::FirstLong;
    }
    if (second == ButtonEvent::Short) {
        return PairEvent::SecondShort;
    }
    if (second == ButtonEvent::Long) {
        return PairEvent::SecondLong;
    }
    return PairEvent::None;
}

}  // namespace ui
