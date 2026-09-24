// Screen saver timing (PROJECT.md F-014). Hardware-independent: the caller
// passes the time, whether the screen is idle, and whether an input is pressed.
#pragma once

#include <stdint.h>

namespace ui {

enum class SaverPhase : uint8_t {
    Awake,      // normal screen
    Dark,       // panel off
    Animating,  // panel on, particle animation
};

class ScreenSaver {
public:
    // idleMs: idle time before the panel goes off. periodMs: time from one
    // animation start to the next, the first one periodMs after going dark.
    // animationMs: length of one animation.
    ScreenSaver(uint32_t idleMs, uint32_t periodMs, uint32_t animationMs)
        : idleMs_(idleMs), periodMs_(periodMs), animationMs_(animationMs)
    {
    }

    // idle: nothing to show (empty queue, no cover screen). pressed: an input
    // is down; it keeps the screen awake and restarts the idle time.
    SaverPhase update(uint64_t nowMs, bool idle, bool pressed);

    SaverPhase phase() const { return phase_; }
    // Start of the current animation; valid while Animating.
    uint64_t animationStartMs() const { return animationStartMs_; }

private:
    uint32_t idleMs_;
    uint32_t periodMs_;
    uint32_t animationMs_;
    SaverPhase phase_ = SaverPhase::Awake;
    bool idleKnown_ = false;
    uint64_t idleSinceMs_ = 0;
    uint64_t nextAnimationMs_ = 0;
    uint64_t animationStartMs_ = 0;
};

}  // namespace ui
