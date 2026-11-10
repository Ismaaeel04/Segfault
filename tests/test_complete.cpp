// ============================================================================
// test_complete.cpp  -  Console unit tests for the editor's autocompletion.
// Build target: complete_tests   (run it, or use `ctest` in the build folder)
// ============================================================================
#include <cstdio>
#include <string>
#include <vector>
#include "../src/app/complete.h"

using namespace complete;

static int passed = 0, failed = 0;

#define CHECK(cond) do { if (cond) passed++; else { failed++; \
    printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static bool Has(const std::vector<Item>& items, const std::string& w) {
    for (const Item& i : items) if (i.word == w) return true;
    return false;
}
static int Pos(const std::vector<Item>& items, const std::string& w) {
    for (size_t i = 0; i < items.size(); i++) if (items[i].word == w) return (int)i;
    return -1;
}
// cursor at the end of the last line
static std::vector<Item> At(const std::vector<std::string>& lines, int max = 8, bool force = false) {
    return Suggest(lines, (int)lines.size() - 1, (int)lines.back().size(), max, force);
}

int main() {
    printf("[keywords, types, library]\n");
    {
        auto s = At({"    whi"});
        CHECK(!s.empty() && s[0].word == "while" && s[0].kind == KEYWORD);
        s = At({"    pri"});
        CHECK(Has(s, "printf") && s[Pos(s, "printf")].kind == LIBRARY);
        s = At({"unsig"});
        CHECK(!s.empty() && s[0].word == "unsigned" && s[0].kind == TYPE);
        s = At({"#incl"});
        CHECK(Has(s, "include"));
    }

    printf("[names from the student's code]\n");
    {
        std::vector<std::string> code = {
            "int countVowels(const char* text) {",
            "    int total = 0;",
            "    for (int i = 0; text[i] != '\\0'; i++) {",
            "        if (isVowel(text[i])) total++;",
            "    }",
            "    return tot"};
        auto s = At(code);
        CHECK(!s.empty() && s[0].word == "total" && s[0].kind == VARIABLE);
        code.back() = "    return cou";
        s = At(code);
        CHECK(!s.empty() && s[0].word == "countVowels" && s[0].kind == FUNCTION);
        code.back() = "    if (isV";
        s = At(code);
        CHECK(Has(s, "isVowel") && s[Pos(s, "isVowel")].kind == FUNCTION);
        code.back() = "    te";
        s = At(code);
        CHECK(Has(s, "text") && Pos(s, "text") < Pos(s, "template"));   // the student's own names first
    }

    printf("[linked list names]\n");
    {
        std::vector<std::string> code = {
            "Node* newNode = new Node(value);",
            "newNode->next = head;",
            "head = newNode;",
            "Node* temp = head;",
            "while (temp->ne"};
        auto s = At(code);
        CHECK(!s.empty() && s[0].word == "next");
        CHECK(!Has(s, "new"));                                    // after -> only members, no keywords
        code.back() = "while (temp->next != nul";
        s = At(code);
        CHECK(Has(s, "nullptr"));
    }

    printf("[case]\n");
    {
        auto s = At({"Node* head;", "nod"});
        CHECK(Has(s, "Node"));                                   // other case still found
        s = At({"int nodeCount;", "Node* head;", "Nod"});
        CHECK(!s.empty() && s[0].word == "Node");                 // same case ranks first
    }

    printf("[nothing to suggest]\n");
    {
        CHECK(At({"// whi"}).empty());                            // in a comment
        CHECK(At({"printf(\"whi"}).empty());                      // in a string
        CHECK(At({"int x = 12"}).empty());                        // a number
        CHECK(At({"int x = "}).empty());                          // nothing typed
        CHECK(At({"int counter;", "counter"}).empty() || !Has(At({"int counter;", "counter"}), "counter"));   // already typed in full
        CHECK(Suggest({"whileX"}, 0, 3).empty());                 // cursor in the middle of a word
        CHECK(At({"zzqq"}).empty());
        CHECK(Suggest({}, 0, 0).empty());
    }

    printf("[Ctrl+Space and limits]\n");
    {
        auto s = At({"int total = 0;", "    "}, 8, true);
        CHECK(!s.empty() && (int)s.size() <= 8 && Has(s, "total"));   // forced: everything, best first
        CHECK((int)At({"s"}, 3).size() <= 3);
        CHECK(WordStart("    retu", 8) == 4 && WordStart("a+b", 2) == 2 && WordStart("", 0) == 0);
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
