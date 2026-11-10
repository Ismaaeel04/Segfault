// ============================================================================
// paste.h  -  What a single-line text box keeps from pasted text. No raylib
// here, so the unit tests (paste_tests) can include it directly.
// ============================================================================
#pragma once
#include <string>

namespace ui {

// Returns `current` with the usable part of `clip` added at the end:
// - only printable characters (line breaks, tabs and other control characters go)
// - numericOnly: only what a number box accepts (digits, '-', ' ', ',')
// - trim: no spaces at either end of the pasted part, which is what usually
//   comes along by accident when copying a name or a code. Passwords are
//   pasted as they are, because a space can be part of one.
// - never longer than maxLen
inline std::string PasteInto(const std::string& current, const std::string& clip, int maxLen,
                             bool numericOnly, bool trim) {
    std::string add;
    for (char c : clip) {
        unsigned char u = (unsigned char)c;
        bool ok = u >= 32 && u < 127;
        if (numericOnly) ok = (c >= '0' && c <= '9') || c == '-' || c == ' ' || c == ',';
        if (ok) add += c;
    }
    if (trim) {
        size_t a = add.find_first_not_of(' '), b = add.find_last_not_of(' ');
        add = a == std::string::npos ? "" : add.substr(a, b - a + 1);
    }
    std::string out = current + add;
    if (maxLen >= 0 && (int)out.size() > maxLen) out.resize(maxLen > (int)current.size() ? maxLen : current.size());
    return out;
}

} // namespace ui
