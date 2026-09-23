// Inline color markup for the message value (PROJECT.md F-005, D-021).
// "{green}ok{/} rest": {name} switches to a named color, {/} returns to the
// base color. Anything else in braces is shown as written.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ui {

// Copies the visible text of markup into text (null-terminated) and the color
// index of each visible character into colors. colorNames lists the tag names
// in index order. text and colors need room for strlen(markup) + 1 entries.
// Returns the visible length.
size_t parseMarkup(const char* markup, uint8_t baseColor, const char* const* colorNames, size_t colorCount,
                   char* text, uint8_t* colors);

}  // namespace ui
