// Converts UTF-8 text to what the built-in ASCII fonts can draw (PROJECT.md F-003, D-026).
#pragma once

#include <stddef.h>

namespace mcp {

// Copies text into out (null-terminated, at most outSize - 1 characters),
// keeping printable ASCII, '\n', and '\t' as a space. Common typographic
// characters are mapped (dashes to '-', curly quotes to straight quotes,
// ellipsis to "...", arrows to "->" and "<-"); any other non-ASCII character
// becomes '?'. '\r' and other control characters are dropped.
// Returns the length the result needs, which can exceed outSize - 1 when the
// text did not fit. replaced counts characters that became '?'.
size_t cleanText(const char* text, char* out, size_t outSize, size_t& replaced);

}  // namespace mcp
