#include "text_clean.h"

#include <stdint.h>
#include <string.h>

namespace mcp {

namespace {

struct Mapping {
    uint32_t codePoint;
    const char* ascii;
};

// Characters LLM output often contains, mapped to the closest ASCII.
const Mapping kMappings[] = {
    {0x00A0, " "},   {0x2002, " "},   {0x2003, " "},   {0x2009, " "},   {0x2010, "-"},  {0x2011, "-"},
    {0x2012, "-"},   {0x2013, "-"},   {0x2014, "-"},   {0x2015, "-"},   {0x2212, "-"},  {0x2018, "'"},
    {0x2019, "'"},   {0x201A, "'"},   {0x201C, "\""},  {0x201D, "\""},  {0x201E, "\""}, {0x2026, "..."},
    {0x2022, "*"},   {0x00B7, "*"},   {0x2192, "->"},  {0x2190, "<-"},  {0x21D2, "=>"}, {0x00D7, "x"},
    {0x2713, "v"},   {0x2714, "v"},   {0x2717, "x"},   {0x2718, "x"},   {0x00B0, "deg"},
};

// Decodes one UTF-8 sequence at text; returns its length (at least 1).
// Invalid bytes decode as a single unknown character.
size_t decodeUtf8(const unsigned char* text, uint32_t& codePoint)
{
    const unsigned char c = text[0];
    size_t length = 0;
    if (c >= 0xF0 && c <= 0xF4) {
        length = 4;
        codePoint = c & 0x07;
    } else if (c >= 0xE0) {
        length = 3;
        codePoint = c & 0x0F;
    } else if (c >= 0xC2 && c <= 0xDF) {
        length = 2;
        codePoint = c & 0x1F;
    } else {
        codePoint = 0xFFFD;
        return 1;
    }
    for (size_t i = 1; i < length; i++) {
        if ((text[i] & 0xC0) != 0x80) {
            codePoint = 0xFFFD;
            return i;
        }
        codePoint = (codePoint << 6) | (text[i] & 0x3F);
    }
    return length;
}

const char* mapCodePoint(uint32_t codePoint)
{
    for (const Mapping& mapping : kMappings) {
        if (mapping.codePoint == codePoint) {
            return mapping.ascii;
        }
    }
    return nullptr;
}

}  // namespace

size_t cleanText(const char* text, char* out, size_t outSize, size_t& replaced)
{
    replaced = 0;
    size_t needed = 0;
    auto append = [&](const char* s) {
        for (; *s != '\0'; s++) {
            if (needed + 1 < outSize) {
                out[needed] = *s;
            }
            needed++;
        }
    };

    const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
    while (*p != '\0') {
        if (*p < 0x80) {
            if (*p == '\n' || (*p >= 0x20 && *p < 0x7F)) {
                const char c[2] = {static_cast<char>(*p), '\0'};
                append(c);
            } else if (*p == '\t') {
                append(" ");
            }
            p++;
            continue;
        }
        uint32_t codePoint = 0;
        p += decodeUtf8(p, codePoint);
        const char* ascii = mapCodePoint(codePoint);
        if (ascii == nullptr) {
            ascii = "?";
            replaced++;
        }
        append(ascii);
    }
    if (outSize > 0) {
        out[needed < outSize ? needed : outSize - 1] = '\0';
    }
    return needed;
}

}  // namespace mcp
