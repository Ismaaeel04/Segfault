// ============================================================================
// problems.cpp  -  The Practice / Mock Exam problem bank.
//
// Each problem has a statement, starter code, a reference solution and a
// test harness. The harness lines use the PQ_* macros from runner.cpp:
//   PQ_TOTAL(n)                          number of test cases
//   PQ_INT(id, "input", expr, expected)  integer result
//   PQ_BOOL(id, "input", expr, expected) bool result
//   PQ_STR(id, "input", got, "expected") string result (pq_arr() formats arrays)
//   PQ_OUT_BEGIN / PQ_OUT_END            compares what the code PRINTS
// Most reference solutions are the real code from src/solutions (same code
// the unit tests check), loaded through sources::Text().
// ============================================================================
#include "content.h"
#include "app/theme.h"
#include <utility>

namespace {

std::vector<Problem> gProblems;

std::string Esc(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
    return o;
}

// Generates harness lines for functions returning bool / int.
std::string TB(const std::string& fn, const std::vector<std::pair<std::string, bool>>& cases) {
    std::string s = "PQ_TOTAL(" + std::to_string(cases.size()) + ");\n";
    int id = 1;
    for (auto& c : cases)
        s += "PQ_BOOL(" + std::to_string(id++) + ", \"" + Esc(fn + "(" + c.first + ")") + "\", " + fn + "(" + c.first + "), " +
             (c.second ? "true" : "false") + ");\n";
    return s;
}
std::string TI(const std::string& fn, const std::vector<std::pair<std::string, long long>>& cases) {
    std::string s = "PQ_TOTAL(" + std::to_string(cases.size()) + ");\n";
    int id = 1;
    for (auto& c : cases)
        s += "PQ_INT(" + std::to_string(id++) + ", \"" + Esc(fn + "(" + c.first + ")") + "\", " + fn + "(" + c.first + "), " +
             std::to_string(c.second) + "LL);\n";
    return s;
}

void Push(Problem p) {
    p.paper = p.section <= S_STRINGS ? 1 : 2;
    gProblems.push_back(std::move(p));
}

const char* kLabGrid = "{0,0,3,1,3,4},{0,0,2,3,4,3},{0,0,1,3,3,2},{0,0,1,1,1,1}";

// ------------------------------------------------------------------ lists ----
const char* kNode = R"PQ(class Node {
public:
    int data;
    Node* next;
    Node(int value) : data(value), next(nullptr) {}
};
)PQ";

const char* kDNode = R"PQ(class DoublyLinkedNode {
public:
    int data;
    DoublyLinkedNode* prevPointer;
    DoublyLinkedNode* nextPointer;
    DoublyLinkedNode(int value) : data(value), prevPointer(nullptr), nextPointer(nullptr) {}
};
)PQ";

const char* kListHelpers = R"PQ(
static const char* pq_list(Node* h) {
    static int a[64]; int n = 0;
    while (h != nullptr && n < 64) { a[n++] = h->data; h = h->next; }
    return pq_arr(a, n);
}
)PQ";

const char* kDListHelpers = R"PQ(
static const char* pq_fwd(DoublyLinkedNode* h) {
    static int a[64]; int n = 0;
    while (h != nullptr && n < 64) { a[n++] = h->data; h = h->nextPointer; }
    return pq_arr(a, n);
}
static const char* pq_bwd(DoublyLinkedNode* t) {
    static int a[64]; int n = 0;
    while (t != nullptr && n < 64) { a[n++] = t->data; t = t->prevPointer; }
    return pq_arr(a, n);
}
)PQ";

void Build() {
    // =================================================== NUMBERS & DIGITS ===
    {
        Problem p;
        p.id = "isLeapYear"; p.title = "Leap Year"; p.section = S_NUMBERS; p.difficulty = 0;
        p.statement = "Return `true` if `year` is a leap year, otherwise `false`.\n\n"
                      "A year is a leap year if it is divisible by **4**, but **not** by **100** - unless it is also divisible by **400**.";
        p.examples = {{"year = 1600", "true", "Divisible by 400."},
                      {"year = 1700", "false", "Divisible by 100 but not by 400."},
                      {"year = 2016", "true", "Divisible by 4 and not by 100."}};
        p.constraints = "1 <= year <= 9999";
        p.starter = "bool isLeapYear(int year) {\n    // Write your code here\n    return false;\n}\n";
        p.solution = sources::Text("isLeapYear");
        p.tests = TB("isLeapYear", {{"1600", true}, {"1700", false}, {"2016", true}, {"2019", false},
                                    {"2000", true}, {"1900", false}, {"4", true}, {"2100", false}});
        p.hints = {"Check the most specific rule first: % 400, then % 100, then % 4.",
                   "Three lines: if (year % 400 == 0) return true; if (year % 100 == 0) return false; return year % 4 == 0;"};
        Push(p);
    }
    {
        Problem p;
        p.id = "Reversed"; p.title = "Reverse the Digits"; p.section = S_NUMBERS; p.difficulty = 0;
        p.statement = "Return the number with its digits in reverse order. Trailing zeros disappear: `1200` becomes `21`.";
        p.examples = {{"testNumber = 123", "321", ""}, {"testNumber = 1", "1", ""}, {"testNumber = 1200", "21", "0021 is just 21 as an int."}};
        p.constraints = "0 <= testNumber <= 999999999";
        p.starter = "int Reversed(int testNumber) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = sources::Text("Reversed");
        p.tests = TI("Reversed", {{"123", 321}, {"1", 1}, {"1200", 21}, {"98765", 56789}, {"0", 0}, {"10", 1}, {"7007", 7007}});
        p.hints = {"`n % 10` is the last digit and `n / 10` removes it.",
                   "Build the answer with `reversed = reversed * 10 + digit` inside a while loop."};
        Push(p);
    }
    {
        Problem p;
        p.id = "isAPalindrome"; p.title = "Palindrome Number"; p.section = S_NUMBERS; p.difficulty = 0;
        p.statement = "Return `true` if the number reads the same forwards and backwards.\n\n"
                      "You may write a helper function (for example `Reversed`) in the same editor.";
        p.examples = {{"testNumber = 121", "true", ""}, {"testNumber = 1213", "false", ""}, {"testNumber = 1", "true", "A single digit is always a palindrome."}};
        p.constraints = "0 <= testNumber <= 999999999";
        p.starter = "bool isAPalindrome(int testNumber) {\n    // Write your code here\n    return false;\n}\n";
        p.solution = sources::Text("Reversed") + "\n" + sources::Text("isAPalindrome");
        p.tests = TB("isAPalindrome", {{"121", true}, {"1213", false}, {"1", true}, {"12321", true}, {"10", false},
                                       {"0", true}, {"1221", true}, {"123", false}});
        p.hints = {"Reverse the number (see the Reverse the Digits problem), then compare.",
                   "return Reversed(testNumber) == testNumber;"};
        Push(p);
    }
    {
        Problem p;
        p.id = "isAPrimeNumber"; p.title = "Prime Number"; p.section = S_NUMBERS; p.difficulty = 1;
        p.statement = "Return `true` if `numbertoTest` is prime: only divisible by 1 and itself.\n\n"
                      "The tests include **2147483647** (the largest int), so a loop up to n is too slow: the time limit is 4 seconds.";
        p.examples = {{"numbertoTest = 3", "true", ""}, {"numbertoTest = 4", "false", "4 = 2 x 2"},
                      {"numbertoTest = 2147483647", "true", "Must finish quickly."}};
        p.constraints = "-2^31 <= numbertoTest <= 2^31 - 1. Numbers below 2 are not prime.";
        p.starter = "bool isAPrimeNumber(int numbertoTest) {\n    // Write your code here\n    return false;\n}\n";
        p.solution = sources::Text("isAPrimeNumber");
        p.tests = TB("isAPrimeNumber", {{"3", true}, {"4", false}, {"2147483647", true}, {"2", true}, {"1", false},
                                        {"0", false}, {"-7", false}, {"91", false}, {"97", true}, {"1000003", true}});
        p.hints = {"You only need to test divisors up to the square root: loop while i * i <= n.",
                   "Use `long long i` so `i * i` cannot overflow. Handle n < 2 and even numbers first."};
        Push(p);
    }
    {
        Problem p;
        p.id = "convertBinarytoDecimal"; p.title = "Binary to Decimal"; p.section = S_NUMBERS; p.difficulty = 0;
        p.statement = "The parameter is an int that *looks* binary, like `110`. Return its decimal value **without using the shift operator** (`<<`).";
        p.examples = {{"binaryNumber = 110", "6", "0*1 + 1*2 + 1*4"}, {"binaryNumber = 111", "7", ""}, {"binaryNumber = 1011", "11", "1 + 2 + 0 + 8"}};
        p.constraints = "binaryNumber only contains the digits 0 and 1 (at most 10 digits).";
        p.starter = "int convertBinarytoDecimal(int binaryNumber) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = sources::Text("convertBinarytoDecimal");
        p.tests = TI("convertBinarytoDecimal", {{"110", 6}, {"111", 7}, {"1011", 11}, {"0", 0}, {"1", 1}, {"10000000", 128}, {"1111111111", 1023}});
        p.forbid = {"<<"};
        p.ruleText = "The exam asks for this without the shift operator.";
        p.hints = {"Read the digits from the right with % 10 and / 10.",
                   "Keep a `base` that starts at 1 and doubles every loop: decimal += digit * base."};
        Push(p);
    }
    {
        Problem p;
        p.id = "sumOfDigits"; p.title = "Sum of Digits"; p.section = S_NUMBERS; p.difficulty = 0;
        p.statement = "Return the sum of the digits of a non-negative number.";
        p.examples = {{"n = 1234", "10", "1 + 2 + 3 + 4"}, {"n = 0", "0", ""}, {"n = 99999", "45", ""}};
        p.constraints = "0 <= n <= 2^31 - 1";
        p.starter = "int sumOfDigits(int n) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = "int sumOfDigits(int n) {\n    int sum = 0;\n    while (n > 0) {\n        sum += n % 10;\n        n /= 10;\n    }\n    return sum;\n}\n";
        p.tests = TI("sumOfDigits", {{"1234", 10}, {"0", 0}, {"99999", 45}, {"9", 9}, {"1001", 2}, {"2147483647", 46}});
        p.hints = {"Same loop as Reverse the Digits, but add the digit instead of appending it."};
        Push(p);
    }
    {
        Problem p;
        p.id = "gcd"; p.title = "Greatest Common Divisor"; p.section = S_NUMBERS; p.difficulty = 0;
        p.statement = "Return the greatest common divisor of two non-negative integers (not both 0).\n\n"
                      "Euclid: `gcd(a, b) = gcd(b, a % b)` and `gcd(a, 0) = a`.";
        p.examples = {{"a = 12, b = 18", "6", ""}, {"a = 7, b = 13", "1", ""}, {"a = 0, b = 5", "5", ""}};
        p.constraints = "0 <= a, b <= 10^9";
        p.starter = "int gcd(int a, int b) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = "int gcd(int a, int b) {\n    while (b != 0) {\n        int r = a % b;\n        a = b;\n        b = r;\n    }\n    return a;\n}\n";
        p.tests = TI("gcd", {{"12, 18", 6}, {"7, 13", 1}, {"0, 5", 5}, {"100, 10", 10}, {"21, 6", 3}, {"5, 0", 5}, {"1000000000, 999999999", 1}});
        p.hints = {"Loop while b != 0: remainder = a % b; a = b; b = remainder."};
        Push(p);
    }

    // =================================================== LOOPS & PATTERNS ===
    {
        Problem p;
        p.id = "drawRightAngledTriangle"; p.title = "Right-Angled Triangle"; p.section = S_LOOPS; p.difficulty = 0;
        p.statement = "Print this triangle using **two nested for loops**:\n\n`A`\n`AA`\n`AAA`\n`AAAA`";
        p.examples = {{"drawRightAngledTriangle()", "A\nAA\nAAA\nAAAA", ""}};
        p.exampleCases = 1;
        p.starter = "void drawRightAngledTriangle() {\n    // Use printf(\"A\") and printf(\"\\n\")\n}\n";
        p.solution = sources::Text("drawRightAngledTriangle");
        p.tests = "PQ_TOTAL(1);\nPQ_OUT_BEGIN(1, \"drawRightAngledTriangle()\", \"A\\\\nAA\\\\nAAA\\\\nAAAA\");\ndrawRightAngledTriangle();\nPQ_OUT_END(1);\n";
        p.hints = {"Outer loop: row from 1 to 4. Inner loop: print `row` A's. Then a newline."};
        Push(p);
    }
    {
        Problem p;
        p.id = "drawIsocelesTriangle"; p.title = "Diamond with One If"; p.section = S_LOOPS; p.difficulty = 1;
        p.statement = "Print the pattern below using **2 for loops** and **at most one if/else**:\n\n`A`\n`AA`\n`AAA`\n`AAAA`\n`AAA`\n`AA`\n`A`";
        p.examples = {{"drawIsocelesTriangle()", "A\nAA\nAAA\nAAAA\nAAA\nAA\nA", ""}};
        p.exampleCases = 1;
        p.starter = "void drawIsocelesTriangle() {\n    // Write your code here\n}\n";
        p.solution = sources::Text("drawIsocelesTriangle");
        p.tests = "PQ_TOTAL(1);\nPQ_OUT_BEGIN(1, \"drawIsocelesTriangle()\", \"A\\\\nAA\\\\nAAA\\\\nAAAA\\\\nAAA\\\\nAA\\\\nA\");\ndrawIsocelesTriangle();\nPQ_OUT_END(1);\n";
        p.hints = {"Use 7 rows. For rows 1-4 the count is the row number.", "For rows 5-7 the count is 8 - row."};
        Push(p);
    }
    {
        Problem p;
        p.id = "drawIsocelesTriangle2"; p.title = "Diamond with No Conditionals"; p.section = S_LOOPS; p.difficulty = 1;
        p.statement = "Print the same 1-2-3-4-3-2-1 pattern of A's with **2 for loops** and **no conditional statements** "
                      "(no `if`, no `else`, no `?:`, no `switch`).";
        p.examples = {{"drawIsocelesTriangle2()", "A\nAA\nAAA\nAAAA\nAAA\nAA\nA", ""}};
        p.exampleCases = 1;
        p.starter = "void drawIsocelesTriangle2() {\n    // Write your code here\n}\n";
        p.solution = sources::Text("drawIsocelesTriangle2");
        p.tests = "PQ_TOTAL(1);\nPQ_OUT_BEGIN(1, \"drawIsocelesTriangle2()\", \"A\\\\nAA\\\\nAAA\\\\nAAAA\\\\nAAA\\\\nAA\\\\nA\");\ndrawIsocelesTriangle2();\nPQ_OUT_END(1);\n";
        p.forbid = {"if", "else", "?", "switch"};
        p.ruleText = "This version must not use conditionals.";
        p.hints = {"Let the row go from -3 to 3.", "The count is 4 - abs(row). abs() is in <stdlib.h>."};
        Push(p);
    }
    {
        Problem p;
        p.id = "printNumberTriangle"; p.title = "Number Triangle"; p.section = S_LOOPS; p.difficulty = 0;
        p.statement = "Print `n` rows. Row `r` prints the numbers 1 to r with no spaces. For n = 4:\n\n`1`\n`12`\n`123`\n`1234`";
        p.examples = {{"n = 4", "1\n12\n123\n1234", ""}, {"n = 1", "1", ""}};
        p.exampleCases = 2;
        p.starter = "void printNumberTriangle(int n) {\n    // Write your code here\n}\n";
        p.solution = "void printNumberTriangle(int n) {\n    for (int row = 1; row <= n; row++) {\n        for (int k = 1; k <= row; k++) {\n            printf(\"%d\", k);\n        }\n        printf(\"\\n\");\n    }\n}\n";
        p.tests = "PQ_TOTAL(3);\n"
                  "PQ_OUT_BEGIN(1, \"n = 4\", \"1\\\\n12\\\\n123\\\\n1234\"); printNumberTriangle(4); PQ_OUT_END(1);\n"
                  "PQ_OUT_BEGIN(2, \"n = 1\", \"1\"); printNumberTriangle(1); PQ_OUT_END(2);\n"
                  "PQ_OUT_BEGIN(3, \"n = 6\", \"1\\\\n12\\\\n123\\\\n1234\\\\n12345\\\\n123456\"); printNumberTriangle(6); PQ_OUT_END(3);\n";
        p.hints = {"Same as the right-angled triangle, but print the column number k instead of 'A'."};
        Push(p);
    }
    {
        Problem p;
        p.id = "printHollowSquare"; p.title = "Hollow Square"; p.section = S_LOOPS; p.difficulty = 1;
        p.statement = "Print an `n x n` square of `*` that is hollow: only the border has stars, the inside is spaces. For n = 4:\n\n"
                      "`****`\n`*  *`\n`*  *`\n`****`";
        p.examples = {{"n = 4", "****\n*  *\n*  *\n****", ""}, {"n = 2", "**\n**", ""}};
        p.exampleCases = 2;
        p.starter = "void printHollowSquare(int n) {\n    // Write your code here\n}\n";
        p.solution = "void printHollowSquare(int n) {\n    for (int row = 0; row < n; row++) {\n        for (int col = 0; col < n; col++) {\n"
                     "            bool border = row == 0 || row == n - 1 || col == 0 || col == n - 1;\n"
                     "            printf(\"%c\", border ? '*' : ' ');\n        }\n        printf(\"\\n\");\n    }\n}\n";
        p.tests = "PQ_TOTAL(3);\n"
                  "PQ_OUT_BEGIN(1, \"n = 4\", \"****\\\\n*  *\\\\n*  *\\\\n****\"); printHollowSquare(4); PQ_OUT_END(1);\n"
                  "PQ_OUT_BEGIN(2, \"n = 2\", \"**\\\\n**\"); printHollowSquare(2); PQ_OUT_END(2);\n"
                  "PQ_OUT_BEGIN(3, \"n = 5\", \"*****\\\\n*   *\\\\n*   *\\\\n*   *\\\\n*****\"); printHollowSquare(5); PQ_OUT_END(3);\n";
        p.hints = {"A cell is on the border if row is 0 or n-1, or col is 0 or n-1."};
        Push(p);
    }

    // ============================================================= ARRAYS ===
    {
        Problem p;
        p.id = "find"; p.title = "Find an Element"; p.section = S_ARRAYS; p.difficulty = 0;
        p.statement = "Return the index of the **first** occurrence of `toFind` in the array, or `-1` if it is not there.";
        p.examples = {{"arr = {4, 8, 15, 16, 23, 42}, toFind = 15", "2", ""}, {"arr = {4, 8, 15}, toFind = 99", "-1", ""},
                      {"arr = {7, 7, 7}, toFind = 7", "0", "First occurrence."}};
        p.constraints = "0 <= size <= 1000";
        p.starter = "int find(int size, int arr[], int toFind) {\n    // Write your code here\n    return -1;\n}\n";
        p.solution = sources::Text("find");
        p.tests = R"PQ(PQ_TOTAL(5);
{ int a[] = {4, 8, 15, 16, 23, 42}; PQ_INT(1, "arr = {4, 8, 15, 16, 23, 42}, toFind = 15", find(6, a, 15), 2); }
{ int a[] = {4, 8, 15}; PQ_INT(2, "arr = {4, 8, 15}, toFind = 99", find(3, a, 99), -1); }
{ int a[] = {7, 7, 7}; PQ_INT(3, "arr = {7, 7, 7}, toFind = 7", find(3, a, 7), 0); }
{ int a[] = {5, 3, 9, 3}; PQ_INT(4, "arr = {5, 3, 9, 3}, toFind = 3", find(4, a, 3), 1); }
{ int a[] = {1}; PQ_INT(5, "size = 0", find(0, a, 1), -1); }
)PQ";
        p.hints = {"Loop i from 0 to size - 1 and return i as soon as arr[i] == toFind."};
        Push(p);
    }
    {
        Problem p;
        p.id = "find2ndLargest"; p.title = "Second Largest"; p.section = S_ARRAYS; p.difficulty = 1;
        p.statement = "The array holds positive integers. Return the **second largest distinct value**, or `-1` if there is none.";
        p.examples = {{"arr = {3, 9, 2, 8, 5}", "8", ""}, {"arr = {7, 7, 7}", "-1", "All values are the same."},
                      {"arr = {5, 9, 9, 1}", "5", "The second 9 does not count."}};
        p.constraints = "0 <= size <= 1000, arr[i] >= 1";
        p.starter = "int find2ndLargest(int size, int arr[]) {\n    // Write your code here\n    return -1;\n}\n";
        p.solution = sources::Text("find2ndLargest");
        p.tests = R"PQ(PQ_TOTAL(6);
{ int a[] = {3, 9, 2, 8, 5}; PQ_INT(1, "arr = {3, 9, 2, 8, 5}", find2ndLargest(5, a), 8); }
{ int a[] = {7, 7, 7}; PQ_INT(2, "arr = {7, 7, 7}", find2ndLargest(3, a), -1); }
{ int a[] = {5, 9, 9, 1}; PQ_INT(3, "arr = {5, 9, 9, 1}", find2ndLargest(4, a), 5); }
{ int a[] = {4}; PQ_INT(4, "arr = {4}", find2ndLargest(1, a), -1); }
{ int a[] = {1, 2}; PQ_INT(5, "arr = {1, 2}", find2ndLargest(2, a), 1); }
{ int a[] = {10, 20, 30, 30, 20}; PQ_INT(6, "arr = {10, 20, 30, 30, 20}", find2ndLargest(5, a), 20); }
)PQ";
        p.hints = {"Keep two variables, largest and second, both starting at -1.",
                   "If a value beats largest, the old largest becomes second. Else if it is < largest and > second, it becomes second."};
        Push(p);
    }
    {
        Problem p;
        p.id = "copyArraytoArray"; p.title = "Copy an Array"; p.section = S_ARRAYS; p.difficulty = 0;
        p.statement = "Copy every element of `arr1` into `arr2`. Both arrays have `size` elements.";
        p.examples = {{"arr1 = {1, 2, 3, 4}", "arr2 = {1, 2, 3, 4}", ""}, {"arr1 = {9}", "arr2 = {9}", ""}};
        p.exampleCases = 2;
        p.starter = "void copyArraytoArray(int size, int arr1[], int arr2[]) {\n    // Write your code here\n}\n";
        p.solution = sources::Text("copyArraytoArray");
        p.tests = R"PQ(PQ_TOTAL(3);
{ int a[] = {1, 2, 3, 4}; int b[4] = {0}; copyArraytoArray(4, a, b); PQ_STR(1, "arr1 = {1, 2, 3, 4}", pq_arr(b, 4), "{1, 2, 3, 4}"); }
{ int a[] = {9}; int b[1] = {0}; copyArraytoArray(1, a, b); PQ_STR(2, "arr1 = {9}", pq_arr(b, 1), "{9}"); }
{ int a[] = {-5, 0, 5, 10, 15}; int b[5] = {0}; copyArraytoArray(5, a, b); PQ_STR(3, "arr1 = {-5, 0, 5, 10, 15}", pq_arr(b, 5), "{-5, 0, 5, 10, 15}"); }
)PQ";
        p.hints = {"One loop: arr2[i] = arr1[i];"};
        Push(p);
    }
    {
        Problem p;
        p.id = "insertElement"; p.title = "Insert into an Array"; p.section = S_ARRAYS; p.difficulty = 1; p.cppOnly = true;
        p.statement = "Insert `elementToInsert` at `insertIndex`, shifting the elements after it one place to the right.\n\n"
                      "- `size` is the capacity of the array, `count` is how many slots are used.\n"
                      "- Return `false` (and change nothing) if the array is full or the index is not between 0 and count.\n"
                      "- On success, `count` grows by one. Both are passed **by reference** (`int&`).";
        p.examples = {{"arr = {10, 20, 30, 40}, size = 6, count = 4, insert 25 at 2", "true, arr = {10, 20, 25, 30, 40}, count = 5", ""},
                      {"arr = {1, 2, 3}, size = 3, count = 3, insert 9 at 0", "false", "The array is full."}};
        p.exampleCases = 3;
        p.starter = "bool insertElement(int& size, int& count, int arr[], int elementToInsert, int insertIndex) {\n    // Write your code here\n    return false;\n}\n";
        p.solution = sources::Text("insertElement");
        p.tests = R"PQ(PQ_TOTAL(8);
{ int a[6] = {10, 20, 30, 40}; int size = 6, count = 4;
  bool ok = insertElement(size, count, a, 25, 2);
  PQ_BOOL(1, "arr = {10, 20, 30, 40}, size 6, insert 25 at 2 -> returns", ok, true);
  PQ_STR(2, "... array afterwards", pq_arr(a, count), "{10, 20, 25, 30, 40}");
  PQ_INT(3, "... count afterwards", count, 5); }
{ int a[3] = {1, 2, 3}; int size = 3, count = 3;
  PQ_BOOL(4, "full array {1, 2, 3}, insert 9 at 0 -> returns", insertElement(size, count, a, 9, 0), false);
  PQ_STR(5, "... array unchanged", pq_arr(a, count), "{1, 2, 3}"); }
{ int a[4] = {1, 2}; int size = 4, count = 2; insertElement(size, count, a, 3, 2);
  PQ_STR(6, "{1, 2}, insert 3 at index 2 (the end)", pq_arr(a, count), "{1, 2, 3}"); }
{ int a[4] = {5, 6}; int size = 4, count = 2; insertElement(size, count, a, 4, 0);
  PQ_STR(7, "{5, 6}, insert 4 at index 0", pq_arr(a, count), "{4, 5, 6}"); }
{ int a[4] = {1, 2}; int size = 4, count = 2;
  PQ_BOOL(8, "{1, 2}, insert at index 5 -> returns", insertElement(size, count, a, 9, 5), false); }
)PQ";
        p.hints = {"Refuse first: if (count >= size || insertIndex < 0 || insertIndex > count) return false;",
                   "Shift from the END: for (i = count; i > insertIndex; i--) arr[i] = arr[i - 1]; then store and count++."};
        Push(p);
    }
    {
        Problem p;
        p.id = "deleteElement"; p.title = "Delete from an Array"; p.section = S_ARRAYS; p.difficulty = 1; p.cppOnly = true;
        p.statement = "Delete the element at `deleteIndex`, shifting the later elements one place to the left, and decrease `count`.\n\n"
                      "Return `false` (and change nothing) if the index is not between 0 and count - 1.";
        p.examples = {{"arr = {10, 20, 30, 40, 50}, count = 5, deleteIndex = 1", "true, arr = {10, 30, 40, 50}, count = 4", ""},
                      {"arr = {10, 20}, count = 2, deleteIndex = 2", "false", "Index 2 does not exist."}};
        p.exampleCases = 3;
        p.starter = "bool deleteElement(int& size, int& count, int arr[], int deleteIndex) {\n    // Write your code here\n    return false;\n}\n";
        p.solution = sources::Text("deleteElement");
        p.tests = R"PQ(PQ_TOTAL(7);
{ int a[5] = {10, 20, 30, 40, 50}; int size = 5, count = 5;
  bool ok = deleteElement(size, count, a, 1);
  PQ_BOOL(1, "{10, 20, 30, 40, 50}, delete index 1 -> returns", ok, true);
  PQ_STR(2, "... array afterwards", pq_arr(a, count), "{10, 30, 40, 50}");
  PQ_INT(3, "... count afterwards", count, 4); }
{ int a[2] = {10, 20}; int size = 2, count = 2;
  PQ_BOOL(4, "{10, 20}, delete index 2 -> returns", deleteElement(size, count, a, 2), false); }
{ int a[3] = {1, 2, 3}; int size = 3, count = 3; deleteElement(size, count, a, 2);
  PQ_STR(5, "{1, 2, 3}, delete the last one", pq_arr(a, count), "{1, 2}"); }
{ int a[3] = {1, 2, 3}; int size = 3, count = 3; deleteElement(size, count, a, 0);
  PQ_STR(6, "{1, 2, 3}, delete index 0", pq_arr(a, count), "{2, 3}"); }
{ int a[3] = {1, 2, 3}; int size = 3, count = 3;
  PQ_BOOL(7, "{1, 2, 3}, delete index -1 -> returns", deleteElement(size, count, a, -1), false); }
)PQ";
        p.hints = {"Validate: if (deleteIndex < 0 || deleteIndex >= count) return false;",
                   "Shift forwards from the hole: arr[i] = arr[i + 1] for i from deleteIndex to count - 2, then count--."};
        Push(p);
    }
    {
        Problem p;
        p.id = "frequencyCount"; p.title = "Count Occurrences"; p.section = S_ARRAYS; p.difficulty = 0;
        p.statement = "Return how many times `value` appears in the array.";
        p.examples = {{"arr = {1, 2, 2, 3, 3, 3}, value = 3", "3", ""}, {"arr = {1, 2, 3}, value = 7", "0", ""}, {"arr = {5, 5}, value = 5", "2", ""}};
        p.starter = "int frequencyCount(int size, int arr[], int value) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = sources::Text("frequencyCount");
        p.tests = R"PQ(PQ_TOTAL(4);
{ int a[] = {1, 2, 2, 3, 3, 3}; PQ_INT(1, "arr = {1, 2, 2, 3, 3, 3}, value = 3", frequencyCount(6, a, 3), 3); }
{ int a[] = {1, 2, 3}; PQ_INT(2, "arr = {1, 2, 3}, value = 7", frequencyCount(3, a, 7), 0); }
{ int a[] = {5, 5}; PQ_INT(3, "arr = {5, 5}, value = 5", frequencyCount(2, a, 5), 2); }
{ int a[] = {0, -1, 0, -1, 0}; PQ_INT(4, "arr = {0, -1, 0, -1, 0}, value = 0", frequencyCount(5, a, 0), 3); }
)PQ";
        p.hints = {"A counter and one loop."};
        Push(p);
    }
    {
        Problem p;
        p.id = "countDuplicates"; p.title = "Count Duplicates"; p.section = S_ARRAYS; p.difficulty = 1;
        p.statement = "Count the duplicate elements: an element is a duplicate if the **same value appeared earlier** in the array.";
        p.examples = {{"arr = {1, 2, 2, 3, 3, 3}", "3", "One extra 2 and two extra 3's."}, {"arr = {1, 2, 3}", "0", ""},
                      {"arr = {5, 5, 5, 5}", "3", ""}};
        p.starter = "int countDuplicates(int size, int arr[]) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = sources::Text("countDuplicates");
        p.tests = R"PQ(PQ_TOTAL(5);
{ int a[] = {1, 2, 2, 3, 3, 3}; PQ_INT(1, "arr = {1, 2, 2, 3, 3, 3}", countDuplicates(6, a), 3); }
{ int a[] = {1, 2, 3}; PQ_INT(2, "arr = {1, 2, 3}", countDuplicates(3, a), 0); }
{ int a[] = {5, 5, 5, 5}; PQ_INT(3, "arr = {5, 5, 5, 5}", countDuplicates(4, a), 3); }
{ int a[] = {4, 1, 4, 2, 1}; PQ_INT(4, "arr = {4, 1, 4, 2, 1}", countDuplicates(5, a), 2); }
{ int a[] = {7}; PQ_INT(5, "arr = {7}", countDuplicates(1, a), 0); }
)PQ";
        p.hints = {"For each i, look at every j < i.", "If arr[j] == arr[i], count it and break, so arr[i] is counted only once."};
        Push(p);
    }
    {
        Problem p;
        p.id = "reverse"; p.title = "Reverse an Array in Place"; p.section = S_ARRAYS; p.difficulty = 0;
        p.statement = "Reverse the array **in place**: do not use a second array.";
        p.examples = {{"arr = {1, 2, 3, 4, 5}", "{5, 4, 3, 2, 1}", ""}, {"arr = {1, 2, 3, 4}", "{4, 3, 2, 1}", ""}, {"arr = {7}", "{7}", ""}};
        p.starter = "void reverse(int size, int arr[]) {\n    // Write your code here\n}\n";
        p.solution = sources::Text("reverse");
        p.tests = R"PQ(PQ_TOTAL(4);
{ int a[] = {1, 2, 3, 4, 5}; reverse(5, a); PQ_STR(1, "arr = {1, 2, 3, 4, 5}", pq_arr(a, 5), "{5, 4, 3, 2, 1}"); }
{ int a[] = {1, 2, 3, 4}; reverse(4, a); PQ_STR(2, "arr = {1, 2, 3, 4}", pq_arr(a, 4), "{4, 3, 2, 1}"); }
{ int a[] = {7}; reverse(1, a); PQ_STR(3, "arr = {7}", pq_arr(a, 1), "{7}"); }
{ int a[] = {9, 8}; reverse(2, a); PQ_STR(4, "arr = {9, 8}", pq_arr(a, 2), "{8, 9}"); }
)PQ";
        p.hints = {"Two indexes: left = 0 and right = size - 1. Swap, then move them towards each other while left < right."};
        Push(p);
    }
    {
        Problem p;
        p.id = "rotateLeft"; p.title = "Rotate Left by One"; p.section = S_ARRAYS; p.difficulty = 0;
        p.statement = "Move every element one place to the left; the first element goes to the end. Return the element that moved (or -1 if the array is empty).";
        p.examples = {{"arr = {1, 2, 3, 4}", "returns 1, arr = {2, 3, 4, 1}", ""}, {"arr = {9}", "returns 9, arr = {9}", ""}};
        p.exampleCases = 2;
        p.starter = "int rotateLeft(int size, int arr[]) {\n    // Write your code here\n    return -1;\n}\n";
        p.solution = sources::Text("rotateLeft");
        p.tests = R"PQ(PQ_TOTAL(5);
{ int a[] = {1, 2, 3, 4}; int r = rotateLeft(4, a); PQ_STR(1, "arr = {1, 2, 3, 4}", pq_arr(a, 4), "{2, 3, 4, 1}"); PQ_INT(2, "arr = {1, 2, 3, 4}: return value", r, 1); }
{ int a[] = {9}; rotateLeft(1, a); PQ_STR(3, "arr = {9}", pq_arr(a, 1), "{9}"); }
{ int a[] = {5, 6, 7}; rotateLeft(3, a); PQ_STR(4, "arr = {5, 6, 7}", pq_arr(a, 3), "{6, 7, 5}"); }
{ int a[] = {0}; PQ_INT(5, "size = 0: return value", rotateLeft(0, a), -1); }
)PQ";
        p.hints = {"Save arr[0], shift arr[i] = arr[i + 1], then put the saved value in arr[size - 1]."};
        Push(p);
    }
    {
        Problem p;
        p.id = "twoMovies"; p.title = "Two Movies on a Flight"; p.section = S_ARRAYS; p.difficulty = 1;
        p.statement = "On a flight you watch **exactly two different movies**. Return `true` if two movies' lengths add up exactly to `flightLength`.";
        p.examples = {{"flightLength = 190, movieLengths = {90, 125, 60, 100}", "true", "90 + 100"},
                      {"flightLength = 180, movieLengths = {90, 125, 60, 100}", "false", "90 + 90 would be the same movie twice."},
                      {"flightLength = 185, movieLengths = {90, 125, 60, 100}", "true", "125 + 60"}};
        p.starter = "bool twoMovies(int flightLength, int movieLengths[], int size) {\n    // Write your code here\n    return false;\n}\n";
        p.solution = sources::Text("twoMovies");
        p.tests = R"PQ(PQ_TOTAL(6);
{ int m[] = {90, 125, 60, 100}; PQ_BOOL(1, "190, {90, 125, 60, 100}", twoMovies(190, m, 4), true); }
{ int m[] = {90, 125, 60, 100}; PQ_BOOL(2, "180, {90, 125, 60, 100}", twoMovies(180, m, 4), false); }
{ int m[] = {90, 125, 60, 100}; PQ_BOOL(3, "185, {90, 125, 60, 100}", twoMovies(185, m, 4), true); }
{ int m[] = {90, 90}; PQ_BOOL(4, "180, {90, 90}", twoMovies(180, m, 2), true); }
{ int m[] = {100}; PQ_BOOL(5, "200, {100}", twoMovies(200, m, 1), false); }
{ int m[] = {30, 40, 50}; PQ_BOOL(6, "500, {30, 40, 50}", twoMovies(500, m, 3), false); }
)PQ";
        p.hints = {"Check every pair once: for i, then for j = i + 1."};
        Push(p);
    }
    {
        Problem p;
        p.id = "wordCounter"; p.title = "Word Counter"; p.section = S_ARRAYS; p.difficulty = 1;
        p.statement = "Count the words in a char array. A word is one or more letters `A..Z` or `a..z`; words are separated by spaces. "
                      "Stop at `size` or at the `'\\0'` terminator.";
        p.examples = {{"\"Hello world  from C\"", "4", "Extra spaces do not create words."}, {"\"   \"", "0", ""}, {"\"one\"", "1", ""}};
        p.starter = "int wordCounter(int size, char characters[]) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = sources::Text("wordCounter");
        p.tests = R"PQ(PQ_TOTAL(5);
{ char s[] = "Hello world  from C"; PQ_INT(1, "\"Hello world  from C\"", wordCounter((int)sizeof(s), s), 4); }
{ char s[] = "   "; PQ_INT(2, "\"   \"", wordCounter((int)sizeof(s), s), 0); }
{ char s[] = "one"; PQ_INT(3, "\"one\"", wordCounter((int)sizeof(s), s), 1); }
{ char s[] = "  leading and trailing  "; PQ_INT(4, "\"  leading and trailing  \"", wordCounter((int)sizeof(s), s), 3); }
{ char s[] = "a b c d e"; PQ_INT(5, "\"a b c d e\"", wordCounter((int)sizeof(s), s), 5); }
)PQ";
        p.hints = {"Keep a bool inWord. When you see a letter and inWord is false, a new word starts: count it.",
                   "Any non-letter sets inWord back to false."};
        Push(p);
    }

    // ========================================================== 2D ARRAYS ===
    {
        Problem p;
        p.id = "PrintArray"; p.title = "Print a 4x6 Grid"; p.section = S_GRID; p.difficulty = 0;
        p.statement = "Print the 4x6 array in rows and columns: one row per line, values separated by spaces.";
        p.examples = {{"Lab1 grid", "0 0 3 1 3 4\n0 0 2 3 4 3\n0 0 1 3 3 2\n0 0 1 1 1 1", "Spacing is not checked, only the order."}};
        p.exampleCases = 1;
        p.starter = "void PrintArray(int array[4][6]) {\n    // Write your code here\n}\n";
        p.solution = sources::Text("PrintArray");
        p.tests = std::string("PQ_TOTAL(2);\n{ int g[4][6] = {") + kLabGrid + "};\n"
                  "PQ_OUT_BEGIN(1, \"Lab1 grid\", \"0 0 3 1 3 4\\\\n0 0 2 3 4 3\\\\n0 0 1 3 3 2\\\\n0 0 1 1 1 1\"); PrintArray(g); PQ_OUT_END(1); }\n"
                  "{ int g[4][6] = {{1,2,3,4,5,6},{7,8,9,10,11,12},{-1,-2,0,0,0,0},{4,4,4,4,4,4}};\n"
                  "PQ_OUT_BEGIN(2, \"numbered grid\", \"1 2 3 4 5 6\\\\n7 8 9 10 11 12\\\\n-1 -2 0 0 0 0\\\\n4 4 4 4 4 4\"); PrintArray(g); PQ_OUT_END(2); }\n";
        p.hints = {"Outer loop over the 4 rows, inner loop over the 6 columns, newline after each row."};
        Push(p);
    }
    {
        Problem p;
        p.id = "ReplaceWithMinusOne"; p.title = "Find and Remove a Cluster"; p.section = S_GRID; p.difficulty = 2;
        p.statement = "Scan the grid from (0,0), left to right along each row. At each position:\n"
                      "- first look for a **horizontal** line of 3 or more identical values starting there (going right),\n"
                      "- if there is none, look for a **vertical** line of 3 or more going down.\n\n"
                      "The **first** cluster found is scored (sum of its values) and replaced with -1. Return the score, or 0 if there is no cluster. "
                      "Only values 1 to 4 can form clusters (0, -1 and -2 never do).";
        p.examples = {{"Lab1 grid", "4", "The bottom row 1 1 1 1 is the first cluster; it becomes -1 -1 -1 -1."}};
        p.exampleCases = 2;
        p.starter = "int ReplaceWithMinusOne(int array[4][6]) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = sources::Text("ReplaceWithMinusOne");
        p.tests = std::string("PQ_TOTAL(8);\n{ int g[4][6] = {") + kLabGrid + R"PQ(};
  int s = ReplaceWithMinusOne(g);
  PQ_INT(1, "Lab1 grid: score", s, 4);
  PQ_STR(2, "Lab1 grid: grid afterwards", pq_arr(&g[0][0], 24), "{0, 0, 3, 1, 3, 4, 0, 0, 2, 3, 4, 3, 0, 0, 1, 3, 3, 2, 0, 0, -1, -1, -1, -1}"); }
{ int g[4][6] = {{2,0,0,0,0,0},{2,0,0,0,0,0},{2,0,0,0,0,0},{1,0,0,0,0,0}};
  PQ_INT(3, "vertical 2,2,2 in column 0: score", ReplaceWithMinusOne(g), 6);
  PQ_STR(4, "... column 0 afterwards", pq_arr(&g[0][0], 24), "{-1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}"); }
{ int g[4][6] = {{4,4,4,0,0,0},{4,0,0,0,0,0},{4,0,0,0,0,0},{0,0,0,0,0,0}};
  PQ_INT(5, "(0,0) has both: horizontal wins, score", ReplaceWithMinusOne(g), 12);
  PQ_INT(6, "... (1,0) is still 4", g[1][0], 4); }
{ int g[4][6] = {{1,2,1,2,1,2},{2,1,2,1,2,1},{1,2,1,2,1,2},{2,1,2,1,2,1}};
  PQ_INT(7, "no cluster: score", ReplaceWithMinusOne(g), 0); }
{ int g[4][6] = {{-2,-2,-2,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,3,3,3,0,0}};
  PQ_INT(8, "-2 -2 -2 is not a cluster, 3 3 3 is", ReplaceWithMinusOne(g), 9); }
)PQ";
        p.hints = {"Skip values that are not 1..4 with `continue`.",
                   "Count the run to the right with while (col + length < 6 && array[row][col + length] == value) length++;",
                   "If length >= 3: replace, and return value * length immediately. Then do the same downwards."};
        Push(p);
    }
    {
        Problem p;
        p.id = "FallDownAndReplace"; p.title = "Gravity: Fall Down"; p.section = S_GRID; p.difficulty = 2;
        p.statement = "Remove the -1's by making the items above them **fall down** into the empty space, column by column. "
                      "The empty slots created at the top get the value **-2**.";
        p.examples = {{"column (top to bottom) 2, -1, 3, -1", "-2, -2, 2, 3", "2 and 3 fall, keeping their order."}};
        p.exampleCases = 1;
        p.starter = "void FallDownAndReplace(int array[4][6]) {\n    // Write your code here\n}\n";
        p.solution = sources::Text("FallDownAndReplace");
        p.tests = R"PQ(PQ_TOTAL(4);
{ int g[4][6] = {{2,0,0,0,0,0},{-1,0,0,0,0,0},{3,0,0,0,0,0},{-1,0,0,0,0,0}}; FallDownAndReplace(g);
  PQ_STR(1, "column 0 = 2, -1, 3, -1", pq_arr(&g[0][0], 24), "{-2, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0}"); }
{ int g[4][6] = {{0,0,3,1,3,4},{0,0,2,3,4,3},{0,0,1,3,3,2},{0,0,-1,-1,-1,-1}}; FallDownAndReplace(g);
  PQ_STR(2, "Lab1 grid after the bottom row was removed", pq_arr(&g[0][0], 24), "{0, 0, -2, -2, -2, -2, 0, 0, 3, 1, 3, 4, 0, 0, 2, 3, 4, 3, 0, 0, 1, 3, 3, 2}"); }
{ int g[4][6] = {{1,2,3,4,1,2},{1,2,3,4,1,2},{1,2,3,4,1,2},{1,2,3,4,1,2}}; FallDownAndReplace(g);
  PQ_STR(3, "no -1 anywhere: unchanged", pq_arr(&g[0][0], 24), "{1, 2, 3, 4, 1, 2, 1, 2, 3, 4, 1, 2, 1, 2, 3, 4, 1, 2, 1, 2, 3, 4, 1, 2}"); }
{ int g[4][6] = {{-1,0,0,0,0,0},{-1,0,0,0,0,0},{-1,0,0,0,0,0},{-1,0,0,0,0,0}}; FallDownAndReplace(g);
  PQ_STR(4, "a full column of -1", pq_arr(&g[0][0], 24), "{-2, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0}"); }
)PQ";
        p.hints = {"Work one column at a time, from the bottom row up.",
                   "Keep writeRow = 3. Every value that is not -1 is copied to array[writeRow][col] and writeRow--.",
                   "When the column is done, fill rows writeRow..0 with -2."};
        Push(p);
    }

    // ================================================== STRINGS & PARSING ===
    {
        Problem p;
        p.id = "toUpperCase"; p.title = "Upper Case in Place"; p.section = S_STRINGS; p.difficulty = 0;
        p.statement = "Change every lowercase letter of the string to uppercase, in place. Other characters stay the same.";
        p.examples = {{"\"Hello, World 2!\"", "\"HELLO, WORLD 2!\"", ""}, {"\"\"", "\"\"", ""}, {"\"abc\"", "\"ABC\"", ""}};
        p.starter = "void toUpperCase(char s[]) {\n    // Write your code here\n}\n";
        p.solution = "void toUpperCase(char s[]) {\n    for (int i = 0; s[i] != '\\0'; i++) {\n        if (s[i] >= 'a' && s[i] <= 'z') s[i] = s[i] - 'a' + 'A';\n    }\n}\n";
        p.tests = R"PQ(PQ_TOTAL(4);
{ char s[] = "Hello, World 2!"; toUpperCase(s); PQ_STR(1, "\"Hello, World 2!\"", s, "HELLO, WORLD 2!"); }
{ char s[] = ""; toUpperCase(s); PQ_STR(2, "\"\"", s, ""); }
{ char s[] = "abc"; toUpperCase(s); PQ_STR(3, "\"abc\"", s, "ABC"); }
{ char s[] = "zAz{`"; toUpperCase(s); PQ_STR(4, "\"zAz{`\"", s, "ZAZ{`"); }
)PQ";
        p.hints = {"Loop until s[i] == '\\0'.", "A lowercase letter c becomes c - 'a' + 'A'."};
        Push(p);
    }
    {
        Problem p;
        p.id = "isStringPalindrome"; p.title = "Palindrome String"; p.section = S_STRINGS; p.difficulty = 0;
        p.statement = "Return `true` if the string reads the same forwards and backwards (case-sensitive).";
        p.examples = {{"\"racecar\"", "true", ""}, {"\"abca\"", "false", ""}, {"\"\"", "true", "The empty string is a palindrome."}};
        p.starter = "bool isStringPalindrome(char s[]) {\n    // Write your code here\n    return false;\n}\n";
        p.solution = "bool isStringPalindrome(char s[]) {\n    int length = 0;\n    while (s[length] != '\\0') length++;\n"
                     "    for (int i = 0, j = length - 1; i < j; i++, j--) {\n        if (s[i] != s[j]) return false;\n    }\n    return true;\n}\n";
        p.tests = R"PQ(PQ_TOTAL(5);
{ char s[] = "racecar"; PQ_BOOL(1, "\"racecar\"", isStringPalindrome(s), true); }
{ char s[] = "abca"; PQ_BOOL(2, "\"abca\"", isStringPalindrome(s), false); }
{ char s[] = ""; PQ_BOOL(3, "\"\"", isStringPalindrome(s), true); }
{ char s[] = "abba"; PQ_BOOL(4, "\"abba\"", isStringPalindrome(s), true); }
{ char s[] = "Aa"; PQ_BOOL(5, "\"Aa\"", isStringPalindrome(s), false); }
)PQ";
        p.hints = {"Find the length first (count until '\\0').", "Compare s[i] with s[length - 1 - i] for the first half."};
        Push(p);
    }
    {
        Problem p;
        p.id = "FindAllVariables"; p.title = "Find All Variables"; p.section = S_STRINGS; p.difficulty = 2;
        p.statement = "`code` holds a C program as a string (lines separated by `\\n`). Fill `variables` with a **comma separated** list of every "
                      "variable declared as `int` or `char` (one declaration per line), in order.\n\n"
                      "- `int main()` is a function, not a variable.\n- Words like `print` contain 'int' but are not declarations.\n"
                      "- Text inside string literals does not count.";
        p.examples = {{"\"int main() {\\n    int count = 0;\\n    char letter = 'a';\\n}\"", "\"count,letter\"", ""}};
        p.exampleCases = 1;
        p.starter = "#include <string.h>\n\nvoid FindAllVariables(char variables[], char code[]) {\n    variables[0] = '\\0';\n    // Write your code here\n}\n";
        p.solution = sources::Text("wordCounter") + "\n" + sources::Text("FindAllVariables");
        p.tests = R"PQ(PQ_TOTAL(5);
{ char code[] = "int main() {\n    int count = 0;\n    char letter = 'a';\n}\n"; char v[500];
  FindAllVariables(v, code); PQ_STR(1, "main with count and letter", v, "count,letter"); }
{ char code[] = "int main() {\n    int count = 0;\n    char letter = 'a';\n    if (count == 0) {\n        int temp = 5;\n        char* name;\n    }\n    int total = 10;\n    printf(\"int fake\");\n    return 0;\n}\n"; char v[500];
  FindAllVariables(v, code); PQ_STR(2, "nested block, pointer, string literal", v, "count,letter,temp,name,total"); }
{ char code[] = "int main() {\n    print(x);\n    return 0;\n}\n"; char v[500];
  FindAllVariables(v, code); PQ_STR(3, "no declarations ('print' contains int)", v, ""); }
{ char code[] = "int main() {\n    char name[20];\n    int* ptr = 0;\n}\n"; char v[500];
  FindAllVariables(v, code); PQ_STR(4, "array and pointer", v, "name,ptr"); }
{ char code[] = ""; char v[500];
  FindAllVariables(v, code); PQ_STR(5, "empty code", v, ""); }
)PQ";
        p.hints = {"Look for the words int / char that are not part of a longer word.",
                   "After the type, skip spaces and '*', then copy the identifier (letters, digits, _).",
                   "If the next non-space character is '(' it is a function. Use strcat to build the list, adding ',' between names."};
        Push(p);
    }
    {
        Problem p;
        p.id = "FindAllVariablesInScope"; p.title = "Variables in Scope"; p.section = S_STRINGS; p.difficulty = 2;
        p.statement = "Like Find All Variables, but list only the variables that are **in scope after line `lineNumber`** (lines start at 1).\n\n"
                      "Track the brace depth: `{` adds 1, `}` subtracts 1. A variable declared inside a block that has already closed is out of scope.";
        p.examples = {{"lineNumber = 5 (inside the if block)", "\"count,letter,temp\"", ""},
                      {"lineNumber = 7 (after the block closes)", "\"count,letter\"", ""}};
        p.exampleCases = 2;
        p.starter = "#include <string.h>\n\nvoid FindAllVariablesInScope(char variables[], char code[], int lineNumber) {\n    variables[0] = '\\0';\n    // Write your code here\n}\n";
        p.solution = sources::Text("wordCounter") + "\n" + sources::Text("FindAllVariables") + "\n" + sources::Text("FindAllVariablesInScope");
        p.tests = R"PQ(PQ_TOTAL(5);
char code[] = "int main() {\n    int count = 0;\n    char letter = 'a';\n    if (count == 0) {\n        int temp = 5;\n        char* name;\n    }\n    int total = 10;\n    return 0;\n}\n";
char v[500];
FindAllVariablesInScope(v, code, 5); PQ_STR(1, "lineNumber = 5", v, "count,letter,temp");
FindAllVariablesInScope(v, code, 7); PQ_STR(2, "lineNumber = 7", v, "count,letter");
FindAllVariablesInScope(v, code, 6); PQ_STR(3, "lineNumber = 6", v, "count,letter,temp,name");
FindAllVariablesInScope(v, code, 8); PQ_STR(4, "lineNumber = 8", v, "count,letter,total");
FindAllVariablesInScope(v, code, 1); PQ_STR(5, "lineNumber = 1", v, "");
)PQ";
        p.hints = {"Keep arrays names[][] and depths[] used as a stack, plus a depth counter and a line counter.",
                   "On '}': depth--, then pop every variable whose depth is now greater than depth.",
                   "Stop when the line counter passes lineNumber, then join what is left on the stack."};
        Push(p);
    }

    // ============================================================ POINTERS ===
    {
        Problem p;
        p.id = "swap_ints"; p.title = "Swap Two Ints"; p.section = S_POINTERS; p.difficulty = 0;
        p.statement = "Swap the values of two integers through pointers. Called as `swap_ints(&x, &y)`.";
        p.examples = {{"x = 5, y = 10", "x = 10, y = 5", ""}};
        p.exampleCases = 2;
        p.starter = "void swap_ints(int *a, int *b) {\n    // Write your code here\n}\n";
        p.solution = sources::Text("swap_ints");
        p.tests = R"PQ(PQ_TOTAL(4);
{ int x = 5, y = 10; swap_ints(&x, &y); PQ_INT(1, "x = 5, y = 10 -> x", x, 10); PQ_INT(2, "x = 5, y = 10 -> y", y, 5); }
{ int x = -3, y = -3; swap_ints(&x, &y); PQ_INT(3, "x = -3, y = -3 -> x", x, -3); }
{ int a[2] = {1, 2}; swap_ints(&a[0], &a[1]); PQ_STR(4, "swap the two elements of {1, 2}", pq_arr(a, 2), "{2, 1}"); }
)PQ";
        p.hints = {"int temp = *a; *a = *b; *b = temp;"};
        Push(p);
    }
    {
        Problem p;
        p.id = "swap_chars"; p.title = "Swap Two Chars"; p.section = S_POINTERS; p.difficulty = 0;
        p.statement = "Swap two characters through pointers.";
        p.examples = {{"a = 'A', b = 'B'", "a = 'B', b = 'A'", ""}};
        p.exampleCases = 1;
        p.starter = "void swap_chars(char *a, char *b) {\n    // Write your code here\n}\n";
        p.solution = sources::Text("swap_chars");
        p.tests = R"PQ(PQ_TOTAL(2);
{ char a = 'A', b = 'B'; swap_chars(&a, &b); char r[3] = {a, b, 0}; PQ_STR(1, "a = 'A', b = 'B'", r, "BA"); }
{ char s[] = "xy"; swap_chars(&s[0], &s[1]); PQ_STR(2, "swap the chars of \"xy\"", s, "yx"); }
)PQ";
        p.hints = {"Exactly like swap_ints, with char."};
        Push(p);
    }
    {
        Problem p;
        p.id = "minMax"; p.title = "Min and Max through Pointers"; p.section = S_POINTERS; p.difficulty = 1;
        p.statement = "A function can only return one value. Return **both** the minimum and the maximum of the array by writing them "
                      "through the pointers `min` and `max`.";
        p.examples = {{"arr = {3, 9, 2, 8, 5}", "*min = 2, *max = 9", ""}, {"arr = {-5}", "*min = -5, *max = -5", ""}};
        p.exampleCases = 2;
        p.starter = "void minMax(int arr[], int size, int *min, int *max) {\n    // Write your code here\n}\n";
        p.solution = "void minMax(int arr[], int size, int *min, int *max) {\n    *min = arr[0];\n    *max = arr[0];\n"
                     "    for (int i = 1; i < size; i++) {\n        if (arr[i] < *min) *min = arr[i];\n        if (arr[i] > *max) *max = arr[i];\n    }\n}\n";
        p.tests = R"PQ(PQ_TOTAL(3);
{ int a[] = {3, 9, 2, 8, 5}; int mn = 0, mx = 0; minMax(a, 5, &mn, &mx); int r[2] = {mn, mx}; PQ_STR(1, "{3, 9, 2, 8, 5} -> {min, max}", pq_arr(r, 2), "{2, 9}"); }
{ int a[] = {-5}; int mn = 0, mx = 0; minMax(a, 1, &mn, &mx); int r[2] = {mn, mx}; PQ_STR(2, "{-5} -> {min, max}", pq_arr(r, 2), "{-5, -5}"); }
{ int a[] = {-1, -7, -3}; int mn = 0, mx = 0; minMax(a, 3, &mn, &mx); int r[2] = {mn, mx}; PQ_STR(3, "{-1, -7, -3} -> {min, max}", pq_arr(r, 2), "{-7, -1}"); }
)PQ";
        p.hints = {"Start both at arr[0]: *min = arr[0];", "Compare and write through the pointers: if (arr[i] < *min) *min = arr[i];"};
        Push(p);
    }

    // ================================================== POINTER ARITHMETIC ===
    const std::vector<std::string> noIdx = {"["};
    const std::string noIdxText = "The exam rule: no array indexing (arr[i]) - use pointer arithmetic like *(p + i) or p++.";
    {
        Problem p;
        p.id = "find_max"; p.title = "Find the Maximum (pointers)"; p.section = S_PTRARITH; p.difficulty = 0;
        p.statement = "Return the largest value in the array using **pointer arithmetic only** (no `arr[i]`).";
        p.examples = {{"arr = {3, 9, 2, 8, 5}", "9", ""}, {"arr = {-5, -2, -9}", "-2", ""}, {"arr = {4}", "4", ""}};
        p.starter = "int find_max(int *arr, int size) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = sources::Text("find_max");
        p.tests = R"PQ(PQ_TOTAL(4);
{ int a[] = {3, 9, 2, 8, 5}; PQ_INT(1, "{3, 9, 2, 8, 5}", find_max(a, 5), 9); }
{ int a[] = {-5, -2, -9}; PQ_INT(2, "{-5, -2, -9}", find_max(a, 3), -2); }
{ int a[] = {4}; PQ_INT(3, "{4}", find_max(a, 1), 4); }
{ int a[] = {1, 2, 3, 4, 100}; PQ_INT(4, "{1, 2, 3, 4, 100}", find_max(a, 5), 100); }
)PQ";
        p.forbid = noIdx; p.ruleText = noIdxText;
        p.hints = {"int max = *arr; then walk a pointer: for (int *p = arr + 1; p < arr + size; p++)"};
        Push(p);
    }
    {
        Problem p;
        p.id = "sum_array"; p.title = "Sum an Array (pointers)"; p.section = S_PTRARITH; p.difficulty = 0;
        p.statement = "Return the sum of the elements using pointer arithmetic only.";
        p.examples = {{"arr = {1, 2, 3, 4}", "10", ""}, {"size = 0", "0", ""}, {"arr = {-5, 5}", "0", ""}};
        p.starter = "int sum_array(int *arr, int size) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = sources::Text("sum_array");
        p.tests = R"PQ(PQ_TOTAL(4);
{ int a[] = {1, 2, 3, 4}; PQ_INT(1, "{1, 2, 3, 4}", sum_array(a, 4), 10); }
{ int a[] = {1}; PQ_INT(2, "size = 0", sum_array(a, 0), 0); }
{ int a[] = {-5, 5}; PQ_INT(3, "{-5, 5}", sum_array(a, 2), 0); }
{ int a[] = {100, 200, 300}; PQ_INT(4, "{100, 200, 300}", sum_array(a, 3), 600); }
)PQ";
        p.forbid = noIdx; p.ruleText = noIdxText;
        p.hints = {"for (int *p = arr; p < arr + size; p++) sum += *p;"};
        Push(p);
    }
    {
        Problem p;
        p.id = "reverse_array"; p.title = "Reverse with Two Pointers"; p.section = S_PTRARITH; p.difficulty = 0;
        p.statement = "Reverse the array in place using two pointers and no indexing.";
        p.examples = {{"arr = {1, 2, 3, 4, 5}", "{5, 4, 3, 2, 1}", ""}, {"arr = {1, 2}", "{2, 1}", ""}, {"arr = {7}", "{7}", ""}};
        p.starter = "void reverse_array(int *arr, int size) {\n    // Write your code here\n}\n";
        p.solution = sources::Text("reverse_array");
        p.tests = R"PQ(PQ_TOTAL(4);
{ int a[] = {1, 2, 3, 4, 5}; reverse_array(a, 5); PQ_STR(1, "{1, 2, 3, 4, 5}", pq_arr(a, 5), "{5, 4, 3, 2, 1}"); }
{ int a[] = {1, 2}; reverse_array(a, 2); PQ_STR(2, "{1, 2}", pq_arr(a, 2), "{2, 1}"); }
{ int a[] = {7}; reverse_array(a, 1); PQ_STR(3, "{7}", pq_arr(a, 1), "{7}"); }
{ int a[] = {4, 3, 2, 1}; reverse_array(a, 4); PQ_STR(4, "{4, 3, 2, 1}", pq_arr(a, 4), "{1, 2, 3, 4}"); }
)PQ";
        p.forbid = noIdx; p.ruleText = noIdxText;
        p.hints = {"int *left = arr; int *right = arr + size - 1;", "Swap *left and *right, then left++ and right-- while left < right."};
        Push(p);
    }
    {
        Problem p;
        p.id = "strlen_ptr"; p.title = "String Length (pointers)"; p.section = S_PTRARITH; p.difficulty = 0;
        p.statement = "Return the length of a null-terminated string using pointers.";
        p.examples = {{"str = \"Hello\"", "5", ""}, {"str = \"\"", "0", ""}, {"str = \"Hello, world!\"", "13", ""}};
        p.starter = "int strlen_ptr(char *str) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = sources::Text("strlen_ptr");
        p.tests = R"PQ(PQ_TOTAL(4);
{ char s[] = "Hello"; PQ_INT(1, "\"Hello\"", strlen_ptr(s), 5); }
{ char s[] = ""; PQ_INT(2, "\"\"", strlen_ptr(s), 0); }
{ char s[] = "Hello, world!"; PQ_INT(3, "\"Hello, world!\"", strlen_ptr(s), 13); }
{ char s[] = "a b"; PQ_INT(4, "\"a b\"", strlen_ptr(s), 3); }
)PQ";
        p.forbid = noIdx; p.ruleText = noIdxText;
        p.hints = {"Walk a pointer p until *p == '\\0'.", "The length is p - str (pointer subtraction)."};
        Push(p);
    }
    {
        Problem p;
        p.id = "count_vowels"; p.title = "Count Vowels (pointers)"; p.section = S_PTRARITH; p.difficulty = 0;
        p.statement = "Count the vowels (a, e, i, o, u - upper or lower case) using pointers.\n\n"
                      "Note: the exam brief says `\"OpenAI\"` has 3 vowels, but O, e, A **and I** are vowels, so the answer is **4**.";
        p.examples = {{"str = \"OpenAI\"", "4", "O, e, A, I"}, {"str = \"rhythm\"", "0", ""}, {"str = \"AEIOU aeiou\"", "10", ""}};
        p.starter = "int count_vowels(char *str) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = sources::Text("count_vowels");
        p.tests = R"PQ(PQ_TOTAL(4);
{ char s[] = "OpenAI"; PQ_INT(1, "\"OpenAI\"", count_vowels(s), 4); }
{ char s[] = "rhythm"; PQ_INT(2, "\"rhythm\"", count_vowels(s), 0); }
{ char s[] = "AEIOU aeiou"; PQ_INT(3, "\"AEIOU aeiou\"", count_vowels(s), 10); }
{ char s[] = ""; PQ_INT(4, "\"\"", count_vowels(s), 0); }
)PQ";
        p.forbid = noIdx; p.ruleText = noIdxText;
        p.hints = {"for (char *p = str; *p != '\\0'; p++)", "Convert to lowercase first, then compare with 'a', 'e', 'i', 'o', 'u'."};
        Push(p);
    }
    {
        Problem p;
        p.id = "copy_string"; p.title = "Copy a String (pointers)"; p.section = S_PTRARITH; p.difficulty = 0;
        p.statement = "Copy `src` into `dest` using pointers (dest is big enough). Don't forget the terminator.";
        p.examples = {{"src = \"Hello\"", "dest = \"Hello\"", ""}, {"src = \"\"", "dest = \"\"", ""}};
        p.exampleCases = 2;
        p.starter = "void copy_string(char *dest, char *src) {\n    // Write your code here\n}\n";
        p.solution = sources::Text("copy_string");
        p.tests = R"PQ(PQ_TOTAL(3);
{ char src[] = "Hello"; char dest[20]; memset(dest, 'X', sizeof(dest)); copy_string(dest, src); dest[19] = 0; PQ_STR(1, "src = \"Hello\"", dest, "Hello"); }
{ char src[] = ""; char dest[20]; memset(dest, 'X', sizeof(dest)); copy_string(dest, src); dest[19] = 0; PQ_STR(2, "src = \"\"", dest, ""); }
{ char src[] = "pointers!"; char dest[20]; memset(dest, 'X', sizeof(dest)); copy_string(dest, src); dest[19] = 0; PQ_STR(3, "src = \"pointers!\"", dest, "pointers!"); }
)PQ";
        p.forbid = noIdx; p.ruleText = noIdxText;
        p.hints = {"while (*src != '\\0') { *dest = *src; dest++; src++; }", "After the loop: *dest = '\\0';"};
        Push(p);
    }
    {
        Problem p;
        p.id = "concat_strings"; p.title = "Concatenate (pointers)"; p.section = S_PTRARITH; p.difficulty = 1;
        p.statement = "Append `src` to the end of `dest` using pointers (dest has enough space).";
        p.examples = {{"dest = \"Hello, \", src = \"World!\"", "dest = \"Hello, World!\"", ""}, {"dest = \"\", src = \"abc\"", "dest = \"abc\"", ""}};
        p.exampleCases = 2;
        p.starter = "void concat_strings(char *dest, char *src) {\n    // Write your code here\n}\n";
        p.solution = sources::Text("concat_strings");
        p.tests = R"PQ(PQ_TOTAL(3);
{ char d[40] = "Hello, "; char s[] = "World!"; concat_strings(d, s); PQ_STR(1, "\"Hello, \" + \"World!\"", d, "Hello, World!"); }
{ char d[40] = ""; char s[] = "abc"; concat_strings(d, s); PQ_STR(2, "\"\" + \"abc\"", d, "abc"); }
{ char d[40] = "abc"; char s[] = ""; concat_strings(d, s); PQ_STR(3, "\"abc\" + \"\"", d, "abc"); }
)PQ";
        p.forbid = noIdx; p.ruleText = noIdxText;
        p.hints = {"First move dest to its '\\0'.", "Then copy src there, exactly like copy_string."};
        Push(p);
    }
    {
        Problem p;
        p.id = "compare_strings"; p.title = "Compare Strings (pointers)"; p.section = S_PTRARITH; p.difficulty = 1;
        p.statement = "Compare two strings lexicographically. Return 0 if equal, a **negative** value if `str1` comes first, a **positive** value otherwise.";
        p.examples = {{"\"apple\", \"banana\"", "negative", ""}, {"\"apple\", \"apple\"", "0", ""}, {"\"app\", \"apple\"", "negative", "A prefix comes first."}};
        p.starter = "int compare_strings(char *str1, char *str2) {\n    // Write your code here\n    return 0;\n}\n";
        p.solution = sources::Text("compare_strings");
        p.tests = R"PQ(PQ_TOTAL(5);
{ char a[] = "apple", b[] = "banana"; int r = compare_strings(a, b); PQ_STR(1, "\"apple\", \"banana\"", r < 0 ? "negative" : r > 0 ? "positive" : "0", "negative"); }
{ char a[] = "apple", b[] = "apple"; int r = compare_strings(a, b); PQ_STR(2, "\"apple\", \"apple\"", r < 0 ? "negative" : r > 0 ? "positive" : "0", "0"); }
{ char a[] = "app", b[] = "apple"; int r = compare_strings(a, b); PQ_STR(3, "\"app\", \"apple\"", r < 0 ? "negative" : r > 0 ? "positive" : "0", "negative"); }
{ char a[] = "banana", b[] = "apple"; int r = compare_strings(a, b); PQ_STR(4, "\"banana\", \"apple\"", r < 0 ? "negative" : r > 0 ? "positive" : "0", "positive"); }
{ char a[] = "", b[] = ""; int r = compare_strings(a, b); PQ_STR(5, "\"\", \"\"", r < 0 ? "negative" : r > 0 ? "positive" : "0", "0"); }
)PQ";
        p.forbid = noIdx; p.ruleText = noIdxText;
        p.hints = {"Move both pointers while the chars are equal and not '\\0'.", "Return *str1 - *str2 (cast to unsigned char)."};
        Push(p);
    }

    // ================================================= MEMORY MANAGEMENT ===
    {
        Problem p;
        p.id = "createArray"; p.title = "Create an Array on the Heap"; p.section = S_MEMORY; p.difficulty = 0; p.cppOnly = true;
        p.statement = "Allocate an array of `n` ints with `new`, fill every element with `value` and return it. "
                      "The caller frees it with `delete[]`.";
        p.examples = {{"n = 4, value = 7", "{7, 7, 7, 7}", ""}, {"n = 1, value = 0", "{0}", ""}};
        p.exampleCases = 2;
        p.starter = "int* createArray(int n, int value) {\n    // Write your code here\n    return nullptr;\n}\n";
        p.solution = "int* createArray(int n, int value) {\n    int* arr = new int[n];\n    for (int i = 0; i < n; i++) {\n        arr[i] = value;\n    }\n    return arr;\n}\n";
        p.tests = R"PQ(PQ_TOTAL(3);
{ int* a = createArray(4, 7); PQ_STR(1, "n = 4, value = 7", a ? pq_arr(a, 4) : "nullptr", "{7, 7, 7, 7}"); delete[] a; }
{ int* a = createArray(1, 0); PQ_STR(2, "n = 1, value = 0", a ? pq_arr(a, 1) : "nullptr", "{0}"); delete[] a; }
{ int* a = createArray(6, -1); PQ_STR(3, "n = 6, value = -1", a ? pq_arr(a, 6) : "nullptr", "{-1, -1, -1, -1, -1, -1}"); delete[] a; }
)PQ";
        p.hints = {"int* arr = new int[n]; - memory that survives after the function returns.",
                   "Returning the address of a local array would create a dangling pointer."};
        Push(p);
    }
    {
        Problem p;
        p.id = "growArray"; p.title = "Grow an Array"; p.section = S_MEMORY; p.difficulty = 1; p.cppOnly = true;
        p.statement = "Return a **new** array of `newSize` ints: copy the `oldSize` old values, set the extra slots to 0, and free the old array with `delete[]`.";
        p.examples = {{"old = {1, 2, 3}, newSize = 5", "{1, 2, 3, 0, 0}", ""}};
        p.exampleCases = 1;
        p.starter = "int* growArray(int* old, int oldSize, int newSize) {\n    // Write your code here\n    return nullptr;\n}\n";
        p.solution = "int* growArray(int* old, int oldSize, int newSize) {\n    int* bigger = new int[newSize];\n"
                     "    for (int i = 0; i < newSize; i++) {\n        bigger[i] = i < oldSize ? old[i] : 0;\n    }\n"
                     "    delete[] old;      // free the old block: no leak\n    return bigger;\n}\n";
        p.tests = R"PQ(PQ_TOTAL(3);
{ int* a = new int[3]; a[0] = 1; a[1] = 2; a[2] = 3; int* b = growArray(a, 3, 5); PQ_STR(1, "{1, 2, 3} -> 5", b ? pq_arr(b, 5) : "nullptr", "{1, 2, 3, 0, 0}"); delete[] b; }
{ int* a = new int[1]; a[0] = 9; int* b = growArray(a, 1, 1); PQ_STR(2, "{9} -> 1", b ? pq_arr(b, 1) : "nullptr", "{9}"); delete[] b; }
{ int* a = new int[2]; a[0] = -1; a[1] = -2; int* b = growArray(a, 2, 4); PQ_STR(3, "{-1, -2} -> 4", b ? pq_arr(b, 4) : "nullptr", "{-1, -2, 0, 0}"); delete[] b; }
)PQ";
        p.hints = {"new int[newSize], copy with a loop, delete[] old, return the new pointer."};
        Push(p);
    }
    {
        Problem p;
        p.id = "create2D"; p.title = "2D Array on the Heap"; p.section = S_MEMORY; p.difficulty = 1; p.cppOnly = true;
        p.statement = "Write `create2D(rows, cols)`: allocate a 2D array with `new` (an array of row pointers, then each row) and fill it so that "
                      "`a[r][c] = r * cols + c + 1`. Also write `free2D(a, rows)` that frees everything in the right order.";
        p.examples = {{"rows = 2, cols = 3", "{1, 2, 3}, {4, 5, 6}", ""}};
        p.exampleCases = 1;
        p.starter = "int** create2D(int rows, int cols) {\n    // Write your code here\n    return nullptr;\n}\n\nvoid free2D(int** a, int rows) {\n    // Write your code here\n}\n";
        p.solution = "int** create2D(int rows, int cols) {\n    int** a = new int*[rows];\n    for (int r = 0; r < rows; r++) {\n"
                     "        a[r] = new int[cols];\n        for (int c = 0; c < cols; c++) a[r][c] = r * cols + c + 1;\n    }\n    return a;\n}\n\n"
                     "void free2D(int** a, int rows) {\n    for (int r = 0; r < rows; r++) delete[] a[r];   // rows first\n    delete[] a;                                    // then the pointers\n}\n";
        p.tests = R"PQ(PQ_TOTAL(3);
{ int** a = create2D(2, 3); int flat[6] = {0}; if (a) for (int r = 0; r < 2; r++) for (int c = 0; c < 3; c++) flat[r * 3 + c] = a[r][c];
  PQ_STR(1, "rows = 2, cols = 3", a ? pq_arr(flat, 6) : "nullptr", "{1, 2, 3, 4, 5, 6}"); if (a) free2D(a, 2); }
{ int** a = create2D(1, 1); PQ_STR(2, "rows = 1, cols = 1", a ? pq_arr(a[0], 1) : "nullptr", "{1}"); if (a) free2D(a, 1); }
{ int** a = create2D(3, 2); int flat[6] = {0}; if (a) for (int r = 0; r < 3; r++) for (int c = 0; c < 2; c++) flat[r * 2 + c] = a[r][c];
  PQ_STR(3, "rows = 3, cols = 2", a ? pq_arr(flat, 6) : "nullptr", "{1, 2, 3, 4, 5, 6}"); if (a) free2D(a, 3); }
)PQ";
        p.hints = {"int** a = new int*[rows]; then for each row: a[r] = new int[cols];",
                   "Free in reverse: delete[] each a[r], then delete[] a."};
        Push(p);
    }

    // ======================================================= LINKED LISTS ===
    {
        Problem p;
        p.id = "list_insert_begin_end"; p.title = "Linked List: Insert at Beginning and End"; p.section = S_LISTS; p.difficulty = 1; p.cppOnly = true;
        p.statement = "Complete `InsertAtBeginning` and `InsertAtEnd` of the singly linked list. `head` points to the first node; the last node's `next` is `nullptr`.";
        p.examples = {{"InsertAtBeginning 3, 2, 1", "{1, 2, 3}", ""}, {"InsertAtEnd 1, 2, 3", "{1, 2, 3}", ""},
                      {"InsertAtEnd(10), InsertAtBeginning(5), InsertAtEnd(20)", "{5, 10, 20}", ""}};
        p.starter = std::string(kNode) + R"PQ(
class LinkedList {
public:
    Node* head = nullptr;

    void InsertAtBeginning(int value) {
        // Write your code here
    }

    void InsertAtEnd(int value) {
        // Write your code here
    }
};
)PQ";
        p.solution = std::string(kNode) + R"PQ(
class LinkedList {
public:
    Node* head = nullptr;

    void InsertAtBeginning(int value) {
        Node* newNode = new Node(value);
        newNode->next = head;
        head = newNode;
    }

    void InsertAtEnd(int value) {
        Node* newNode = new Node(value);
        if (head == nullptr) {
            head = newNode;
            return;
        }
        Node* temp = head;
        while (temp->next != nullptr) temp = temp->next;
        temp->next = newNode;
    }
};
)PQ";
        p.tests = std::string(kListHelpers) + "//@@MAIN\n" + R"PQ(PQ_TOTAL(5);
{ LinkedList l; l.InsertAtBeginning(3); l.InsertAtBeginning(2); l.InsertAtBeginning(1); PQ_STR(1, "InsertAtBeginning 3, 2, 1", pq_list(l.head), "{1, 2, 3}"); }
{ LinkedList l; l.InsertAtEnd(1); l.InsertAtEnd(2); l.InsertAtEnd(3); PQ_STR(2, "InsertAtEnd 1, 2, 3", pq_list(l.head), "{1, 2, 3}"); }
{ LinkedList l; l.InsertAtEnd(10); l.InsertAtBeginning(5); l.InsertAtEnd(20); PQ_STR(3, "InsertAtEnd(10), InsertAtBeginning(5), InsertAtEnd(20)", pq_list(l.head), "{5, 10, 20}"); }
{ LinkedList l; l.InsertAtBeginning(1); PQ_STR(4, "InsertAtBeginning(1) on an empty list", pq_list(l.head), "{1}"); }
{ LinkedList l; PQ_STR(5, "a new list is empty", pq_list(l.head), "{}"); }
)PQ";
        p.hints = {"Beginning: new node -> newNode->next = head -> head = newNode. The order matters!",
                   "End: if the list is empty, head = newNode. Otherwise walk temp while temp->next != nullptr, then temp->next = newNode."};
        Push(p);
    }
    {
        Problem p;
        p.id = "list_insert_position"; p.title = "Linked List: Insert at Position"; p.section = S_LISTS; p.difficulty = 1; p.cppOnly = true;
        p.statement = "Complete `InsertAtPosition(value, position)`. Positions start at 0. Return `false` if the position is not between 0 and the size.";
        p.examples = {{"{5, 10, 20}, InsertAtPosition(15, 2)", "{5, 10, 15, 20}", ""}, {"{5, 10}, InsertAtPosition(1, 0)", "{1, 5, 10}", ""},
                      {"{5, 10}, InsertAtPosition(9, 5)", "false", ""}};
        std::string body = std::string(kNode) + R"PQ(
class LinkedList {
public:
    Node* head = nullptr;

    void InsertAtBeginning(int value) {
        Node* newNode = new Node(value);
        newNode->next = head;
        head = newNode;
    }

    int Size() {
        int count = 0;
        for (Node* t = head; t != nullptr; t = t->next) count++;
        return count;
    }

    bool InsertAtPosition(int value, int position) {
)PQ";
        p.starter = body + "        // Write your code here\n        return false;\n    }\n};\n";
        p.solution = body + R"PQ(        if (position < 0 || position > Size()) return false;
        if (position == 0) {
            InsertAtBeginning(value);
            return true;
        }
        Node* newNode = new Node(value);
        Node* temp = head;
        for (int i = 0; i < position - 1; i++) temp = temp->next;
        newNode->next = temp->next;
        temp->next = newNode;
        return true;
    }
};
)PQ";
        p.tests = std::string(kListHelpers) + "//@@MAIN\n" + R"PQ(PQ_TOTAL(7);
{ LinkedList l; l.InsertAtBeginning(20); l.InsertAtBeginning(10); l.InsertAtBeginning(5); l.InsertAtPosition(15, 2); PQ_STR(1, "{5, 10, 20}, InsertAtPosition(15, 2)", pq_list(l.head), "{5, 10, 15, 20}"); }
{ LinkedList l; l.InsertAtBeginning(10); l.InsertAtBeginning(5); l.InsertAtPosition(1, 0); PQ_STR(2, "{5, 10}, InsertAtPosition(1, 0)", pq_list(l.head), "{1, 5, 10}"); }
{ LinkedList l; l.InsertAtBeginning(10); l.InsertAtBeginning(5); PQ_BOOL(3, "{5, 10}, InsertAtPosition(9, 5) returns", l.InsertAtPosition(9, 5), false); }
{ LinkedList l; l.InsertAtBeginning(10); l.InsertAtBeginning(5); l.InsertAtPosition(99, 2); PQ_STR(4, "{5, 10}, InsertAtPosition(99, 2) (the end)", pq_list(l.head), "{5, 10, 99}"); }
{ LinkedList l; PQ_BOOL(5, "empty list, InsertAtPosition(7, 0) returns", l.InsertAtPosition(7, 0), true); PQ_STR(6, "... list afterwards", pq_list(l.head), "{7}"); }
{ LinkedList l; l.InsertAtBeginning(1); PQ_BOOL(7, "{1}, InsertAtPosition(2, -1) returns", l.InsertAtPosition(2, -1), false); }
)PQ";
        p.hints = {"Position 0 is just InsertAtBeginning.", "Walk temp position - 1 steps: it stops on the node BEFORE the gap.",
                   "newNode->next = temp->next; FIRST, then temp->next = newNode;"};
        Push(p);
    }
    {
        Problem p;
        p.id = "list_delete_position"; p.title = "Linked List: Delete at Position"; p.section = S_LISTS; p.difficulty = 1; p.cppOnly = true;
        p.statement = "Complete `DeleteAtPosition(position)`: unlink the node at that position and free it with `delete`. Return `false` if the position does not exist.";
        p.examples = {{"{1, 2, 3}, DeleteAtPosition(1)", "{1, 3}", ""}, {"{1, 2, 3}, DeleteAtPosition(0)", "{2, 3}", ""}, {"{1}, DeleteAtPosition(3)", "false", ""}};
        std::string body = std::string(kNode) + R"PQ(
class LinkedList {
public:
    Node* head = nullptr;

    void InsertAtEnd(int value) {
        Node* newNode = new Node(value);
        if (head == nullptr) { head = newNode; return; }
        Node* temp = head;
        while (temp->next != nullptr) temp = temp->next;
        temp->next = newNode;
    }

    bool DeleteAtPosition(int position) {
)PQ";
        p.starter = body + "        // Write your code here\n        return false;\n    }\n};\n";
        p.solution = body + R"PQ(        if (head == nullptr || position < 0) return false;
        if (position == 0) {
            Node* toDelete = head;
            head = head->next;
            delete toDelete;
            return true;
        }
        Node* temp = head;
        for (int i = 0; i < position - 1; i++) {
            if (temp->next == nullptr) return false;
            temp = temp->next;
        }
        if (temp->next == nullptr) return false;
        Node* toDelete = temp->next;
        temp->next = toDelete->next;
        delete toDelete;
        return true;
    }
};
)PQ";
        p.tests = std::string(kListHelpers) + "//@@MAIN\n" + R"PQ(PQ_TOTAL(7);
{ LinkedList l; l.InsertAtEnd(1); l.InsertAtEnd(2); l.InsertAtEnd(3); l.DeleteAtPosition(1); PQ_STR(1, "{1, 2, 3}, DeleteAtPosition(1)", pq_list(l.head), "{1, 3}"); }
{ LinkedList l; l.InsertAtEnd(1); l.InsertAtEnd(2); l.InsertAtEnd(3); l.DeleteAtPosition(0); PQ_STR(2, "{1, 2, 3}, DeleteAtPosition(0)", pq_list(l.head), "{2, 3}"); }
{ LinkedList l; l.InsertAtEnd(1); PQ_BOOL(3, "{1}, DeleteAtPosition(3) returns", l.DeleteAtPosition(3), false); }
{ LinkedList l; l.InsertAtEnd(1); l.InsertAtEnd(2); l.InsertAtEnd(3); l.DeleteAtPosition(2); PQ_STR(4, "{1, 2, 3}, DeleteAtPosition(2) (last)", pq_list(l.head), "{1, 2}"); }
{ LinkedList l; l.InsertAtEnd(1); PQ_BOOL(5, "{1}, DeleteAtPosition(0) returns", l.DeleteAtPosition(0), true); PQ_STR(6, "... list afterwards", pq_list(l.head), "{}"); }
{ LinkedList l; PQ_BOOL(7, "empty list, DeleteAtPosition(0) returns", l.DeleteAtPosition(0), false); }
)PQ";
        p.hints = {"Position 0: save head, move head to head->next, delete the saved node.",
                   "Otherwise walk temp to the node BEFORE, then temp->next = toDelete->next; delete toDelete;",
                   "Check for nullptr while walking so a too-big position returns false instead of crashing."};
        Push(p);
    }
    {
        Problem p;
        p.id = "list_reverse"; p.title = "Linked List: Reverse"; p.section = S_LISTS; p.difficulty = 2; p.cppOnly = true;
        p.statement = "Complete `Reverse()`: reverse the list **in place** by flipping the `next` pointers. Do not create new nodes.";
        p.examples = {{"{1, 2, 3}", "{3, 2, 1}", ""}, {"{7}", "{7}", ""}, {"{}", "{}", ""}};
        std::string body = std::string(kNode) + R"PQ(
class LinkedList {
public:
    Node* head = nullptr;

    void InsertAtEnd(int value) {
        Node* newNode = new Node(value);
        if (head == nullptr) { head = newNode; return; }
        Node* temp = head;
        while (temp->next != nullptr) temp = temp->next;
        temp->next = newNode;
    }

    void Reverse() {
)PQ";
        p.starter = body + "        // Write your code here\n    }\n};\n";
        p.solution = body + R"PQ(        Node* prev = nullptr;
        Node* current = head;
        while (current != nullptr) {
            Node* nextNode = current->next;
            current->next = prev;
            prev = current;
            current = nextNode;
        }
        head = prev;
    }
};
)PQ";
        p.tests = std::string(kListHelpers) + "//@@MAIN\n" + R"PQ(PQ_TOTAL(5);
{ LinkedList l; l.InsertAtEnd(1); l.InsertAtEnd(2); l.InsertAtEnd(3); l.Reverse(); PQ_STR(1, "{1, 2, 3}", pq_list(l.head), "{3, 2, 1}"); }
{ LinkedList l; l.InsertAtEnd(7); l.Reverse(); PQ_STR(2, "{7}", pq_list(l.head), "{7}"); }
{ LinkedList l; l.Reverse(); PQ_STR(3, "{}", pq_list(l.head), "{}"); }
{ LinkedList l; l.InsertAtEnd(1); l.InsertAtEnd(2); l.Reverse(); l.Reverse(); PQ_STR(4, "{1, 2} reversed twice", pq_list(l.head), "{1, 2}"); }
{ LinkedList l; Node* first = nullptr; l.InsertAtEnd(4); l.InsertAtEnd(5); l.InsertAtEnd(6); first = l.head; l.Reverse();
  Node* last = l.head; while (last && last->next) last = last->next; PQ_BOOL(5, "no new nodes: the old head is now the last node", last == first, true); }
)PQ";
        p.hints = {"Use three pointers: prev (nullptr), current (head) and nextNode.",
                   "In the loop: save nextNode = current->next; flip current->next = prev; move prev and current forward.",
                   "At the end, head = prev."};
        Push(p);
    }
    {
        Problem p;
        p.id = "dlist_insert_begin_end"; p.title = "Doubly Linked List: Insert at Beginning and End"; p.section = S_LISTS; p.difficulty = 1; p.cppOnly = true;
        p.statement = "Each `DoublyLinkedNode` has `prevPointer` and `nextPointer`. The list keeps `head` **and** `tail`. "
                      "Complete `InsertAtBeginning` and `InsertAtEnd` (InsertAtEnd must not walk the list: use `tail`).";
        p.examples = {{"InsertAtEnd 1, 2, 3", "forward {1, 2, 3}, backward {3, 2, 1}", ""},
                      {"InsertAtBeginning 3, 2, 1", "forward {1, 2, 3}", ""}, {"one node", "head == tail", ""}};
        p.starter = std::string(kDNode) + R"PQ(
class DoublyLinkedList {
public:
    DoublyLinkedNode* head = nullptr;
    DoublyLinkedNode* tail = nullptr;

    void InsertAtBeginning(int value) {
        // Write your code here
    }

    void InsertAtEnd(int value) {
        // Write your code here
    }
};
)PQ";
        p.solution = std::string(kDNode) + R"PQ(
class DoublyLinkedList {
public:
    DoublyLinkedNode* head = nullptr;
    DoublyLinkedNode* tail = nullptr;

    void InsertAtBeginning(int value) {
        DoublyLinkedNode* newNode = new DoublyLinkedNode(value);
        if (head == nullptr) {
            head = tail = newNode;
            return;
        }
        newNode->nextPointer = head;
        head->prevPointer = newNode;
        head = newNode;
    }

    void InsertAtEnd(int value) {
        DoublyLinkedNode* newNode = new DoublyLinkedNode(value);
        if (tail == nullptr) {
            head = tail = newNode;
            return;
        }
        newNode->prevPointer = tail;
        tail->nextPointer = newNode;
        tail = newNode;
    }
};
)PQ";
        p.tests = std::string(kDListHelpers) + "//@@MAIN\n" + R"PQ(PQ_TOTAL(6);
{ DoublyLinkedList l; l.InsertAtEnd(1); l.InsertAtEnd(2); l.InsertAtEnd(3);
  PQ_STR(1, "InsertAtEnd 1, 2, 3: forward", pq_fwd(l.head), "{1, 2, 3}"); PQ_STR(2, "InsertAtEnd 1, 2, 3: backward from tail", pq_bwd(l.tail), "{3, 2, 1}"); }
{ DoublyLinkedList l; l.InsertAtBeginning(3); l.InsertAtBeginning(2); l.InsertAtBeginning(1);
  PQ_STR(3, "InsertAtBeginning 3, 2, 1: forward", pq_fwd(l.head), "{1, 2, 3}"); PQ_STR(4, "InsertAtBeginning 3, 2, 1: backward from tail", pq_bwd(l.tail), "{3, 2, 1}"); }
{ DoublyLinkedList l; l.InsertAtBeginning(5); PQ_BOOL(5, "one node: head == tail", l.head != nullptr && l.head == l.tail, true); }
{ DoublyLinkedList l; l.InsertAtEnd(2); l.InsertAtBeginning(1); l.InsertAtEnd(3);
  PQ_BOOL(6, "mixed: head->prevPointer and tail->nextPointer are nullptr", l.head && l.tail && l.head->prevPointer == nullptr && l.tail->nextPointer == nullptr, true); }
)PQ";
        p.hints = {"Empty list: head = tail = newNode.",
                   "Beginning: newNode->nextPointer = head; head->prevPointer = newNode; head = newNode;",
                   "End: newNode->prevPointer = tail; tail->nextPointer = newNode; tail = newNode;"};
        Push(p);
    }
    {
        Problem p;
        p.id = "dlist_insert_position"; p.title = "Doubly Linked List: Insert at Position"; p.section = S_LISTS; p.difficulty = 2; p.cppOnly = true;
        p.statement = "Complete `InsertAtPosition(value, position)` for the doubly linked list. Four links change for a middle insert. "
                      "Keep `head` and `tail` correct and return `false` for a position outside 0..size.";
        p.examples = {{"{1, 2, 4}, InsertAtPosition(3, 2)", "forward {1, 2, 3, 4}, backward {4, 3, 2, 1}", ""},
                      {"{1, 2}, InsertAtPosition(0, 0)", "{0, 1, 2}", ""}, {"{1, 2}, InsertAtPosition(3, 2)", "{1, 2, 3} (new tail)", ""}};
        std::string body = std::string(kDNode) + R"PQ(
class DoublyLinkedList {
public:
    DoublyLinkedNode* head = nullptr;
    DoublyLinkedNode* tail = nullptr;

    void InsertAtBeginning(int value) {
        DoublyLinkedNode* newNode = new DoublyLinkedNode(value);
        if (head == nullptr) { head = tail = newNode; return; }
        newNode->nextPointer = head;
        head->prevPointer = newNode;
        head = newNode;
    }

    void InsertAtEnd(int value) {
        DoublyLinkedNode* newNode = new DoublyLinkedNode(value);
        if (tail == nullptr) { head = tail = newNode; return; }
        newNode->prevPointer = tail;
        tail->nextPointer = newNode;
        tail = newNode;
    }

    int Size() {
        int count = 0;
        for (DoublyLinkedNode* t = head; t != nullptr; t = t->nextPointer) count++;
        return count;
    }

    bool InsertAtPosition(int value, int position) {
)PQ";
        p.starter = body + "        // Write your code here\n        return false;\n    }\n};\n";
        p.solution = body + R"PQ(        int size = Size();
        if (position < 0 || position > size) return false;
        if (position == 0) { InsertAtBeginning(value); return true; }
        if (position == size) { InsertAtEnd(value); return true; }
        DoublyLinkedNode* newNode = new DoublyLinkedNode(value);
        DoublyLinkedNode* temp = head;
        for (int i = 0; i < position - 1; i++) temp = temp->nextPointer;
        newNode->nextPointer = temp->nextPointer;
        newNode->prevPointer = temp;
        temp->nextPointer->prevPointer = newNode;
        temp->nextPointer = newNode;
        return true;
    }
};
)PQ";
        p.tests = std::string(kDListHelpers) + "//@@MAIN\n" + R"PQ(PQ_TOTAL(7);
{ DoublyLinkedList l; l.InsertAtEnd(1); l.InsertAtEnd(2); l.InsertAtEnd(4); l.InsertAtPosition(3, 2);
  PQ_STR(1, "{1, 2, 4}, InsertAtPosition(3, 2): forward", pq_fwd(l.head), "{1, 2, 3, 4}"); PQ_STR(2, "... backward from tail", pq_bwd(l.tail), "{4, 3, 2, 1}"); }
{ DoublyLinkedList l; l.InsertAtEnd(1); l.InsertAtEnd(2); l.InsertAtPosition(0, 0); PQ_STR(3, "{1, 2}, InsertAtPosition(0, 0)", pq_fwd(l.head), "{0, 1, 2}"); }
{ DoublyLinkedList l; l.InsertAtEnd(1); l.InsertAtEnd(2); l.InsertAtPosition(3, 2);
  PQ_STR(4, "{1, 2}, InsertAtPosition(3, 2): backward (tail updated?)", pq_bwd(l.tail), "{3, 2, 1}"); }
{ DoublyLinkedList l; l.InsertAtEnd(1); PQ_BOOL(5, "{1}, InsertAtPosition(9, 3) returns", l.InsertAtPosition(9, 3), false); }
{ DoublyLinkedList l; PQ_BOOL(6, "empty, InsertAtPosition(5, 0) returns", l.InsertAtPosition(5, 0), true); PQ_BOOL(7, "... head == tail", l.head != nullptr && l.head == l.tail, true); }
)PQ";
        p.hints = {"Positions 0 and size are InsertAtBeginning / InsertAtEnd.",
                   "Walk temp to the node before the position.",
                   "newNode->nextPointer = temp->nextPointer; newNode->prevPointer = temp; temp->nextPointer->prevPointer = newNode; temp->nextPointer = newNode; (last line last!)"};
        Push(p);
    }
}

} // namespace

const std::vector<Problem>& Problems() {
    if (gProblems.empty()) Build();
    return gProblems;
}

const Problem* ProblemById(const std::string& id) {
    for (const Problem& p : Problems()) if (p.id == id) return &p;
    return nullptr;
}

const char* DifficultyName(int d) { return d == 0 ? "Easy" : d == 1 ? "Medium" : "Hard"; }
Color DifficultyColor(int d) {
    return ui::Tone(d == 0 ? Color{52, 211, 153, 255} : d == 1 ? Color{251, 191, 36, 255} : Color{248, 113, 113, 255});
}
