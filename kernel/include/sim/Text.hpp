#pragma once
// Text.hpp — shared string-normalization helpers (header-only, no state).
// Replaces five private trim/lowercase copies (Actions.cpp, Dialogue.cpp,
// Population.cpp, Justice.cpp, Schedule.cpp) with one definition; the
// whitespace set (space/tab/CR/LF/FF/VT) matches Justice.cpp's original,
// the strictest of the five, so switching callers over changes no behaviour.
#include <string>

namespace sim {

inline std::string trim(const std::string& s) {
    const auto is_space = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
    };
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && is_space(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && is_space(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

inline std::string ascii_lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

// trim + lowercase in one call — the common "normalize for comparison" case.
inline std::string normalize_key(const std::string& s) { return ascii_lower(trim(s)); }

}  // namespace sim
