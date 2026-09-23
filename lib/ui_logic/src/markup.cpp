#include "markup.h"

#include <string.h>

namespace ui {

namespace {

// Returns the length of the tag at markup (including braces) and sets color,
// or returns 0 if markup does not start with a known tag.
size_t matchTag(const char* markup, uint8_t baseColor, const char* const* colorNames, size_t colorCount,
                uint8_t& color)
{
    if (strncmp(markup, "{/}", 3) == 0) {
        color = baseColor;
        return 3;
    }
    for (size_t i = 0; i < colorCount; i++) {
        const size_t nameLength = strlen(colorNames[i]);
        if (strncmp(markup + 1, colorNames[i], nameLength) == 0 && markup[nameLength + 1] == '}') {
            color = static_cast<uint8_t>(i);
            return nameLength + 2;
        }
    }
    return 0;
}

}  // namespace

size_t parseMarkup(const char* markup, uint8_t baseColor, const char* const* colorNames, size_t colorCount,
                   char* text, uint8_t* colors)
{
    uint8_t color = baseColor;
    size_t length = 0;
    size_t pos = 0;
    while (markup[pos] != '\0') {
        if (markup[pos] == '{') {
            const size_t tagLength = matchTag(markup + pos, baseColor, colorNames, colorCount, color);
            if (tagLength > 0) {
                pos += tagLength;
                continue;
            }
        }
        text[length] = markup[pos];
        colors[length] = color;
        length++;
        pos++;
    }
    text[length] = '\0';
    return length;
}

}  // namespace ui
