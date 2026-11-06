// ============================================================================
// brackets.h  -  Pure bracket-matching logic for the code editor.
//
// Finds the bracket at or immediately before the cursor and scans forward or
// backward for its partner, skipping brackets inside comments and string or
// character literals. Kept free of raylib so `bracket_tests` can test it
// directly.
// ============================================================================
#pragma once
#include <string>
#include <vector>

struct BracketMatch {
    bool active = false;    // cursor is next to a code bracket
    bool matched = false;   // matching partner was found
    int y1 = 0, x1 = 0;     // bracket at/near the cursor
    int y2 = 0, x2 = 0;     // matching partner (valid when matched == true)
};

inline bool IsOpenBracket(char c) { return c == '(' || c == '[' || c == '{'; }
inline bool IsCloseBracket(char c) { return c == ')' || c == ']' || c == '}'; }
inline bool IsBracket(char c) { return IsOpenBracket(c) || IsCloseBracket(c); }

inline char MatchingBracket(char c) {
    switch (c) {
        case '(': return ')';
        case ')': return '(';
        case '[': return ']';
        case ']': return '[';
        case '{': return '}';
        case '}': return '{';
        default: return 0;
    }
}

// Marks every character across `lines` as true when it is normal code and
// false when it lies inside a // comment, /* */ comment, "..." or '...'.
inline std::vector<std::vector<bool>> BuildCodeMask(const std::vector<std::string>& lines) {
    std::vector<std::vector<bool>> mask(lines.size());
    bool inBlockComment = false;
    for (size_t y = 0; y < lines.size(); y++) {
        const std::string& l = lines[y];
        mask[y].assign(l.size(), true);
        char quote = 0;
        for (size_t x = 0; x < l.size(); x++) {
            if (inBlockComment) {
                mask[y][x] = false;
                if (l[x] == '*' && x + 1 < l.size() && l[x + 1] == '/') {
                    mask[y][x + 1] = false;
                    x++;
                    inBlockComment = false;
                }
                continue;
            }
            if (quote) {
                mask[y][x] = false;
                if (l[x] == '\\' && x + 1 < l.size()) {
                    mask[y][x + 1] = false;
                    x++;
                } else if (l[x] == quote) {
                    quote = 0;
                }
                continue;
            }
            if (l[x] == '/' && x + 1 < l.size() && l[x + 1] == '/') {
                for (size_t k = x; k < l.size(); k++) mask[y][k] = false;
                break;
            }
            if (l[x] == '/' && x + 1 < l.size() && l[x + 1] == '*') {
                mask[y][x] = false;
                mask[y][x + 1] = false;
                x++;
                inBlockComment = true;
                continue;
            }
            if (l[x] == '"' || l[x] == '\'') {
                mask[y][x] = false;
                quote = l[x];
            }
        }
    }
    return mask;
}

inline BracketMatch FindMatchingBracket(const std::vector<std::string>& lines, int cy, int cx) {
    BracketMatch res;
    if (cy < 0 || cy >= (int)lines.size()) return res;
    auto mask = BuildCodeMask(lines);
    const std::string& cur = lines[cy];

    int bx = -1;
    if (cx >= 0 && cx < (int)cur.size() && IsBracket(cur[cx]) && mask[cy][cx]) bx = cx;
    else if (cx > 0 && cx - 1 < (int)cur.size() && IsBracket(cur[cx - 1]) && mask[cy][cx - 1]) bx = cx - 1;
    if (bx < 0) return res;

    char ch = cur[bx];
    char partner = MatchingBracket(ch);
    res.active = true;
    res.y1 = cy;
    res.x1 = bx;

    int depth = 0;
    if (IsOpenBracket(ch)) {
        for (int y = cy; y < (int)lines.size(); y++) {
            int startX = (y == cy) ? bx : 0;
            for (int x = startX; x < (int)lines[y].size(); x++) {
                if (!mask[y][x]) continue;
                char c = lines[y][x];
                if (c == ch) depth++;
                else if (c == partner) {
                    depth--;
                    if (depth == 0) {
                        res.matched = true;
                        res.y2 = y;
                        res.x2 = x;
                        return res;
                    }
                }
            }
        }
    } else {
        for (int y = cy; y >= 0; y--) {
            int startX = (y == cy) ? bx : (int)lines[y].size() - 1;
            for (int x = startX; x >= 0; x--) {
                if (!mask[y][x]) continue;
                char c = lines[y][x];
                if (c == ch) depth++;
                else if (c == partner) {
                    depth--;
                    if (depth == 0) {
                        res.matched = true;
                        res.y2 = y;
                        res.x2 = x;
                        return res;
                    }
                }
            }
        }
    }
    return res;
}
