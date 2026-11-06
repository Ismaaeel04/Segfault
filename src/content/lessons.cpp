// ============================================================================
// lessons.cpp  -  Study content, organised by section.
// Built from Lab1, the Coding Exam 2 brief and the lecture notes
// "Pointers in C,C++" and "Memory Management".
//
// Block types (see content.h): Txt, Code, Walk (step through the code one
// line at a time), Table, Tip, Warn, Demo (open an interactive demo),
// Practice (links to coding problems) and Check (a quick question).
// ============================================================================
#include "content.h"

namespace {

Block Txt(const std::string& s) { Block b; b.type = BlockType::Text; b.text = s; return b; }
Block Code(const std::string& s) { Block b; b.type = BlockType::Code; b.text = s; return b; }
Block Table(const std::string& s) { Block b; b.type = BlockType::Table; b.text = s; return b; }
Block Tip(const std::string& s) { Block b; b.type = BlockType::Tip; b.text = s; return b; }
Block Warn(const std::string& s) { Block b; b.type = BlockType::Warn; b.text = s; return b; }
Block Walk(const std::string& code, std::vector<WalkStep> steps) {
    Block b; b.type = BlockType::Walk; b.text = code; b.steps = std::move(steps); return b;
}
Block Demo(const std::string& label, SceneId scene, const std::string& param) {
    Block b; b.type = BlockType::Demo; b.text = label; b.scene = scene; b.param = param; return b;
}
Block Practice(const std::string& ids) { Block b; b.type = BlockType::Practice; b.text = ids; return b; }
Block Check(const std::string& q, std::vector<std::string> options, const std::string& explain) {
    Block b; b.type = BlockType::Check; b.text = q; b.options = std::move(options); b.explain = explain; return b;
}

std::vector<Lesson> gLessons;

void Add(const std::string& id, int section, const std::string& title, const std::string& summary, std::vector<Block> blocks) {
    gLessons.push_back({id, section, title, summary, std::move(blocks)});
}

void Build() {
    // ================================================== NUMBERS & DIGITS ===
    Add("numbers_mod", S_NUMBERS, "The % and / toolkit", "How to take a number apart digit by digit", {
        Txt("Many exam questions about numbers are really the same trick: take the number apart **one digit at a time**. "
            "You only need two integer operators:\n"
            "- `n % 10` is the **remainder** of dividing by 10, which is always the **last digit**.\n"
            "- `n / 10` is **integer division**: the fractional part is thrown away, so it **removes** the last digit.\n\n"
            "And one more use of `%`: `n % k == 0` means **n is divisible by k**."),
        Table("n|n % 10 (last digit)|n / 10 (the rest)\n1234|4|123\n123|3|12\n12|2|1\n1|1|0"),
        Txt("Repeat until the number becomes 0 and you have visited every digit, from right to left. Step through it:"),
        Walk("int n = 1234;\nint count = 0;\nwhile (n != 0) {\n    int digit = n % 10;\n    count++;\n    n = n / 10;\n}\n// count is 4", {
            {0, "We start with **n = 1234**."},
            {1, "`count` will count the digits. It starts at 0."},
            {2, "The loop keeps going **while n is not 0**. When every digit has been removed, n becomes 0 and we stop."},
            {3, "`n % 10` gives the last digit. First time: **1234 % 10 = 4**."},
            {4, "We have seen one more digit, so count goes up."},
            {5, "`n / 10` removes the last digit: **1234 / 10 = 123**. Integer division, no decimals."},
            {2, "Back to the condition. n is 123, not 0, so we go round again: digit 3, then 2, then 1."},
            {7, "After 4 rounds n is 0 and count is **4**. This loop shape is the base of Reverse, Palindrome, Sum of Digits and Binary to Decimal."}}),
        Tip("`7 / 2` is **3** when both are ints. If you need decimals you must use `double`, but for digit problems integer division is exactly what you want."),
        Check("What is `5678 % 10 + 5678 / 1000`?", {"13", "8", "5678", "567.8"}, "5678 % 10 = 8 and 5678 / 1000 = 5 (integer division). 8 + 5 = 13."),
        Practice("sumOfDigits,gcd"),
    });

    Add("numbers_leap_prime", S_NUMBERS, "Leap years and prime numbers", "Order your checks; stop at the square root", {
        Txt("**Leap years.** The rule: divisible by 4, but not by 100, unless also divisible by 400. "
            "The clean way to code a rule with exceptions is to test the **most specific case first** and return immediately."),
        Walk("bool isLeapYear(int year) {\n    if (year % 400 == 0) return true;\n    if (year % 100 == 0) return false;\n    return year % 4 == 0;\n}", {
            {1, "The rarest case first: divisible by 400 is **always** a leap year (1600, 2000). We return straight away."},
            {2, "If we get here, the year is NOT divisible by 400. So if it is divisible by 100 (1700, 1900) it is **not** a leap year."},
            {3, "Only normal years are left: they are leap years when divisible by 4 (2016 yes, 2019 no). The comparison itself is the bool we return."}}),
        Table("year|% 400|% 100|% 4|leap?\n1600|0|-|-|yes\n1700|100|0|-|no\n2016|16|16|0|yes\n2019|19|19|3|no"),
        Txt("**Prime numbers.** A prime is only divisible by 1 and itself. If n = a * b, then one of a or b is at most the square root of n. "
            "So you only need to try divisors **up to sqrt(n)**. Lab1 tests 2147483647: trying every number up to n would take seconds, "
            "trying up to sqrt(n) is about 23,000 checks."),
        Walk("bool isAPrimeNumber(int n) {\n    if (n < 2) return false;\n    if (n % 2 == 0) return n == 2;\n    for (long long i = 3; i * i <= n; i += 2) {\n        if (n % i == 0) return false;\n    }\n    return true;\n}", {
            {1, "0, 1 and negative numbers are not prime."},
            {2, "Even numbers: only 2 is prime. `return n == 2;` handles both cases in one line."},
            {3, "Try odd divisors 3, 5, 7... while **i * i <= n** (that means i <= sqrt(n), without using sqrt). `long long` stops `i * i` overflowing near 2147483647."},
            {4, "Found a divisor: not prime. Stop immediately."},
            {6, "No divisor up to the square root, so it is prime."}}),
        Warn("`for (int i = 2; i < n; i++)` is correct but too slow for 2147483647. In Practice you will get **Time Limit Exceeded** - try it and see."),
        Practice("isLeapYear,isAPrimeNumber"),
    });

    Add("numbers_reverse_binary", S_NUMBERS, "Reversing digits, palindromes and binary", "Building a number up instead of taking it apart", {
        Txt("To **reverse** a number we take digits off the right of `n` and **append** them to the right of the result. "
            "Appending a digit to a number means: multiply by 10, then add the digit."),
        Walk("int Reversed(int testNumber) {\n    int reversed = 0;\n    while (testNumber != 0) {\n        int digit = testNumber % 10;\n        reversed = reversed * 10 + digit;\n        testNumber = testNumber / 10;\n    }\n    return reversed;\n}", {
            {1, "The result starts empty: 0."},
            {3, "Take the last digit of 123: **3**."},
            {4, "Append it: 0 * 10 + 3 = **3**. Next round: 3 * 10 + 2 = 32. Then 32 * 10 + 1 = **321**."},
            {5, "Drop the digit we used: 123 -> 12 -> 1 -> 0."},
            {7, "When testNumber reaches 0 we are done. 1200 gives 21 because 0021 is just 21 as an int."}}),
        Table("round|testNumber|digit|reversed\n1|123|3|3\n2|12|2|32\n3|1|1|321"),
        Txt("A **palindrome** reads the same both ways, so it is one line on top of Reversed: `return Reversed(n) == n;`. "
            "Reusing a helper is exactly what Lab1 expects."),
        Txt("**Binary to decimal** works the same way, but each digit is worth a **power of 2**: 1, 2, 4, 8... "
            "The exam says *without the shift operator*, so keep the power in a variable that doubles."),
        Walk("int convertBinarytoDecimal(int binaryNumber) {\n    int decimal = 0;\n    int base = 1;\n    while (binaryNumber > 0) {\n        int bit = binaryNumber % 10;\n        decimal = decimal + bit * base;\n        base = base * 2;\n        binaryNumber = binaryNumber / 10;\n    }\n    return decimal;\n}", {
            {2, "The right-most bit is worth 2^0 = **1**."},
            {4, "For 110 the last digit is **0**."},
            {5, "Add the bit times its value: 0 * 1 = 0. Next round: 1 * 2 = 2. Then 1 * 4 = 4."},
            {6, "The next bit to the left is worth double: 1, 2, 4, 8..."},
            {7, "Remove the bit we used, like every digit loop."},
            {9, "0 + 2 + 4 = **6**."}}),
        Practice("Reversed,isAPalindrome,convertBinarytoDecimal"),
    });

    // =================================================== LOOPS & PATTERNS ===
    Add("loops_nested", S_LOOPS, "Nested loops: rows and columns", "The outer loop picks the row, the inner loop fills it", {
        Txt("For any pattern question, first write down **how many characters each row prints**. Then:\n"
            "- the **outer loop** goes over the rows,\n- the **inner loop** prints that many characters,\n- after the inner loop, print a **newline**."),
        Table("row|A's printed\n1|1\n2|2\n3|3\n4|4"),
        Walk("void drawRightAngledTriangle() {\n    for (int row = 1; row <= 4; row++) {\n        for (int col = 1; col <= row; col++) {\n            printf(\"A\");\n        }\n        printf(\"\\n\");\n    }\n}", {
            {1, "Rows 1 to 4."},
            {2, "The inner loop runs **row** times: once on row 1, twice on row 2... That gives the triangle shape."},
            {3, "Print one A. No newline here, so the A's stay on the same line."},
            {5, "Only after the inner loop: end the row with a newline."}}),
        Tip("If the count depends on a parameter `n`, replace the 4 with `n`. If you print the column number instead of 'A', you get 1, 12, 123, 1234."),
        Check("How many times does `printf(\"A\")` run in total for 4 rows?", {"10", "4", "16", "8"}, "1 + 2 + 3 + 4 = 10."),
        Practice("drawRightAngledTriangle,printNumberTriangle"),
    });

    Add("loops_diamond", S_LOOPS, "Shapes that grow and shrink", "One if/else, or no if at all", {
        Txt("The diamond prints **1 2 3 4 3 2 1** A's: 7 rows. Write the table first and look for the rule."),
        Table("row|count|rule\n1|1|row\n2|2|row\n3|3|row\n4|4|row\n5|3|8 - row\n6|2|8 - row\n7|1|8 - row"),
        Walk("for (int row = 1; row <= 7; row++) {\n    int count;\n    if (row <= 4) count = row;\n    else count = 8 - row;\n    for (int col = 1; col <= count; col++) printf(\"A\");\n    printf(\"\\n\");\n}", {
            {0, "Seven rows."},
            {2, "Growing half: the count is the row number. This is the single if/else Lab1 allows."},
            {3, "Shrinking half: 8 - 5 = 3, 8 - 6 = 2, 8 - 7 = 1."},
            {4, "The inner loop prints count A's - the same as the triangle."}}),
        Txt("**No conditionals at all.** Make the row go from **-3 to 3** instead. The distance from the middle is `abs(row)`: 3 2 1 0 1 2 3. "
            "So the count is `4 - abs(row)`: 1 2 3 4 3 2 1. Symmetry around 0 is the trick for every 'up then down' pattern."),
        Code("for (int row = -3; row <= 3; row++) {\n    int count = 4 - abs(row);     // abs() is in <stdlib.h>\n    for (int col = 1; col <= count; col++) printf(\"A\");\n    printf(\"\\n\");\n}"),
        Tip("Hollow shapes: print a star when you are on the border (`row == 0 || row == n - 1 || col == 0 || col == n - 1`), otherwise a space."),
        Practice("drawIsocelesTriangle,drawIsocelesTriangle2,printHollowSquare"),
    });

    // ============================================================= ARRAYS ===
    Add("arrays_basics", S_ARRAYS, "Arrays: indexes, size and searching", "Why every function takes a size", {
        Txt("An array is a row of values of the same type, next to each other in memory. `int arr[5]` has indexes **0 to 4**. "
            "When you pass an array to a function, C passes only the **address of the first element** - the function cannot know the length. "
            "That is why every Lab1 function also takes `int size`."),
        Code("int find(int size, int arr[], int toFind) {\n    for (int i = 0; i < size; i++) {\n        if (arr[i] == toFind) return i;\n    }\n    return -1;\n}"),
        Warn("`i <= size` reads `arr[size]`, one element past the end. C does not stop you: you get garbage or a crash. Always `i < size`."),
        Txt("**Second largest in one pass.** Keep two variables: `largest` and `second`. Each new value either beats the largest, "
            "beats only the second, or does nothing."),
        Walk("int largest = -1, second = -1;\nfor (int i = 0; i < size; i++) {\n    if (arr[i] > largest) {\n        second = largest;\n        largest = arr[i];\n    } else if (arr[i] < largest && arr[i] > second) {\n        second = arr[i];\n    }\n}\nreturn second;", {
            {0, "The values are positive, so -1 means 'nothing yet'."},
            {2, "A new champion..."},
            {3, "...so the old champion becomes the runner-up. This line must come **before** the next one."},
            {4, "Now update the champion."},
            {5, "Not a new champion. It can still be the new runner-up, but only if it is **smaller** than largest: that stops a second 9 from counting."},
            {9, "If all values were equal, second is still -1: there is no second largest."}}),
        Table("value|largest|second\n3|3|-1\n9|9|3\n2|9|3\n8|9|8\n5|9|8"),
        Practice("find,find2ndLargest,copyArraytoArray,frequencyCount"),
    });

    Add("arrays_shift", S_ARRAYS, "Insert and delete by shifting", "Make room or close the gap - in the right direction", {
        Txt("An array cannot grow in the middle. To **insert** at index k, every element from k onwards moves one place **right** to make a hole. "
            "In Lab1 `size` is the capacity and `count` is how many slots are used."),
        Table("step|arr (count = 4, capacity 6)\nstart|10 20 30 40 _ _\narr[4] = arr[3]|10 20 30 40 40 _\narr[3] = arr[2]|10 20 30 30 40 _\narr[2] = 25|10 20 25 30 40 _"),
        Walk("bool insertElement(int& size, int& count, int arr[], int elementToInsert, int insertIndex) {\n    if (count >= size || insertIndex < 0 || insertIndex > count) return false;\n    for (int i = count; i > insertIndex; i--) {\n        arr[i] = arr[i - 1];\n    }\n    arr[insertIndex] = elementToInsert;\n    count++;\n    return true;\n}", {
            {0, "`int&` means **reference**: count is the caller's own variable, not a copy. When we do count++ the caller sees it."},
            {1, "Refuse bad requests first: the array is full, or the index is outside 0..count (count itself is allowed: inserting at the end)."},
            {2, "Shift **from the end backwards**. Going forwards would copy the same value into every slot."},
            {3, "Each element moves one place right."},
            {5, "The hole is ready: store the new value."},
            {6, "One more slot is in use."}}),
        Txt("**Delete** is the mirror image: shift **left**, starting **at the hole**, then `count--`."),
        Code("for (int i = deleteIndex; i < count - 1; i++) {\n    arr[i] = arr[i + 1];\n}\ncount--;"),
        Check("Why does insert shift from the end and delete from the hole?", {
            "So each value is moved before it is overwritten", "It is faster", "Arrays can only be read backwards", "Because count is a reference"},
            "Always copy a value out of a slot before that slot is overwritten. That decides the direction."),
        Practice("insertElement,deleteElement"),
    });

    Add("arrays_more", S_ARRAYS, "Reverse, rotate, pairs and words", "Two indexes, a saved value, nested loops and a state flag", {
        Txt("**Reverse in place**: one index at each end. Swap them and move both towards the middle until they meet. "
            "For 5 elements that is only 2 swaps; the middle stays."),
        Walk("for (int left = 0, right = size - 1; left < right; left++, right--) {\n    int temp = arr[left];\n    arr[left] = arr[right];\n    arr[right] = temp;\n}", {
            {0, "Two indexes in one for loop. Stop when they meet in the middle."},
            {1, "The classic swap needs a temporary variable..."},
            {2, "...otherwise this line would destroy arr[left] before we copied it."},
            {3, "Complete the swap."}}),
        Txt("**Rotate left by one**: save `arr[0]`, shift everything one place left, put the saved value at the end."),
        Txt("**Every pair once** (two movies): the inner loop starts at `j = i + 1`. Starting at 0 would check each pair twice and pair a movie with itself."),
        Code("for (int i = 0; i < size; i++)\n    for (int j = i + 1; j < size; j++)\n        if (movieLengths[i] + movieLengths[j] == flightLength) return true;\nreturn false;"),
        Txt("**Counting words** uses a *state flag*: remember whether you are inside a word. A word starts when you see a letter and you were not in a word."),
        Table("char|letter?|inWord before|count\nH|yes|false|1\ni|yes|true|1\n(space)|no|true|1\n(space)|no|false|1\nC|yes|false|2"),
        Practice("reverse,rotateLeft,countDuplicates,twoMovies,wordCounter"),
    });

    // ========================================================== 2D ARRAYS ===
    Add("grid_basics", S_GRID, "2D arrays: rows first", "array[row][col] and the nested loop", {
        Txt("`int array[4][6]` has **4 rows** and **6 columns**. `array[row][col]`: the first index is the row (up/down), the second is the column (left/right). "
            "To visit every cell left-to-right, top-to-bottom: outer loop over rows, inner loop over columns."),
        Code("void PrintArray(int array[4][6]) {\n    for (int row = 0; row < 4; row++) {\n        for (int col = 0; col < 6; col++) {\n            printf(\"%3d \", array[row][col]);\n        }\n        printf(\"\\n\");\n    }\n}"),
        Tip("`%3d` prints every number 3 characters wide, so -1 and 4 line up in columns."),
        Txt("When you pass a 2D array to a function, the **number of columns must be written** (`int array[4][6]` or `int array[][6]`): "
            "the compiler needs it to find where each row starts."),
        Demo("Open Grid Crusher (Lab1's grid, interactive)", SceneId::Grid, ""),
        Practice("PrintArray"),
    });

    Add("grid_cluster", S_GRID, "Finding the first cluster", "Horizontal first, then vertical - and return straight away", {
        Txt("ReplaceWithMinusOne scans every cell in order. At each cell it counts how many identical values continue **to the right**. "
            "If there are fewer than 3, it counts **downwards**. The first run of 3 or more is the cluster."),
        Walk("for (int row = 0; row < 4; row++) {\n    for (int col = 0; col < 6; col++) {\n        int value = array[row][col];\n        if (value < 1 || value > 4) continue;\n        int length = 1;\n        while (col + length < 6 && array[row][col + length] == value) length++;\n        if (length >= 3) {\n            for (int k = 0; k < length; k++) array[row][col + k] = -1;\n            return value * length;\n        }\n        length = 1;\n        while (row + length < 4 && array[row + length][col] == value) length++;\n        if (length >= 3) {\n            for (int k = 0; k < length; k++) array[row + k][col] = -1;\n            return value * length;\n        }\n    }\n}\nreturn 0;", {
            {3, "0, -1 and -2 are never part of a cluster. `continue` jumps to the next cell."},
            {4, "The cell itself counts as 1."},
            {5, "Count identical values to the right. `col + length < 6` stops us leaving the row."},
            {6, "3 or more in a row: this is the cluster."},
            {7, "Replace the whole run with -1."},
            {8, "Score = sum of the values = value * length. **Return now**: only the first cluster counts."},
            {11, "No horizontal run: count downwards the same way."},
            {18, "Scanned the whole grid without a cluster: score 0."}}),
        Txt("In Lab1's grid nothing forms a run of 3 until row 3, column 2: **1 1 1 1**. Score 4."),
        Demo("Watch the scan in Grid Crusher", SceneId::Grid, ""),
        Practice("ReplaceWithMinusOne"),
    });

    Add("grid_gravity", S_GRID, "Gravity with a write pointer", "Compact each column from the bottom up", {
        Txt("After a cluster becomes -1, the values above it must **fall**. Work one column at a time, from the bottom row up, "
            "with a second index `writeRow` that marks the next free slot. This 'read index / write index' pattern removes items from any array in place."),
        Walk("for (int col = 0; col < 6; col++) {\n    int writeRow = 3;\n    for (int row = 3; row >= 0; row--) {\n        if (array[row][col] != -1) {\n            array[writeRow][col] = array[row][col];\n            writeRow--;\n        }\n    }\n    while (writeRow >= 0) {\n        array[writeRow][col] = -2;\n        writeRow--;\n    }\n}", {
            {1, "The lowest free slot is the bottom row."},
            {2, "Read every cell from the bottom up."},
            {3, "Skip the holes (-1)."},
            {4, "A real value falls to the lowest free slot (sometimes onto itself, which is fine)."},
            {5, "The next free slot is one row higher."},
            {8, "Whatever is left at the top is empty: fill it with -2 (new items)."}}),
        Table("column (top to bottom)|after\n2, -1, 3, -1|-2, -2, 2, 3\n1, 3, 3, -1|-2, 1, 3, 3"),
        Practice("FallDownAndReplace"),
    });

    // ================================================== STRINGS & PARSING ===
    Add("strings_basics", S_STRINGS, "C strings: char arrays and '\\0'", "A string is just chars until the terminator", {
        Txt("C has no string type. A string is a **char array** that ends with the **null terminator** `'\\0'`. "
            "`char s[] = \"Hi\";` is 3 chars: 'H', 'i', '\\0'. Every string loop runs **until it finds the terminator**."),
        Table("index|0|1|2\ns[index]|'H'|'i'|'\\0'"),
        Code("void toUpperCase(char s[]) {\n    for (int i = 0; s[i] != '\\0'; i++) {\n        if (s[i] >= 'a' && s[i] <= 'z') s[i] = s[i] - 'a' + 'A';\n    }\n}"),
        Txt("Characters are small numbers, so you can compare and subtract them. `s[i] - 'a'` is the position in the alphabet (0..25); adding 'A' gives the uppercase letter."),
        Warn("An empty string is not 'nothing': it is one char, `'\\0'`. To clear a string write `s[0] = '\\0';`."),
        Practice("toUpperCase,isStringPalindrome"),
    });

    Add("strings_parsing", S_STRINGS, "Reading C code as text", "Find declarations; track scope with a stack", {
        Txt("In Q21/Q22 the code is one long string with `\\n` between lines. To find `int x` / `char c` declarations:\n"
            "- find the word `int` or `char` - but not inside a longer word like **print**,\n"
            "- skip spaces and `*`,\n- read the name (letters, digits, `_`),\n"
            "- if the next non-space char is `(` it is a **function** (`int main()`), not a variable,\n"
            "- ignore text inside `\"...\"` so `printf(\"int x\")` does not count."),
        Code("if (variables[0] != '\\0') strcat(variables, \",\");   // comma between names\nstrcat(variables, name);"),
        Txt("**Scope.** Count braces: `{` adds 1 to the depth, `}` subtracts 1. Store each variable with the depth where it was declared, like a **stack**. "
            "When a `}` makes the depth smaller, pop every variable deeper than the new depth: they went out of scope."),
        Table("line|code|depth|stack after the line\n1|int main() {|1|-\n2|int count = 0;|1|count\n4|if (...) {|2|count\n5|int temp = 5;|2|count, temp\n7|}|1|count"),
        Tip("This is how a real compiler tracks scope - and it is the same idea as the stack in the Memory section: locals die when their block ends."),
        Practice("FindAllVariables,FindAllVariablesInScope"),
    });

    // ============================================================ POINTERS ===
    Add("ptr_basics", S_POINTERS, "What a pointer is", "An address, and the two operators & and *", {
        Txt("Memory is a long row of numbered slots. Each variable lives at an **address**. `int i = 5;` stores 5 at, say, address 1000. "
            "A **pointer** is a variable whose value is an address: it does not hold 5, it holds **where** 5 is."),
        Table("address|1000|1004|1008\nvariable|i|j|ptr\nvalue|5|10|1000"),
        Txt("Two operators:\n- `&x` - **address-of**: the address of x.\n- `*p` - **dereference**: the value stored at the address in p. On the left of `=` it writes there.\n\n"
            "In a declaration the star means *is a pointer* (`int* p;`). In an expression it means *go to that address* (`*p = 3;`)."),
        Walk("int i = 5, j = 10;\nint* ptr;\nptr = &i;\n*ptr = 3;\nptr = &j;\n*ptr = -2;", {
            {0, "Two ints, i at 1000 and j at 1004."},
            {1, "A pointer to int. It is not initialised yet: it holds garbage, so never use *ptr now."},
            {2, "ptr stores the **address** of i: 1000."},
            {3, "`*ptr` is 'the int at address 1000', which is i. So **i becomes 3**."},
            {4, "Now ptr holds 1004: it points to j. i keeps its value 3."},
            {5, "The same expression now writes into **j**: j = -2."}}),
        Demo("See it in the Memory Visualizer", SceneId::Visualizer, "ptr_basics"),
        Tip("Set pointers that point at nothing to `nullptr` and check before using them: `if (ptr != nullptr) *ptr = 5;`"),
        Check("After `int a = 7; int* p = &a; *p = *p + 1;` what is a?", {"8", "7", "The address of a", "Compile error"}, "*p is a, so *p = *p + 1 adds one to a."),
    });

    Add("ptr_functions", S_POINTERS, "Pass by value, pointer and reference", "How a function can change your variables", {
        Txt("C **copies** arguments. Inside `void f(int j)` you change a copy, and the caller's variable is untouched. "
            "To let a function change a variable, give it the **address**."),
        Walk("void swap(int* x, int* y) {\n    int z = *x;\n    *x = *y;\n    *y = z;\n}\n\nint a = 45, b = 35;\nswap(&a, &b);", {
            {7, "We pass the addresses of a and b."},
            {0, "x points to a and y points to b."},
            {1, "Save the value at x (that is a: 45)."},
            {2, "Write b's value into a. a is now 35."},
            {3, "Write the saved 45 into b. They really swapped."}}),
        Demo("Compare swap by value and by pointer", SceneId::Visualizer, "swap"),
        Txt("C++ also has **references**: `void swap(int& x, int& y)` and call `swap(a, b)`. Neater syntax, same effect. "
            "The difference: a pointer can be `nullptr`, a reference cannot (and cannot be re-pointed)."),
        Txt("**Returning more than one value.** A function returns one thing, but it can write extra results through pointers:"),
        Code("void minMax(int arr[], int size, int *min, int *max) {\n    *min = arr[0];\n    *max = arr[0];\n    for (int i = 1; i < size; i++) {\n        if (arr[i] < *min) *min = arr[i];\n        if (arr[i] > *max) *max = arr[i];\n    }\n}"),
        Practice("swap_ints,swap_chars,minMax"),
    });

    Add("ptr_pptr", S_POINTERS, "Pointers to pointers and function pointers", "int**, and calling a function through a pointer", {
        Txt("`int** pptr` stores the address of an `int*`. Read the stars one at a time:\n"
            "- `pptr` - the address of ptr\n- `*pptr` - the value of ptr (an address of an int)\n- `**pptr` - the int itself"),
        Table("expression|type|meaning\npptr|int**|address of ptr\n*pptr|int*|ptr itself\n**pptr|int|what ptr points to"),
        Walk("int i = 2, j = 8;\nint* ptr = &i;\nint** pptr = &ptr;\n**pptr = 12;\nptr = &j;\n**pptr = 34;\n*pptr = &i;", {
            {2, "pptr points at ptr, not at i."},
            {3, "Follow both arrows: pptr -> ptr -> i. **i = 12**."},
            {4, "ptr moves to j. pptr still points at ptr."},
            {5, "Now the chain ends at j: **j = 34**."},
            {6, "`*pptr` IS ptr, so this makes ptr point at i again."}}),
        Demo("Do the lecture exercise (with predictions)", SceneId::Visualizer, "pptr_exercise"),
        Txt("**Function pointers.** Functions have addresses too. The pointer type must match the return and parameter types. You can assign and call them, but not do arithmetic on them."),
        Code("int Add(int a, int b) { return a + b; }\n\nint (*functionPtr)(int, int);\nfunctionPtr = Add;\nint result = functionPtr(20, 10);   // 30"),
    });

    // ================================================== POINTER ARITHMETIC ===
    Add("ptrarith_basics", S_PTRARITH, "ptr + 1 and arrays as pointers", "Pointer maths moves by whole elements", {
        Txt("Adding 1 to a pointer moves it to the **next element**, not the next byte: the address grows by `sizeof(type)`."),
        Table("pointer type|sizeof|ptr|ptr + 1\nchar*|1|1000|1001\nint*|4|1000|1004\ndouble*|8|1000|1008"),
        Txt("An array name is the address of its first element, so **`a[i]` is just `*(a + i)`**. "
            "Valid pointer maths: `p + n`, `p - n`, `p++`, `p2 - p1` (distance in elements) and comparing pointers. Not valid: `p * 2`, `p / 2`."),
        Demo("Step through the lecture's float array", SceneId::Visualizer, "ptr_arith"),
        Txt("Coding Exam 2 says: **no array indexing**. Walk a pointer instead of an index:"),
        Walk("int sum_array(int *arr, int size) {\n    int sum = 0;\n    for (int *p = arr; p < arr + size; p++) {\n        sum += *p;\n    }\n    return sum;\n}", {
            {2, "p starts at the first element. `arr + size` is the address just **past** the last element: we stop there."},
            {3, "*p is the current element."},
            {2, "p++ moves p to the next int (4 bytes further)."},
            {5, "No [ ] anywhere: the exam rule is respected. The Practice tab checks this rule too."}}),
        Practice("sum_array,find_max,reverse_array"),
    });

    Add("ptrarith_strings", S_PTRARITH, "String functions with pointers", "strlen, copy, concat and compare without indexes", {
        Txt("With strings you walk a `char*` until it reaches `'\\0'`. The length is then just the **distance** between two pointers."),
        Walk("int strlen_ptr(char *str) {\n    char *p = str;\n    while (*p != '\\0') {\n        p++;\n    }\n    return p - str;\n}", {
            {1, "p starts at the first char."},
            {2, "Keep going while the current char is not the terminator."},
            {3, "Next char."},
            {5, "p is on the '\\0'. `p - str` counts the chars before it: for \"Hello\" that is 5."}}),
        Code("void copy_string(char *dest, char *src) {\n    while (*src != '\\0') {\n        *dest = *src;\n        dest++;\n        src++;\n    }\n    *dest = '\\0';      // the loop stops BEFORE copying it\n}"),
        Txt("**concat** = walk dest to its '\\0', then copy src there. **compare** = skip the common prefix, then return the difference of the first different chars (negative, 0 or positive)."),
        Warn("The brief says `count_vowels(\"OpenAI\")` is 3, but O, e, A **and I** are vowels: the correct answer is **4**."),
        Practice("strlen_ptr,count_vowels,copy_string,concat_strings,compare_strings"),
    });

    // ================================================= MEMORY MANAGEMENT ===
    Add("mem_stack", S_MEMORY, "The stack, scope and dangling pointers", "Local variables die when their block ends", {
        Txt("A local variable (`int x;` inside a function or block) lives on the **stack**. When its block ends it goes **out of scope** and that memory can be reused."),
        Txt("A pointer that still holds the address of a dead variable is a **dangling pointer**. Using it is undefined behaviour: it may print 5, garbage, or crash."),
        Code("int* getPtrToFive() {\n    int x = 5;\n    return &x;     // WRONG: x dies when the function returns\n}"),
        Demo("Watch the pointer dangle", SceneId::Visualizer, "dangling"),
        Txt("**Globals and shadowing.** A global variable lives for the whole program. If a function declares a local with the same name, "
            "the compiler makes a **new** local variable and only changes that one."),
        Code("int glVar = 10;\nvoid f() {\n    int glVar;     // a NEW local\n    glVar = 20;    // the global is still 10\n}"),
        Check("Where does `int x = 5;` inside a function live?", {"On the stack", "On the heap", "In a global", "In a register forever"},
              "Locals are on the stack and are freed automatically at the end of their block."),
    });

    Add("mem_heap", S_MEMORY, "The heap: new and delete", "Memory that lives until you free it", {
        Txt("`new` allocates on the **heap** and returns a pointer. The memory stays until **you** call `delete`. That fixes getPtrToFive:"),
        Code("int* getPtrToFive() {\n    int* x = new int;\n    *x = 5;\n    return x;          // fine: heap memory survives\n}\n\nint* p = getPtrToFive();\ncout << *p;   // 5\ndelete p;"),
        Txt("The rules - break any of them and the behaviour is undefined:"),
        Table("rule|wrong|why\nevery new needs one delete|delete only after a loop|earlier blocks are leaked\nno use after delete|delete p; cout << *p;|the memory is gone\nno double delete|delete p; delete p;|crashes or corrupts memory\nonly delete what new gave you|int x; delete &x;|x is on the stack"),
        Demo("See a memory leak happen", SceneId::Visualizer, "leak"),
        Demo("...and the fixed version", SceneId::Visualizer, "leak_fixed"),
        Tip("After `delete p;` write `p = nullptr;`. A null pointer is easy to check, and deleting nullptr is harmless."),
        Practice("createArray"),
    });

    Add("mem_arrays", S_MEMORY, "Dynamic arrays, 2D arrays and malloc", "new[] / delete[], rows of rows, and the C way", {
        Txt("Stack arrays need a size known at compile time. With `new[]` the size can come from the user. Free it with `delete[]` (with the brackets)."),
        Code("int numItems;\ncin >> numItems;\nint* arr = new int[numItems];\n// ... use arr[0] .. arr[numItems - 1]\ndelete[] arr;"),
        Demo("Step through new[] and delete[]", SceneId::Visualizer, "dyn_array"),
        Txt("**2D on the heap**: first an array of row pointers, then each row. Free in the **reverse** order, or you lose the row pointers."),
        Walk("int** arr2D = new int*[rows];\nfor (int r = 0; r < rows; r++) arr2D[r] = new int[cols];\n// ... use arr2D[r][c]\nfor (int r = 0; r < rows; r++) delete[] arr2D[r];\ndelete[] arr2D;", {
            {0, "An array of `rows` pointers. Each one will point to a row."},
            {1, "Allocate every row."},
            {3, "Free each row first..."},
            {4, "...then the array of pointers. Doing this first would leak every row."}}),
        Txt("**C's version** is `malloc` / `free`. malloc takes a number of **bytes** and can fail, so always check for NULL."),
        Code("int* arr = (int*)malloc(size * sizeof(int));\nif (arr == NULL) {\n    printf(\"Memory allocation failed!\\n\");\n    return 1;\n}\n// ...\nfree(arr);\narr = NULL;"),
        Demo("malloc and free in the visualizer", SceneId::Visualizer, "malloc"),
        Practice("growArray,create2D"),
    });

    // ======================================================= LINKED LISTS ===
    Add("lists_singly", S_LISTS, "Singly linked lists", "Nodes on the heap, joined by next pointers", {
        Txt("A linked list is a chain of **nodes**. Each node holds data and a pointer `next` to the following node. "
            "The list itself only stores `head`, the first node. The last node's next is `nullptr`. "
            "Unlike an array, the nodes are created one by one with `new`, anywhere in memory."),
        Code("class Node {\npublic:\n    int data;\n    Node* next;\n    Node(int value) : data(value), next(nullptr) {}\n};"),
        Walk("void InsertAtBeginning(int value) {\n    Node* newNode = new Node(value);\n    newNode->next = head;\n    head = newNode;\n}", {
            {1, "Create the node on the heap. Its next is nullptr for now."},
            {2, "Link the new node to the current first node. Nothing is lost yet."},
            {3, "Only now move head. If you did this first, `newNode->next = head` would point the node at itself and the rest of the list would be lost."}}),
        Walk("void InsertAtEnd(int value) {\n    Node* newNode = new Node(value);\n    if (head == nullptr) {\n        head = newNode;\n        return;\n    }\n    Node* temp = head;\n    while (temp->next != nullptr) {\n        temp = temp->next;\n    }\n    temp->next = newNode;\n}", {
            {2, "Empty list: the new node is the whole list."},
            {6, "temp walks the list from head."},
            {7, "Stop on the **last** node (its next is nullptr), not after it."},
            {10, "The last node now links to the new node. Walking makes this O(n)."}}),
        Demo("Animate it in the Linked List Lab", SceneId::ListLab, "singly"),
        Practice("list_insert_begin_end"),
    });

    Add("lists_position", S_LISTS, "Insert, delete and reverse", "Stop on the node BEFORE the position", {
        Txt("To change the link **into** position p you need the node **before** it. Walk `temp` p - 1 steps from head."),
        Walk("bool InsertAtPosition(int value, int position) {\n    if (position < 0 || position > Size()) return false;\n    if (position == 0) { InsertAtBeginning(value); return true; }\n    Node* newNode = new Node(value);\n    Node* temp = head;\n    for (int i = 0; i < position - 1; i++) temp = temp->next;\n    newNode->next = temp->next;\n    temp->next = newNode;\n    return true;\n}", {
            {1, "Valid positions are 0 .. size (size means 'at the end')."},
            {2, "Position 0 has no node before it: reuse InsertAtBeginning."},
            {5, "p - 1 steps: temp stops on the node before the gap."},
            {6, "1) Link the new node to the rest of the list first."},
            {7, "2) Then redirect temp. In the other order you would lose the rest of the list."}}),
        Txt("**Delete at position**: walk to the node before, bypass the node, then `delete` it - otherwise it leaks."),
        Code("Node* toDelete = temp->next;\ntemp->next = toDelete->next;   // bypass\ndelete toDelete;               // free it"),
        Txt("**Reverse** flips every arrow with three pointers:"),
        Walk("Node* prev = nullptr;\nNode* current = head;\nwhile (current != nullptr) {\n    Node* nextNode = current->next;\n    current->next = prev;\n    prev = current;\n    current = nextNode;\n}\nhead = prev;", {
            {0, "The new last node will point to nullptr."},
            {3, "Save the rest of the list before breaking the link."},
            {4, "Flip the arrow backwards."},
            {5, "Move prev and current one step forward."},
            {8, "current is nullptr: prev is the old last node, now the head."}}),
        Demo("Try Insert, Delete and Reverse in the lab", SceneId::ListLab, "singly"),
        Practice("list_insert_position,list_delete_position,list_reverse"),
    });

    Add("lists_doubly", S_LISTS, "Doubly linked lists", "prevPointer, nextPointer, head and tail", {
        Txt("A `DoublyLinkedNode` has **two** links: `prevPointer` and `nextPointer`. The list keeps `head` **and** `tail`, "
            "so InsertAtEnd needs no walking: it is O(1)."),
        Code("class DoublyLinkedNode {\npublic:\n    int data;\n    DoublyLinkedNode* prevPointer;\n    DoublyLinkedNode* nextPointer;\n};"),
        Walk("newNode->nextPointer = temp->nextPointer;\nnewNode->prevPointer = temp;\ntemp->nextPointer->prevPointer = newNode;\ntemp->nextPointer = newNode;", {
            {0, "Inserting between temp and the node after it: four links change. 1) the new node points forward to the next node."},
            {1, "2) ...and back to temp."},
            {2, "3) The next node points back to the new node. This still needs the OLD temp->nextPointer..."},
            {3, "4) ...so redirecting temp must be the **last** line."}}),
        Table("case|what to update\nempty list|head = tail = newNode\ninsert at 0|old head's prevPointer, then head\ninsert at size|old tail's nextPointer, then tail\nposition < 0 or > size|return false"),
        Demo("Animate the four links", SceneId::ListLab, "doubly"),
        Practice("dlist_insert_begin_end,dlist_insert_position"),
    });
}

} // namespace

const std::vector<Lesson>& Lessons() {
    if (gLessons.empty()) Build();
    return gLessons;
}
