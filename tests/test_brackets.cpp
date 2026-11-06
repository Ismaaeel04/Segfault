// ============================================================================
// test_brackets.cpp  -  Console unit tests for editor bracket matching.
// Build target: bracket_tests   (run it, or use `ctest` in the build folder)
// ============================================================================
#include <cstdio>
#include <string>
#include <vector>
#include "../src/app/brackets.h"

static int passed = 0, failed = 0;

#define CHECK(cond) do { if (cond) passed++; else { failed++; \
    printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

int main() {
    printf("[single-line and nested brackets]\n");
    {
        std::vector<std::string> lines = {"int a = fn(x[0], (y + 1));"};
        // cursor on '(' of fn(
        BracketMatch m = FindMatchingBracket(lines, 0, 10);
        CHECK(m.active && m.matched);
        CHECK(m.y1 == 0 && m.x1 == 10);
        CHECK(m.y2 == 0 && m.x2 == 24);

        // cursor right after ')' at index 25 finds the closing paren at 24
        BracketMatch back = FindMatchingBracket(lines, 0, 25);
        CHECK(back.active && back.matched);
        CHECK(back.x1 == 24 && back.x2 == 10);

        // square brackets x[0]
        BracketMatch sq = FindMatchingBracket(lines, 0, 12);
        CHECK(sq.active && sq.matched);
        CHECK(sq.x1 == 12 && sq.x2 == 14);
    }

    printf("[multi-line braces]\n");
    {
        std::vector<std::string> lines = {
            "void f() {",
            "    if (ok) {",
            "        work();",
            "    }",
            "}"
        };
        BracketMatch outer = FindMatchingBracket(lines, 0, 9);
        CHECK(outer.active && outer.matched);
        CHECK(outer.y2 == 4 && outer.x2 == 0);

        BracketMatch inner = FindMatchingBracket(lines, 3, 4);
        CHECK(inner.active && inner.matched);
        CHECK(inner.y2 == 1 && inner.x2 == 12);
    }

    printf("[ignores brackets inside strings, chars and comments]\n");
    {
        std::vector<std::string> lines = {
            "void g() { // unmatched { in line comment",
            "    const char* s = \"fake } and ( here\";",
            "    char c = '}'; /* block { comment */",
            "}"
        };
        BracketMatch m = FindMatchingBracket(lines, 0, 9);
        CHECK(m.active && m.matched);
        CHECK(m.y2 == 3 && m.x2 == 0);

        // cursor sitting on a bracket inside a string literal should not activate
        BracketMatch inStr = FindMatchingBracket(lines, 1, 26);
        CHECK(!inStr.active);
    }

    printf("[unmatched brackets]\n");
    {
        std::vector<std::string> lines = {"int x = (1 + 2;"};
        BracketMatch m = FindMatchingBracket(lines, 0, 8);
        CHECK(m.active && !m.matched);
        CHECK(m.y1 == 0 && m.x1 == 8);
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
