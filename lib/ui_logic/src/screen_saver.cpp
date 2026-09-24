#include "screen_saver.h"

namespace ui {

SaverPhase ScreenSaver::update(uint64_t nowMs, bool idle, bool pressed)
{
    if (!idle || pressed) {
        phase_ = SaverPhase::Awake;
        idleKnown_ = false;
        return phase_;
    }
    if (!idleKnown_) {
        idleKnown_ = true;
        idleSinceMs_ = nowMs;
    }
    switch (phase_) {
    case SaverPhase::Awake:
        if (nowMs - idleSinceMs_ >= idleMs_) {
            phase_ = SaverPhase::Dark;
            nextAnimationMs_ = nowMs + periodMs_;
        }
        break;
    case SaverPhase::Dark:
        if (nowMs >= nextAnimationMs_) {
            phase_ = SaverPhase::Animating;
            animationStartMs_ = nowMs;
        }
        break;
    case SaverPhase::Animating:
        if (nowMs - animationStartMs_ >= animationMs_) {
            phase_ = SaverPhase::Dark;
            nextAnimationMs_ = animationStartMs_ + periodMs_;
        }
        break;
    }
    return phase_;
}

}  // namespace ui
