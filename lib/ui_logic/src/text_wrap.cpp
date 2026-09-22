#include "text_wrap.h"

#include <string.h>

namespace ui {

namespace {

size_t skipSpaces(const char* text, size_t pos, size_t end)
{
    while (pos < end && text[pos] == ' ') {
        pos++;
    }
    return pos;
}

// Returns the end of the longest run starting at start that fits, breaking at spaces.
// Falls back to breaking inside the first word; always takes at least one character.
size_t fitLine(const char* text, size_t start, size_t end, int maxWidth, MeasureFn measure, void* context)
{
    size_t fitEnd = start;
    size_t pos = start;
    while (pos < end) {
        size_t wordEnd = pos;
        while (wordEnd < end && text[wordEnd] != ' ') {
            wordEnd++;
        }
        if (measure(text + start, wordEnd - start, context) > maxWidth) {
            break;
        }
        fitEnd = wordEnd;
        pos = skipSpaces(text, wordEnd, end);
    }
    if (fitEnd > start) {
        return fitEnd;
    }

    fitEnd = start + 1;
    while (fitEnd < end && measure(text + start, fitEnd + 1 - start, context) <= maxWidth) {
        fitEnd++;
    }
    return fitEnd;
}

}  // namespace

size_t wrapText(const char* text, int maxWidth, MeasureFn measure, void* context, Line* lines, size_t maxLines)
{
    const size_t length = strlen(text);
    size_t count = 0;
    size_t pos = 0;

    while (count < maxLines) {
        size_t hardEnd = pos;
        while (hardEnd < length && text[hardEnd] != '\n') {
            hardEnd++;
        }

        if (pos == hardEnd) {
            lines[count++] = Line{static_cast<uint16_t>(pos), 0};
        }
        size_t start = pos;
        while (start < hardEnd && count < maxLines) {
            const size_t end = fitLine(text, start, hardEnd, maxWidth, measure, context);
            lines[count++] = Line{static_cast<uint16_t>(start), static_cast<uint16_t>(end - start)};
            start = skipSpaces(text, end, hardEnd);
        }

        if (hardEnd >= length) {
            break;
        }
        pos = hardEnd + 1;
    }
    return count;
}

}  // namespace ui
