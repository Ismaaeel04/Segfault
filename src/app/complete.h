// ============================================================================
// complete.h  -  Autocompletion for the code editor, like the editors on
// HackerRank and LeetCode: the word being typed is completed from C/C++
// keywords and types, common library functions, and the names already used in
// the student's own code. Nothing is suggested inside comments or strings.
// No raylib here, so `complete_tests` can test it directly.
// ============================================================================
#pragma once
#include "brackets.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

namespace complete {

enum Kind { KEYWORD, TYPE, FUNCTION, VARIABLE, LIBRARY };

struct Item {
    std::string word;
    Kind kind;
};

inline bool IsWord(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

// Where the word that ends at the cursor starts (== cx when there is none).
inline int WordStart(const std::string& line, int cx) {
    int s = std::clamp(cx, 0, (int)line.size());
    while (s > 0 && IsWord(line[s - 1])) s--;
    return s;
}

inline const std::vector<std::string>& Keywords() {
    static const std::vector<std::string> k = {
        "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue", "return",
        "sizeof", "struct", "class", "public", "private", "protected", "new", "delete", "const", "static",
        "typedef", "enum", "union", "namespace", "using", "template", "typename", "this", "nullptr",
        "true", "false", "NULL", "include", "define", "ifndef", "endif"};
    return k;
}
inline const std::vector<std::string>& Types() {
    static const std::vector<std::string> t = {
        "int", "char", "bool", "float", "double", "long", "short", "unsigned", "signed", "void",
        "size_t", "string", "vector", "auto"};
    return t;
}
inline const std::vector<std::string>& Library() {
    static const std::vector<std::string> l = {
        "printf", "scanf", "puts", "putchar", "getchar", "malloc", "calloc", "realloc", "free",
        "strlen", "strcpy", "strncpy", "strcat", "strncat", "strcmp", "strncmp", "strchr", "strstr",
        "memset", "memcpy", "memmove", "abs", "fabs", "sqrt", "pow", "toupper", "tolower",
        "isdigit", "isalpha", "isspace", "atoi", "cout", "cin", "endl"};
    return l;
}

inline const char* KindName(Kind k) {
    switch (k) {
        case KEYWORD: return "keyword";
        case TYPE: return "type";
        case FUNCTION: return "function";
        case VARIABLE: return "variable";
        default: return "library";
    }
}

// Suggestions for the word ending at (cy, cx), best first, at most `max`.
// force (Ctrl+Space): also when nothing has been typed yet.
inline std::vector<Item> Suggest(const std::vector<std::string>& lines, int cy, int cx, int max = 8, bool force = false) {
    std::vector<Item> out;
    if (cy < 0 || cy >= (int)lines.size()) return out;
    const std::string& line = lines[cy];
    cx = std::clamp(cx, 0, (int)line.size());
    int start = WordStart(line, cx);
    std::string prefix = line.substr(start, cx - start);
    if (prefix.empty() && !force) return out;
    if (!prefix.empty() && std::isdigit((unsigned char)prefix[0])) return out;   // a number
    if (cx < (int)line.size() && IsWord(line[cx]) && !force) return out;        // in the middle of a word
    auto mask = BuildCodeMask(lines);
    int probe = start > 0 ? start - 1 : start;                                  // inside a comment or string?
    if (probe < (int)mask[cy].size() && !mask[cy][probe] && !(start == 0 && prefix.empty())) return out;
    if (start < (int)mask[cy].size() && !mask[cy][start]) return out;

    struct Cand { Kind kind; int score; };
    std::map<std::string, Cand> cands;
    auto add = [&](const std::string& w, Kind k, int score) {
        auto it = cands.find(w);
        if (it == cands.end()) cands[w] = {k, score};
        else it->second.score += score;
    };
    // After -> or . comes a member of a struct, never a keyword: only the
    // student's own names fit there.
    bool member = (start >= 2 && line.compare(start - 2, 2, "->") == 0) || (start >= 1 && line[start - 1] == '.');
    if (!member) {
        for (const std::string& w : Keywords()) add(w, KEYWORD, 20);
        for (const std::string& w : Types()) add(w, TYPE, 22);
        for (const std::string& w : Library()) add(w, LIBRARY, 12);
    }

    // names in the student's code (not the one being typed right now), and
    // which of them were used as members (after -> or .)
    std::map<std::string, bool> usedAsMember;
    for (int y = 0; y < (int)lines.size(); y++) {
        const std::string& l = lines[y];
        for (int x = 0; x < (int)l.size();) {
            if (!IsWord(l[x]) || !mask[y][x]) { x++; continue; }
            int b = x;
            while (x < (int)l.size() && IsWord(l[x])) x++;
            if (std::isdigit((unsigned char)l[b]) || x - b < 2) continue;
            if (y == cy && b == start) continue;                    // the word under the cursor
            std::string w = l.substr(b, x - b);
            if (member && (std::find(Keywords().begin(), Keywords().end(), w) != Keywords().end() ||
                           std::find(Types().begin(), Types().end(), w) != Types().end())) continue;
            if ((b >= 2 && l.compare(b - 2, 2, "->") == 0) || (b >= 1 && l[b - 1] == '.')) usedAsMember[w] = true;
            int n = x;
            while (n < (int)l.size() && l[n] == ' ') n++;
            bool call = n < (int)l.size() && l[n] == '(';
            auto it = cands.find(w);
            if (it == cands.end()) cands[w] = {call ? FUNCTION : VARIABLE, 30};
            else if (it->second.kind == VARIABLE || it->second.kind == FUNCTION) {
                if (call) it->second.kind = FUNCTION;
                if (it->second.score < 40) it->second.score += 2;     // used more often: a little higher
            } else if (it->second.score < 40) it->second.score += 15;  // a library name the student uses
        }
    }

    std::string lowPrefix = prefix;
    for (char& c : lowPrefix) c = (char)std::tolower((unsigned char)c);
    struct Ranked { std::string word; Kind kind; int score; };
    std::vector<Ranked> ranked;
    for (const auto& c : cands) {
        const std::string& w = c.first;
        if (w == prefix || w.size() <= prefix.size()) continue;   // already typed in full
        int score = c.second.score;
        if (member && usedAsMember.count(w)) score += 25;             // temp->ne... : next, not newNode
        if (w.compare(0, prefix.size(), prefix) == 0) score += 100;   // same case
        else {
            std::string lw = w;
            for (char& ch : lw) ch = (char)std::tolower((unsigned char)ch);
            if (lw.compare(0, lowPrefix.size(), lowPrefix) != 0) continue;
        }
        ranked.push_back({w, c.second.kind, score});
    }
    std::sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.word.size() != b.word.size()) return a.word.size() < b.word.size();
        return a.word < b.word;
    });
    for (const Ranked& r : ranked) {
        if ((int)out.size() >= max) break;
        out.push_back({r.word, r.kind});
    }
    return out;
}

} // namespace complete
