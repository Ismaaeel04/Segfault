// ============================================================================
// questions.cpp  -  Quiz bank. The FIRST option is always the correct one;
// the quiz shuffles the options before showing them.
// ============================================================================
#include "content.h"

const std::vector<Question>& Questions() {
    static std::vector<Question> q;
    if (!q.empty()) return q;
    auto add = [&](int topic, const std::string& prompt, const std::string& code,
                   std::vector<std::string> options, const std::string& explain) {
        q.push_back({(int)q.size() + 1, topic, prompt, code, options, explain});
    };

    // ------------------------------------------------- Numbers & Loops ----
    add(S_NUMBERS, "What does `1234 % 10` evaluate to?", "",
        {"4", "1", "123", "0.4"},
        "`%` gives the remainder of the division by 10, which is always the **last digit**.");
    add(S_NUMBERS, "Which of these years is NOT a leap year?", "",
        {"1900", "2000", "2016", "1600"},
        "1900 is divisible by 100 but not by 400, so it is not a leap year. 2000 and 1600 are divisible by 400.");
    add(S_NUMBERS, "What is the value of `r` after this loop?",
        "int n = 120, r = 0;\nwhile (n != 0) {\n    r = r * 10 + n % 10;\n    n = n / 10;\n}",
        {"21", "120", "12", "210"},
        "The digits are taken 0, 2, 1: r = 0, then 2, then 21. A leading zero disappears in an int.");
    add(S_NUMBERS, "Why does the prime check only loop while `i * i <= n`?", "",
        {"If n has a divisor bigger than sqrt(n), it also has one smaller than sqrt(n)",
         "Because primes are always smaller than their square root",
         "To avoid a division by zero", "Because i * i is faster to compute than i + 1"},
        "Divisors come in pairs a*b = n. One of them is always <= sqrt(n), so checking up to sqrt(n) is enough.");
    add(S_NUMBERS, "The test calls `isAPrimeNumber(2147483647)`. Why is `i` declared as `long long`?", "",
        {"i * i can overflow an int near the end of the loop", "long long loops run faster",
         "% does not work with int", "Because the number is negative"},
        "2147483647 is INT_MAX. When i reaches ~46341, i * i is larger than INT_MAX and an int would overflow.");
    add(S_NUMBERS, "What does `convertBinarytoDecimal(1011)` return?", "",
        {"11", "13", "1011", "9"},
        "1*1 + 1*2 + 0*4 + 1*8 = 11. Read the digits right to left, doubling the base each time.");
    add(S_NUMBERS, "How many `*` characters does this print in total?",
        "for (int r = 1; r <= 3; r++) {\n    for (int c = 1; c <= r; c++) printf(\"*\");\n    printf(\"\\n\");\n}",
        {"6", "3", "9", "4"},
        "Row 1 prints 1, row 2 prints 2, row 3 prints 3: 1 + 2 + 3 = 6.");
    add(S_NUMBERS, "For `row` going from -3 to 3, which expression gives the counts 1 2 3 4 3 2 1?", "",
        {"4 - abs(row)", "abs(row) + 1", "4 + row", "row * row"},
        "abs(row) is 3 2 1 0 1 2 3, so 4 - abs(row) is 1 2 3 4 3 2 1. No if statement needed.");
    add(S_NUMBERS, "What is `7 / 2` when both are ints?", "",
        {"3", "3.5", "4", "A compile error"},
        "Integer division throws away the fractional part. This is why `n / 10` removes the last digit.");
    add(S_NUMBERS, "What does `isAPalindrome(10)` return?", "bool isAPalindrome(int n) {\n    return Reversed(n) == n;\n}",
        {"false, because Reversed(10) is 1", "true", "false, because 10 is even", "It crashes"},
        "Reversing 10 gives 01, which is just 1 as an int. 1 != 10.");
    add(S_NUMBERS, "Which condition is a correct leap year test?", "",
        {"(y % 4 == 0 && y % 100 != 0) || y % 400 == 0", "y % 4 == 0 || y % 100 == 0",
         "y % 4 == 0 && y % 400 == 0", "y % 100 != 0"},
        "Divisible by 4 but not by 100, unless it is also divisible by 400.");

    // ---------------------------------------------------------- Arrays ----
    add(S_ARRAYS, "What are the valid indexes of `int arr[5];`?", "",
        {"0 to 4", "1 to 5", "0 to 5", "1 to 4"},
        "Arrays start at 0, so the last valid index is size - 1.");
    add(S_ARRAYS, "Why does every Lab1 array function also take `int size`?", "",
        {"An array parameter is really a pointer, so the function cannot know the length",
         "printf needs it", "Arrays in C always have 10 elements", "To allocate the memory"},
        "Arrays decay to a pointer to their first element when passed to a function. The length is lost.");
    add(S_ARRAYS, "In `insertElement`, why does the shifting loop start at the END of the array?", "",
        {"Shifting from the front would overwrite values before they are moved",
         "It is faster", "Arrays can only be traversed backwards", "Because count is a reference"},
        "arr[i] = arr[i-1] going forwards would copy the same value into every slot. Going backwards moves each value before it is overwritten.");
    add(S_ARRAYS, "With the Lab1 rules, what does `find2ndLargest` return for this array?", "int a[] = {3, 9, 9, 1};",
        {"3", "9", "1", "-1"},
        "The largest is 9. The second 9 is not smaller than largest, so it is ignored. The second largest distinct value is 3.");
    add(S_ARRAYS, "How many duplicate elements does `{4, 4, 4, 2}` contain (an element is a duplicate if the same value appeared earlier)?", "",
        {"2", "3", "1", "4"},
        "The 2nd and 3rd 4 are duplicates of an earlier 4. The first 4 and the 2 are not.");
    add(S_ARRAYS, "What does `{1, 2, 3, 4}` look like after `rotateLeft`?", "",
        {"{2, 3, 4, 1}", "{4, 1, 2, 3}", "{2, 3, 4, 0}", "{1, 2, 3, 4}"},
        "The first element is saved, everything shifts one place left, and the saved value goes to the end.");
    add(S_ARRAYS, "In `twoMovies`, why does the inner loop start at `j = i + 1`?", "",
        {"So a movie is never paired with itself and each pair is checked once", "To skip the first movie",
         "To make the algorithm O(n)", "Because arrays start at 1"},
        "j = i would pair a movie with itself. j = 0 would check every pair twice.");
    add(S_ARRAYS, "Why is `count` declared as `int& count` in `insertElement`?", "",
        {"So count++ inside the function updates the caller's variable", "To make it faster",
         "Because arrays need references", "So it can be nullptr"},
        "A reference is an alias for the caller's variable. Without &, the function would change a copy.");
    add(S_ARRAYS, "What does `wordCounter` return for `\"  hi  there \"`?", "",
        {"2", "3", "4", "0"},
        "Only the start of each run of letters counts: 'hi' and 'there'. Extra spaces do not create words.");
    add(S_ARRAYS, "What is the bug in this loop?", "int sum = 0;\nfor (int i = 0; i <= size; i++)\n    sum += arr[i];",
        {"It reads one element past the end (arr[size])", "It skips the first element", "Nothing, it is correct",
         "sum must be a pointer"},
        "With `<=`, the last iteration reads arr[size], which is outside the array. Use `i < size`.");
    add(S_ARRAYS, "How many swaps does an in-place reverse do for an array of 5 elements?", "",
        {"2", "5", "4", "3"},
        "Swap (0,4) and (1,3). The middle element (2) stays where it is: size / 2 = 2 swaps.");

    // ------------------------------------------- 2D Arrays & Parsing ----
    add(S_GRID, "In `int g[4][6];`, what does `g[2][5]` refer to?", "",
        {"Row 2, column 5 (third row, last column)", "Row 5, column 2", "The 25th element", "It is out of bounds"},
        "The first index is the row, the second the column. Both start at 0.");
    add(S_GRID, "`ReplaceWithMinusOne` checks each position in which order?", "",
        {"Horizontal first, then vertical", "Vertical first, then horizontal", "Both at the same time", "Diagonals first"},
        "The brief says: check for a HORIZONTAL line first, and only if none is found, check VERTICAL.");
    add(S_GRID, "A vertical cluster of three 3's is found. What is the score?", "",
        {"9", "3", "6", "27"},
        "The score is the sum of the values in the cluster: 3 + 3 + 3 = 9.");
    add(S_GRID, "Which values can NEVER be part of a cluster?", "",
        {"0, -1 and -2", "Only 0", "1 to 4", "Only -2"},
        "Only the values 1 to 4 form clusters. 0 is empty, -1 is removed, -2 is a new item.");
    add(S_GRID, "A column reads (top to bottom) `2, -1, 3, -1`. What is it after `FallDownAndReplace`?", "",
        {"-2, -2, 2, 3", "2, 3, -2, -2", "-2, 2, -2, 3", "3, 2, -2, -2"},
        "The values 2 and 3 fall to the bottom keeping their order. The two empty slots at the top become -2.");
    add(S_GRID, "In the Lab1 grid, which cluster does `ReplaceWithMinusOne` find first?",
        "{0,0,3,1,3,4},\n{0,0,2,3,4,3},\n{0,0,1,3,3,2},\n{0,0,1,1,1,1}",
        {"The bottom row 1 1 1 1 (score 4)", "The 3 3 in row 2", "Column 3: 1 3 3 1", "There is no cluster"},
        "No horizontal or vertical run of 3 exists until row 3, column 2, where 1 1 1 1 starts. 4 x 1 = 4.");
    add(S_GRID, "When searching for `int` in the code text, why check the character BEFORE it?", "",
        {"So words like 'print' are not mistaken for the type int", "To skip comments",
         "To find the semicolon", "Because int has 3 letters"},
        "'print' contains 'int'. A real keyword must not be preceded by a letter, digit or underscore.");
    add(S_GRID, "How can the parser tell that `int main()` is not a variable declaration?", "",
        {"The name is followed by '('", "main is a reserved keyword", "It is always on line 1", "It has a semicolon"},
        "A function name is followed by its parameter list. Variable names are followed by =, ; or [.");
    add(S_GRID, "Which variables are in scope AFTER line 6?",
        "int main() {        // 1\n    int a = 1;      // 2\n    {               // 3\n        int b = 2;  // 4\n    }               // 5\n    char c;         // 6",
        {"a,c", "a,b,c", "a", "b,c"},
        "b was declared at depth 2 and went out of scope at the } on line 5. a and c are at depth 1.");
    add(S_GRID, "How does `FindAllVariablesInScope` track scope?", "",
        {"A depth counter: +1 on '{', -1 on '}'", "+1 for every new line", "By counting semicolons", "By measuring indentation"},
        "Variables are pushed with their depth and popped when a '}' makes the depth smaller than theirs.");
    add(S_GRID, "After `char s[10] = \"\";`, what is `s[0]`?", "",
        {"'\\0' (the terminator)", "' ' (a space)", "'0' (the digit)", "Garbage"},
        "An empty string is just the terminator. That is also how you reset the variables list: variables[0] = '\\0'.");

    // -------------------------------------------------------- Pointers ----
    add(S_POINTERS, "What is the value of `i` at the end?", "int i = 5;\nint* p = &i;\n*p = 10;",
        {"10", "5", "The address of i", "Compile error"},
        "p holds the address of i, so *p = 10 writes 10 into i.");
    add(S_POINTERS, "What does `&x` give you?", "",
        {"The address of x", "The value of x", "A copy of x", "The size of x"},
        "& is the address-of operator. * is the dereference operator.");
    add(S_POINTERS, "Lecture exercise: what are i and j at the end?",
        "int i = 2, j = 8;\nint* ptr = &i;\nint** pptr = &ptr;\n**pptr = 12;\nptr = &j;\n**pptr = 34;",
        {"i = 12, j = 34", "i = 34, j = 8", "i = 12, j = 8", "i = 2, j = 34"},
        "**pptr is whatever ptr points at. First that is i (12), then ptr moves to j, so j becomes 34.");
    add(S_POINTERS, "`int a[4]` starts at address 1000 and an int is 4 bytes. What address is `a + 1`?", "",
        {"1004", "1001", "1008", "2000"},
        "Pointer arithmetic moves by sizeof(int): 1000 + 1 * 4 = 1004.");
    add(S_POINTERS, "Which pointer operation is INVALID?", "",
        {"ptr * 2", "ptr + 2", "ptr - 1", "ptr2 - ptr1"},
        "You can add/subtract integers and subtract two pointers, but multiplying or dividing a pointer makes no sense.");
    add(S_POINTERS, "`a[i]` is exactly the same as:", "",
        {"*(a + i)", "*a + i", "&a + i", "a + *i"},
        "Indexing is pointer arithmetic plus a dereference. *a + i would add i to the first value.");
    add(S_POINTERS, "What is `i` after the call?", "void f(int j) {\n    j = 5;\n}\n\nint i = 3;\nf(i);",
        {"3", "5", "Undefined", "0"},
        "j is a copy of i (pass by value). To change i, pass &i to a function taking int*.");
    add(S_POINTERS, "Which statement about pointers vs references is TRUE?", "",
        {"A pointer can be nullptr, a reference cannot", "A reference can be re-pointed to another variable",
         "Pass by pointer cannot change the original", "References need the * operator"},
        "From the lecture: pass by pointer can pass a null pointer, pass by reference cannot.");
    add(S_POINTERS, "In `strlen_ptr`, why does `return p - str;` give the length?",
        "char* p = str;\nwhile (*p != '\\0') p++;\nreturn p - str;",
        {"Subtracting two pointers into the same array gives the number of elements between them",
         "p - str is the address of the last character", "It converts the pointer to an int",
         "It counts the bytes of the pointer variable"},
        "p stopped on the terminator. The distance from the start is the number of characters before it.");
    add(S_POINTERS, "What is wrong with this copy function?",
        "void copy(char* d, char* s) {\n    while (*s != '\\0') {\n        *d = *s;\n        d++;\n        s++;\n    }\n}",
        {"The '\\0' terminator is never written to d", "s is never incremented", "d should be a reference",
         "Nothing, it is correct"},
        "The loop stops before copying the terminator. Add *d = '\\0'; after the loop.");
    add(S_POINTERS, "What does `fp(20, 10)` return?",
        "int Add(int a, int b) { return a + b; }\n\nint (*fp)(int, int) = Add;",
        {"30", "The address of Add", "Compile error", "2010"},
        "fp points to Add, so calling fp(20, 10) calls Add(20, 10).");
    add(S_POINTERS, "What does `compare_strings(\"apple\", \"banana\")` return?", "",
        {"A negative value", "0", "A positive value", "Always 1"},
        "'a' (97) - 'b' (98) is negative: \"apple\" comes before \"banana\".");
    add(S_POINTERS, "Which loop obeys the rule 'no array indexing'?", "",
        {"for (int* p = arr; p < arr + n; p++) sum += *p;", "for (int i = 0; i < n; i++) sum += arr[i];",
         "sum = arr[0] + arr[1];", "for (int i = 0; i < n; i++) sum += i[arr];"},
        "Only the walking pointer avoids [ ]. (i[arr] is legal C, but it is still indexing!)");

    // ------------------------------------------------ Memory Management ----
    add(S_MEMORY, "Where does `new int` allocate memory?", "",
        {"On the heap", "On the stack", "In a CPU register", "In the code segment"},
        "Local variables live on the stack; memory from new (or malloc) lives on the heap until you free it.");
    add(S_MEMORY, "What is p after the block ends?", "int* p;\nif (true) {\n    int x = 5;\n    p = &x;\n}\ncout << *p;",
        {"A dangling pointer: x is out of scope", "A null pointer", "Always 5", "A memory leak"},
        "x died at the closing brace. p still holds its old address, whose contents are now undefined.");
    add(S_MEMORY, "How many ints are leaked?",
        "int* p;\nfor (int i = 0; i < 3; ++i) {\n    p = getPtrToFive();  // new int\n}\ndelete p;",
        {"2", "3", "0", "1"},
        "Three ints are allocated but only the last one is deleted. The first two have no pointer left: leaked.");
    add(S_MEMORY, "Memory allocated with `new int[n]` must be freed with:", "",
        {"delete[]", "delete", "free", "Nothing, it is automatic"},
        "new pairs with delete, new[] pairs with delete[], malloc pairs with free.");
    add(S_MEMORY, "What is wrong here?", "int x = 5;\nint* xPtr = &x;\ndelete xPtr;",
        {"It deletes memory that was not allocated with new", "Nothing", "It leaks memory", "It should be delete[]"},
        "Only delete memory that came from new. x is on the stack.");
    add(S_MEMORY, "`malloc` returned NULL. What does that mean?", "",
        {"The allocation failed", "The memory was set to zero", "It returned stack memory", "It succeeded"},
        "Always check: if (ptr == NULL) { printf(\"Memory allocation failed!\\n\"); return 1; }");
    add(S_MEMORY, "What is the problem?", "int* x = new int(5);\ndelete x;\ndelete x;",
        {"Double delete: undefined behaviour", "Nothing", "A memory leak", "A compile error"},
        "Deleting the same memory twice is undefined behaviour. Setting x = nullptr after the first delete makes the second harmless.");
    add(S_MEMORY, "How do you free `int** arr2D` allocated as an array of rows?", "",
        {"delete[] each row, then delete[] arr2D", "delete[] arr2D, then each row", "delete arr2D once",
         "free(arr2D) only"},
        "Free the inner arrays first. If you free arr2D first, you lose the row pointers and leak every row.");
    add(S_MEMORY, "What is a good habit right after `delete p;`?", "",
        {"p = nullptr;", "delete p; again", "*p = 0;", "return p;"},
        "A null pointer is easy to check and deleting nullptr is safe. *p = 0 would be use-after-delete.");
    add(S_MEMORY, "What is the global glVar after `f()` runs?",
        "int glVar = 10;\nvoid f() {\n    int glVar;\n    glVar = 20;\n}",
        {"10", "20", "Undefined", "0"},
        "The local glVar shadows the global. Only the local copy becomes 20.");
    add(S_MEMORY, "Why is `int arr[numItems];` (numItems read with cin) not allowed in standard C++?", "",
        {"Stack array sizes must be compile-time constants: use new int[numItems]", "numItems could be negative",
         "Arrays cannot hold ints", "cin cannot be used with arrays"},
        "Use dynamic allocation for a size known only at runtime, and delete[] it when done.");

    // ----------------------------------------------------- Linked Lists ----
    add(S_LISTS, "In a singly linked list, what is the `next` of the last node?", "",
        {"nullptr", "head", "The node itself", "The previous node"},
        "nullptr marks the end; every traversal loop stops on it.");
    add(S_LISTS, "Which is the correct order for InsertAtBeginning?", "",
        {"newNode->next = head; then head = newNode;", "head = newNode; then newNode->next = head;",
         "head->next = newNode;", "delete head; head = newNode;"},
        "If head moves first, newNode->next = head makes the node point at itself and the rest of the list is lost.");
    add(S_LISTS, "For `InsertAtPosition(value, 3)` in a singly list, where should temp stop?", "",
        {"At index 2, the node before position 3", "At index 3", "At index 4", "At the head"},
        "You need the node BEFORE the gap so you can change its next pointer.");
    add(S_LISTS, "What is the time complexity of InsertAtEnd in a singly list with only a head pointer?", "",
        {"O(n)", "O(1)", "O(log n)", "O(n^2)"},
        "You must walk every node to find the last one.");
    add(S_LISTS, "In a doubly list that keeps a tail pointer, InsertAtEnd is:", "",
        {"O(1)", "O(n)", "O(n log n)", "O(2n)"},
        "tail->nextPointer = newNode; newNode->prevPointer = tail; tail = newNode; No walking.");
    add(S_LISTS, "What must the LinkedList destructor do?", "",
        {"Walk the list and delete every node", "delete head only", "Only set head = nullptr",
         "Nothing, nodes are freed automatically"},
        "Every node came from new. Deleting only head would leak the rest of the list.");
    add(S_LISTS, "Why save `temp->next` before `delete temp;` when clearing a list?", "",
        {"After delete, reading temp->next is use-after-free", "It makes it faster", "To print it",
         "Because next is private"},
        "Once temp is deleted, its memory is gone, including the next field.");
    add(S_LISTS, "In a doubly list middle insert, which assignment must come AFTER the others that read `temp->nextPointer`?", "",
        {"temp->nextPointer = newNode;", "newNode->prevPointer = temp;", "newNode->nextPointer = temp->nextPointer;",
         "temp->nextPointer->prevPointer = newNode;"},
        "Changing temp->nextPointer first would make the other lines refer to newNode instead of the old next node.");
    add(S_LISTS, "Inserting into an EMPTY doubly linked list:", "",
        {"head and tail both point to the new node", "Only head points to it", "Only tail points to it",
         "Neither changes"},
        "With one node, it is both the first and the last.");
    add(S_LISTS, "The list is 5 -> 10 -> 20. What is it after `InsertAtPosition(15, 2)`?", "",
        {"5 -> 10 -> 15 -> 20", "5 -> 15 -> 10 -> 20", "15 -> 5 -> 10 -> 20", "5 -> 10 -> 20 -> 15"},
        "Position 2 (0-based) is the third slot: the new node goes between 10 and 20.");
    add(S_LISTS, "Why does LinkedList delete its copy constructor?", "",
        {"A default copy would share the nodes and both lists would delete them", "To make it faster",
         "Copying objects is illegal in C++", "Because Node has no constructor"},
        "The default copy copies the head pointer only (shallow copy). Two destructors would double-delete the nodes.");

    // finer sections for some questions
    for (Question& x : q) {
        if (x.id == 7 || x.id == 8) x.section = S_LOOPS;
        if (x.id >= 29 && x.id <= 33) x.section = S_STRINGS;
        if (x.id == 37 || x.id == 38 || x.id == 39 || x.id == 42 || x.id == 43 || x.id == 45 || x.id == 46) x.section = S_PTRARITH;
    }
    return q;
}

const Question* QuestionById(int id) {
    for (const Question& q : Questions()) if (q.id == id) return &q;
    return nullptr;
}
