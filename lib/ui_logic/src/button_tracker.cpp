#include "button_tracker.h"

namespace ui {

ButtonEvent ButtonTracker::update(bool pressed, uint64_t nowMs)
{
    if (pressed != raw_) {
        raw_ = pressed;
        rawChangedAtMs_ = nowMs;
    }

    if (raw_ != stable_ && nowMs - rawChangedAtMs_ >= debounceMs_) {
        stable_ = raw_;
        if (stable_) {
            pressedAtMs_ = nowMs;
            longFired_ = false;
        } else if (!longFired_) {
            return ButtonEvent::Short;
        }
    }

    if (stable_ && !longFired_ && nowMs - pressedAtMs_ >= longPressMs_) {
        longFired_ = true;
        return ButtonEvent::Long;
    }
    return ButtonEvent::None;
}

}  // namespace ui
