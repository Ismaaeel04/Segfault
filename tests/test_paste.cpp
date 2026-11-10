// ============================================================================
// test_paste.cpp  -  Console unit tests for pasting into a text box.
// Build target: paste_tests   (run it, or use `ctest` in the build folder)
// ============================================================================
#include <cstdio>
#include <string>
#include "../src/app/paste.h"

using ui::PasteInto;

static int passed = 0, failed = 0;

#define CHECK(cond) do { if (cond) passed++; else { failed++; \
    printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

int main() {
    printf("[names and codes]\n");
    CHECK(PasteInto("", "maria_s", 24, false, true) == "maria_s");
    CHECK(PasteInto("", "  maria_s \r\n", 24, false, true) == "maria_s");          // copied with a line break
    CHECK(PasteInto("", "VG3P-5FTE-SWCP\n", 20, false, true) == "VG3P-5FTE-SWCP");
    CHECK(PasteInto("ab", "cd", 24, false, true) == "abcd");                         // added at the end
    CHECK(PasteInto("", "a\tb\x01" "c", 24, false, true) == "abc");                  // control characters dropped
    CHECK(PasteInto("", "ma\xc3\xb1" "ana", 24, false, true) == "maana");            // non-ASCII dropped, like typing
    CHECK(PasteInto("", "   ", 24, false, true) == "");

    printf("[passwords keep their spaces]\n");
    CHECK(PasteInto("", " correct horse ", 64, false, false) == " correct horse ");
    CHECK(PasteInto("", "secret\r\n", 64, false, false) == "secret");

    printf("[length limit]\n");
    CHECK(PasteInto("", "abcdefghij", 4, false, true) == "abcd");
    CHECK(PasteInto("abc", "defgh", 5, false, true) == "abcde");
    CHECK(PasteInto("abcdef", "x", 5, false, true) == "abcdef");                     // never cuts what was typed

    printf("[number boxes]\n");
    CHECK(PasteInto("", "42", 5, true, true) == "42");
    CHECK(PasteInto("", "x=-17;", 5, true, true) == "-17");
    CHECK(PasteInto("", "1, 2, 3", 10, true, true) == "1, 2, 3");
    CHECK(PasteInto("", "abc", 5, true, true) == "");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
