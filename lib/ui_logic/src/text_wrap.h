// Word wrap for the message value (PROJECT.md F-005).
// Hardware-independent: text width comes from a caller-supplied function,
// so the same logic runs with the display font or in native tests.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ui {

struct Line {
    uint16_t start;
    uint16_t length;
};

// Returns the pixel width of text[0..length).
using MeasureFn = int (*)(const char* text, size_t length, void* context);

// Splits text into lines no wider than maxWidth. Breaks at spaces, and inside
// a word only when the word alone is too wide. '\n' forces a line break.
// Spaces at a wrap point are dropped. Returns the number of lines written,
// at most maxLines.
size_t wrapText(const char* text, int maxWidth, MeasureFn measure, void* context, Line* lines, size_t maxLines);

}  // namespace ui
