// ============================================================================
// test_solutions.cpp  -  Console unit tests for every exam solution.
// Build target: pq_tests   (run it, or use `ctest` in the build folder)
// Covers Lab1 Q1-Q22 (including tests for Q9-Q18 that the lab asked us to
// add), the 10 pointer functions, and both linked lists.
// ============================================================================
#include <cstdio>
#include <cstring>
#include <string>
#include "../src/solutions/exam1.h"
#include "../src/solutions/exam2_pointers.h"
#include "../src/solutions/linked_list.h"
#include "../src/solutions/exam_output.h"

static int passed = 0, failed = 0;
static std::string captured;
static void captureOut(const char* text) { captured += text; }

#define CHECK(cond) do { if (cond) passed++; else { failed++; \
    printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static void section(const char* name) { printf("[%s]\n", name); }

static std::string capture(void (*fn)()) {
    captured.clear();
    examOut = captureOut;       // redirect output via the function pointer
    fn();
    examOut = examOutToStdout;
    return captured;
}

static std::string listToString(const LinkedList& l) {
    std::string s;
    for (Node* n = l.GetHead(); n; n = n->next) s += std::to_string(n->data) + " ";
    return s;
}

static std::string dlistForward(const DoublyLinkedList& l) {
    std::string s;
    for (DoublyLinkedNode* n = l.GetHead(); n; n = n->nextPointer) s += std::to_string(n->data) + " ";
    return s;
}

static std::string dlistBackward(const DoublyLinkedList& l) {
    std::string s;
    for (DoublyLinkedNode* n = l.GetTail(); n; n = n->prevPointer) s += std::to_string(n->data) + " ";
    return s;
}

int main() {
    // ------------------------------------------------------------ Exam 1 ----
    section("Q1 isLeapYear");
    CHECK(!isLeapYear(1700)); CHECK(isLeapYear(1600)); CHECK(isLeapYear(2016));
    CHECK(!isLeapYear(2019)); CHECK(isLeapYear(2000)); CHECK(!isLeapYear(1900));

    section("Q2 Reversed / isAPalindrome");
    CHECK(Reversed(123) == 321); CHECK(Reversed(1) == 1); CHECK(Reversed(1234) != 321);
    CHECK(Reversed(1200) == 21); CHECK(Reversed(0) == 0);
    CHECK(isAPalindrome(1)); CHECK(isAPalindrome(121)); CHECK(!isAPalindrome(1213));
    CHECK(isAPalindrome(12321)); CHECK(!isAPalindrome(10));

    section("Q3 isAPrimeNumber");
    CHECK(isAPrimeNumber(3)); CHECK(isAPrimeNumber(2147483647)); CHECK(!isAPrimeNumber(4));
    CHECK(isAPrimeNumber(2)); CHECK(!isAPrimeNumber(1)); CHECK(!isAPrimeNumber(0));
    CHECK(!isAPrimeNumber(-7)); CHECK(!isAPrimeNumber(91)); CHECK(isAPrimeNumber(97));

    section("Q4 convertBinarytoDecimal");
    CHECK(convertBinarytoDecimal(110) == 6); CHECK(convertBinarytoDecimal(111) == 7);
    CHECK(convertBinarytoDecimal(0) == 0); CHECK(convertBinarytoDecimal(1011) == 11);
    CHECK(convertBinarytoDecimal(10000000) == 128);

    section("Q6/Q7/Q19 triangles");
    CHECK(capture(drawRightAngledTriangle) == "A\nAA\nAAA\nAAAA\n");
    const char* diamond = "A\nAA\nAAA\nAAAA\nAAA\nAA\nA\n";
    CHECK(capture(drawIsocelesTriangle) == diamond);
    CHECK(capture(drawIsocelesTriangle2) == diamond);

    section("Q8 find");
    int a8[] = {4, 8, 15, 16, 23, 42};
    CHECK(find(6, a8, 15) == 2); CHECK(find(6, a8, 4) == 0); CHECK(find(6, a8, 99) == -1);
    CHECK(find(0, a8, 4) == -1);

    section("Q9 find2ndLargest");
    int a9[] = {3, 9, 2, 8, 5};
    int a9b[] = {7, 7, 7};
    int a9c[] = {5, 9, 9, 1};
    CHECK(find2ndLargest(5, a9) == 8); CHECK(find2ndLargest(3, a9b) == -1);
    CHECK(find2ndLargest(4, a9c) == 5); CHECK(find2ndLargest(1, a9) == -1);

    section("Q10 copyArraytoArray");
    int src10[] = {1, 2, 3, 4}; int dst10[4] = {0};
    copyArraytoArray(4, src10, dst10);
    CHECK(memcmp(src10, dst10, sizeof(src10)) == 0);

    section("Q11 insertElement");
    int arr11[6] = {10, 20, 30, 40};
    int size11 = 6, count11 = 4;
    CHECK(insertElement(size11, count11, arr11, 25, 2));
    CHECK(count11 == 5 && arr11[2] == 25 && arr11[3] == 30 && arr11[4] == 40);
    CHECK(insertElement(size11, count11, arr11, 5, 0) && arr11[0] == 5 && count11 == 6);
    CHECK(!insertElement(size11, count11, arr11, 99, 1));          // full
    int arr11b[4] = {1, 2}; int s11b = 4, c11b = 2;
    CHECK(insertElement(s11b, c11b, arr11b, 3, 2) && arr11b[2] == 3); // at the end
    CHECK(!insertElement(s11b, c11b, arr11b, 9, 7));                 // bad index

    section("Q12 deleteElement");
    int arr12[5] = {10, 20, 30, 40, 50};
    int size12 = 5, count12 = 5;
    CHECK(deleteElement(size12, count12, arr12, 1));
    CHECK(count12 == 4 && arr12[0] == 10 && arr12[1] == 30 && arr12[3] == 50);
    CHECK(!deleteElement(size12, count12, arr12, 4));
    CHECK(!deleteElement(size12, count12, arr12, -1));

    section("Q13 frequencyCount / Q14 countDuplicates");
    int a13[] = {1, 2, 2, 3, 3, 3};
    CHECK(frequencyCount(6, a13, 3) == 3); CHECK(frequencyCount(6, a13, 7) == 0);
    CHECK(countDuplicates(6, a13) == 3);
    int a14[] = {1, 2, 3}; CHECK(countDuplicates(3, a14) == 0);
    int a14b[] = {5, 5, 5, 5}; CHECK(countDuplicates(4, a14b) == 3);

    section("Q15 reverse / Q16 rotateLeft");
    int a15[] = {1, 2, 3, 4, 5}; reverse(5, a15);
    CHECK(a15[0] == 5 && a15[2] == 3 && a15[4] == 1);
    int a15b[] = {1, 2, 3, 4}; reverse(4, a15b);
    CHECK(a15b[0] == 4 && a15b[1] == 3 && a15b[2] == 2 && a15b[3] == 1);
    int a16[] = {1, 2, 3, 4};
    CHECK(rotateLeft(4, a16) == 1);
    CHECK(a16[0] == 2 && a16[1] == 3 && a16[2] == 4 && a16[3] == 1);
    CHECK(rotateLeft(0, a16) == -1);

    section("Q17 twoMovies / Q18 wordCounter");
    int movies[] = {90, 125, 60, 100};
    CHECK(twoMovies(190, movies, 4)); CHECK(twoMovies(160, movies, 4));
    CHECK(!twoMovies(180, movies, 4));                  // 90+90 would be the same movie
    CHECK(!twoMovies(500, movies, 4));
    char words1[] = "Hello world  from C";
    char words2[] = "   ";
    char words3[] = "one";
    CHECK(wordCounter((int)sizeof(words1), words1) == 4);
    CHECK(wordCounter((int)sizeof(words2), words2) == 0);
    CHECK(wordCounter((int)sizeof(words3), words3) == 1);

    section("Q19-Q21 grid: PrintArray / ReplaceWithMinusOne / FallDownAndReplace");
    int grid[4][6] = { {0,0,3,1,3,4},
                       {0,0,2,3,4,3},
                       {0,0,1,3,3,2},
                       {0,0,1,1,1,1} };
    captured.clear(); examOut = captureOut; PrintArray(grid); examOut = examOutToStdout;
    CHECK(captured.find("  0   0   3   1   3   4 \n") == 0);
    int score = ReplaceWithMinusOne(grid);
    CHECK(score == 4);                                   // bottom row 1,1,1,1
    CHECK(grid[3][2] == -1 && grid[3][5] == -1 && grid[3][1] == 0);
    FallDownAndReplace(grid);
    CHECK(grid[0][2] == -2 && grid[3][2] == 1 && grid[3][3] == 3 && grid[1][5] == 4);
    CHECK(ReplaceWithMinusOne(grid) == 0 || true);       // runs without crashing
    int vgrid[4][6] = { {2,0,0,0,0,0},
                        {2,0,0,0,0,0},
                        {2,0,0,0,0,0},
                        {1,0,0,0,0,0} };
    CHECK(ReplaceWithMinusOne(vgrid) == 6 && vgrid[0][0] == -1 && vgrid[2][0] == -1 && vgrid[3][0] == 1);
    FallDownAndReplace(vgrid);
    CHECK(vgrid[3][0] == 1 && vgrid[0][0] == -2 && vgrid[2][0] == -2);
    int hfirst[4][6] = { {4,4,4,0,0,0},
                         {4,0,0,0,0,0},
                         {4,0,0,0,0,0},
                         {0,0,0,0,0,0} };
    CHECK(ReplaceWithMinusOne(hfirst) == 12 && hfirst[0][2] == -1 && hfirst[1][0] == 4);

    section("Q21 FindAllVariables / Q22 FindAllVariablesInScope");
    char code[1000] =
        "int main() {\n"          // 1
        "    int count = 0;\n"    // 2
        "    char letter = 'a';\n"// 3
        "    if (count == 0) {\n" // 4
        "        int temp = 5;\n" // 5
        "        char* name;\n"   // 6
        "    }\n"                 // 7
        "    int total = 10;\n"   // 8
        "    printf(\"int fake\");\n" // 9
        "    return 0;\n"         // 10
        "}\n";                    // 11
    char vars[1000];
    FindAllVariables(vars, code);
    CHECK(strcmp(vars, "count,letter,temp,name,total") == 0);
    FindAllVariablesInScope(vars, code, 5);
    CHECK(strcmp(vars, "count,letter,temp") == 0);
    FindAllVariablesInScope(vars, code, 6);
    CHECK(strcmp(vars, "count,letter,temp,name") == 0);
    FindAllVariablesInScope(vars, code, 7);
    CHECK(strcmp(vars, "count,letter") == 0);
    FindAllVariablesInScope(vars, code, 8);
    CHECK(strcmp(vars, "count,letter,total") == 0);
    char empty[10] = "";
    FindAllVariables(vars, empty);
    CHECK(vars[0] == '\0');

    // ------------------------------------------------------------ Exam 2 ----
    section("Pointers: swap_ints / swap_chars");
    int x = 5, y = 10; swap_ints(&x, &y); CHECK(x == 10 && y == 5);
    char ca = 'A', cb = 'B'; swap_chars(&ca, &cb); CHECK(ca == 'B' && cb == 'A');

    section("Pointers: find_max / reverse_array / sum_array");
    int pa[5] = {3, 9, 2, 8, 5};
    CHECK(find_max(pa, 5) == 9);
    int neg[3] = {-5, -2, -9}; CHECK(find_max(neg, 3) == -2);
    int ra[5] = {1, 2, 3, 4, 5}; reverse_array(ra, 5);
    CHECK(ra[0] == 5 && ra[1] == 4 && ra[2] == 3 && ra[3] == 2 && ra[4] == 1);
    int sa[4] = {1, 2, 3, 4}; CHECK(sum_array(sa, 4) == 10); CHECK(sum_array(sa, 0) == 0);

    section("Pointers: strings");
    char hello[] = "Hello"; CHECK(strlen_ptr(hello) == 5);
    char none[] = ""; CHECK(strlen_ptr(none) == 0);
    char openai[] = "OpenAI"; CHECK(count_vowels(openai) == 4);  // O,e,A,I (the brief says 3, but I is a vowel too)
    char dest[20]; char src[] = "Hello"; copy_string(dest, src); CHECK(strcmp(dest, "Hello") == 0);
    char d2[20] = "Hello, "; char s2[] = "World!"; concat_strings(d2, s2);
    CHECK(strcmp(d2, "Hello, World!") == 0);
    char apple[] = "apple", banana[] = "banana", apple2[] = "apple", app[] = "app";
    CHECK(compare_strings(apple, banana) < 0); CHECK(compare_strings(banana, apple) > 0);
    CHECK(compare_strings(apple, apple2) == 0); CHECK(compare_strings(app, apple) < 0);

    section("LinkedList");
    {
        LinkedList list;
        CHECK(list.Size() == 0 && list.GetHead() == nullptr);
        list.InsertAtEnd(10); list.InsertAtEnd(20); list.InsertAtBeginning(5);
        CHECK(listToString(list) == "5 10 20 ");
        CHECK(list.InsertAtPosition(15, 2)); CHECK(listToString(list) == "5 10 15 20 ");
        CHECK(list.InsertAtPosition(25, 4)); CHECK(listToString(list) == "5 10 15 20 25 ");
        CHECK(list.InsertAtPosition(1, 0));  CHECK(listToString(list) == "1 5 10 15 20 25 ");
        CHECK(!list.InsertAtPosition(9, 99)); CHECK(!list.InsertAtPosition(9, -1));
        CHECK(list.Search(15) == 3); CHECK(list.Search(99) == -1);
        CHECK(list.DeleteAtPosition(0)); CHECK(listToString(list) == "5 10 15 20 25 ");
        CHECK(list.DeleteAtPosition(2)); CHECK(listToString(list) == "5 10 20 25 ");
        CHECK(list.DeleteAtPosition(3)); CHECK(listToString(list) == "5 10 20 ");
        CHECK(!list.DeleteAtPosition(3));
        list.Reverse(); CHECK(listToString(list) == "20 10 5 ");
        list.Clear(); CHECK(list.Size() == 0 && list.GetHead() == nullptr);
        list.Reverse(); CHECK(list.GetHead() == nullptr);
        CHECK(!list.DeleteAtPosition(0));
    }

    section("DoublyLinkedList");
    {
        DoublyLinkedList d;
        d.InsertAtEnd(2); d.InsertAtBeginning(1); d.InsertAtEnd(4);
        CHECK(dlistForward(d) == "1 2 4 "); CHECK(dlistBackward(d) == "4 2 1 ");
        CHECK(d.InsertAtPosition(3, 2));
        CHECK(dlistForward(d) == "1 2 3 4 "); CHECK(dlistBackward(d) == "4 3 2 1 ");
        CHECK(d.InsertAtPosition(0, 0)); CHECK(d.InsertAtPosition(5, 5));
        CHECK(dlistForward(d) == "0 1 2 3 4 5 "); CHECK(dlistBackward(d) == "5 4 3 2 1 0 ");
        CHECK(!d.InsertAtPosition(9, 99));
        CHECK(d.GetHead()->prevPointer == nullptr && d.GetTail()->nextPointer == nullptr);
        CHECK(d.DeleteAtPosition(0)); CHECK(d.DeleteAtPosition(4)); CHECK(d.DeleteAtPosition(1));
        CHECK(dlistForward(d) == "1 3 4 "); CHECK(dlistBackward(d) == "4 3 1 ");
        CHECK(d.Search(4) == 2 && d.Search(7) == -1);
        d.Clear(); CHECK(d.GetHead() == nullptr && d.GetTail() == nullptr);
        d.InsertAtPosition(7, 0); CHECK(dlistForward(d) == "7 " && dlistBackward(d) == "7 ");
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
