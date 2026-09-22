#include "scroll_offset.h"

namespace ui {

int scrollOffset(int contentSize, int viewportSize, uint64_t elapsedMs, uint32_t pixelsPerSecond, uint32_t pauseMs)
{
    if (contentSize <= viewportSize || pixelsPerSecond == 0) {
        return 0;
    }
    const uint64_t distance = static_cast<uint64_t>(contentSize - viewportSize);
    const uint64_t moveMs = distance * 1000 / pixelsPerSecond;
    const uint64_t cycleMs = pauseMs + moveMs + pauseMs;

    uint64_t t = elapsedMs % cycleMs;
    if (t < pauseMs) {
        return 0;
    }
    t -= pauseMs;
    if (t < moveMs) {
        return static_cast<int>(t * pixelsPerSecond / 1000);
    }
    return static_cast<int>(distance);
}

}  // namespace ui
