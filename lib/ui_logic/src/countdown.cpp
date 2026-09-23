#include "countdown.h"

#include <stdio.h>

namespace ui {

uint32_t countdownSeconds(uint32_t remainingMs)
{
    return remainingMs / 1000 + (remainingMs % 1000 != 0 ? 1 : 0);
}

void formatCountdown(uint32_t seconds, char* out, size_t size)
{
    const unsigned h = seconds / 3600;
    const unsigned m = seconds / 60 % 60;
    const unsigned s = seconds % 60;
    if (seconds < 60) {
        snprintf(out, size, "%us", s);
    } else if (seconds < 3600) {
        snprintf(out, size, "%u:%02u", m, s);
    } else {
        snprintf(out, size, "%u:%02u:%02u", h, m, s);
    }
}

}  // namespace ui
