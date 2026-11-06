// ============================================================================
// exam1.cpp  -  Reference solutions for Coding Exam 1 (Lab1.cpp)
// ----------------------------------------------------------------------------
// The text between "@begin name" and "@end" markers is loaded by the app at
// runtime and shown in the Function Lab / Code Builder. What you see in the
// app is exactly the code that was compiled.
// ============================================================================
#include "exam1.h"
#include "exam_output.h"
#include <cstdlib>
#include <cstring>

// @begin isLeapYear
bool isLeapYear(int year) {
    if (year % 400 == 0) return true;   // 1600, 2000: leap
    if (year % 100 == 0) return false;  // 1700, 1900: not leap
    return year % 4 == 0;               // 2016: leap
}
// @end

// @begin Reversed
int Reversed(int testNumber) {
    int reversed = 0;
    while (testNumber != 0) {
        int digit = testNumber % 10;        // take the last digit
        reversed = reversed * 10 + digit;   // append it to the result
        testNumber = testNumber / 10;       // drop the last digit
    }
    return reversed;
}
// @end

// @begin isAPalindrome
bool isAPalindrome(int testNumber) {
    if (testNumber < 0) return false;
    return Reversed(testNumber) == testNumber;   // 121 -> 121
}
// @end

// @begin isAPrimeNumber
bool isAPrimeNumber(int numbertoTest) {
    if (numbertoTest < 2) return false;
    if (numbertoTest % 2 == 0) return numbertoTest == 2;
    // Only test odd divisors up to sqrt(n). long long avoids overflow of i*i
    for (long long i = 3; i * i <= numbertoTest; i += 2) {
        if (numbertoTest % i == 0) return false;
    }
    return true;
}
// @end

// @begin convertBinarytoDecimal
int convertBinarytoDecimal(int binaryNumber) {
    int decimal = 0;
    int base = 1;                            // 2^0
    while (binaryNumber > 0) {
        int bit = binaryNumber % 10;         // last binary digit (0 or 1)
        decimal = decimal + bit * base;
        base = base * 2;                     // next power of 2 (no << used)
        binaryNumber = binaryNumber / 10;
    }
    return decimal;
}
// @end

// @begin drawRightAngledTriangle
void drawRightAngledTriangle() {
    for (int row = 1; row <= 4; row++) {
        for (int col = 1; col <= row; col++) {   // row 3 prints 3 A's
            examPrintf("A");
        }
        examPrintf("\n");
    }
}
// @end

// @begin drawIsocelesTriangle
void drawIsocelesTriangle() {
    for (int row = 1; row <= 7; row++) {
        int count;
        if (row <= 4) count = row;          // growing half: 1 2 3 4
        else count = 8 - row;               // shrinking half: 3 2 1
        for (int col = 1; col <= count; col++) {
            examPrintf("A");
        }
        examPrintf("\n");
    }
}
// @end

// @begin drawIsocelesTriangle2
void drawIsocelesTriangle2() {
    for (int row = -3; row <= 3; row++) {
        int count = 4 - abs(row);           // 1 2 3 4 3 2 1 with no if
        for (int col = 1; col <= count; col++) {
            examPrintf("A");
        }
        examPrintf("\n");
    }
}
// @end

// @begin find
int find(int size, int arr[], int toFind) {
    for (int i = 0; i < size; i++) {
        if (arr[i] == toFind) return i;     // first index where it appears
    }
    return -1;                              // not found
}
// @end

// @begin find2ndLargest
int find2ndLargest(int size, int arr[]) {
    int largest = -1, second = -1;
    for (int i = 0; i < size; i++) {
        if (arr[i] > largest) {
            second = largest;               // old largest becomes 2nd
            largest = arr[i];
        } else if (arr[i] < largest && arr[i] > second) {
            second = arr[i];                // new 2nd (must be < largest)
        }
    }
    return second;                          // stays -1 if none exists
}
// @end

// @begin copyArraytoArray
void copyArraytoArray(int size, int arr1[], int arr2[]) {
    for (int i = 0; i < size; i++) {
        arr2[i] = arr1[i];
    }
}
// @end

// @begin insertElement
bool insertElement(int& size, int& count, int arr[], int elementToInsert, int insertIndex) {
    if (count >= size || insertIndex < 0 || insertIndex > count) return false;
    for (int i = count; i > insertIndex; i--) {
        arr[i] = arr[i - 1];                // shift right, from the end
    }
    arr[insertIndex] = elementToInsert;
    count++;                                // count is a reference: caller sees it
    return true;
}
// @end

// @begin deleteElement
bool deleteElement(int& size, int& count, int arr[], int deleteIndex) {
    if (count > size || deleteIndex < 0 || deleteIndex >= count) return false;
    for (int i = deleteIndex; i < count - 1; i++) {
        arr[i] = arr[i + 1];                // shift left over the deleted slot
    }
    count--;
    return true;
}
// @end

// @begin frequencyCount
int frequencyCount(int size, int arr[], int value) {
    int frequency = 0;
    for (int i = 0; i < size; i++) {
        if (arr[i] == value) frequency++;
    }
    return frequency;
}
// @end

// @begin countDuplicates
int countDuplicates(int size, int arr[]) {
    int duplicates = 0;
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < i; j++) {
            if (arr[j] == arr[i]) {         // seen before -> arr[i] is a duplicate
                duplicates++;
                break;                      // count arr[i] only once
            }
        }
    }
    return duplicates;
}
// @end

// @begin reverse
void reverse(int size, int arr[]) {
    for (int left = 0, right = size - 1; left < right; left++, right--) {
        int temp = arr[left];
        arr[left] = arr[right];
        arr[right] = temp;
    }
}
// @end

// @begin rotateLeft
int rotateLeft(int size, int arr[]) {
    if (size <= 0) return -1;
    int first = arr[0];                     // remember the first element
    for (int i = 0; i < size - 1; i++) {
        arr[i] = arr[i + 1];                // everything moves one step left
    }
    arr[size - 1] = first;                  // first goes to the end
    return first;
}
// @end

// @begin twoMovies
bool twoMovies(int flightLength, int movieLengths[], int size) {
    for (int i = 0; i < size; i++) {
        for (int j = i + 1; j < size; j++) {    // j > i: never the same movie twice
            if (movieLengths[i] + movieLengths[j] == flightLength) return true;
        }
    }
    return false;
}
// @end

// @begin wordCounter
static bool isLetter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int wordCounter(int size, char characters[]) {
    int words = 0;
    bool inWord = false;
    for (int i = 0; i < size && characters[i] != '\0'; i++) {
        if (isLetter(characters[i])) {
            if (!inWord) words++;           // a new word starts here
            inWord = true;
        } else {
            inWord = false;                 // space (or symbol) ends a word
        }
    }
    return words;
}
// @end

// @begin PrintArray
void PrintArray(int array[4][6]) {
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 6; col++) {
            examPrintf("%3d ", array[row][col]);
        }
        examPrintf("\n");
    }
}
// @end

// @begin ReplaceWithMinusOne
int ReplaceWithMinusOne(int array[4][6]) {
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 6; col++) {
            int value = array[row][col];
            if (value < 1 || value > 4) continue;   // 0, -1, -2 never cluster

            // 1) HORIZONTAL first: count identical items to the right
            int length = 1;
            while (col + length < 6 && array[row][col + length] == value) length++;
            if (length >= 3) {
                for (int k = 0; k < length; k++) array[row][col + k] = -1;
                return value * length;              // score = sum of the values
            }

            // 2) then VERTICAL: count identical items below
            length = 1;
            while (row + length < 4 && array[row + length][col] == value) length++;
            if (length >= 3) {
                for (int k = 0; k < length; k++) array[row + k][col] = -1;
                return value * length;
            }
        }
    }
    return 0;                                       // no cluster found
}
// @end

// @begin FallDownAndReplace
void FallDownAndReplace(int array[4][6]) {
    for (int col = 0; col < 6; col++) {
        int writeRow = 3;                           // next free slot, from the bottom
        for (int row = 3; row >= 0; row--) {
            if (array[row][col] != -1) {
                array[writeRow][col] = array[row][col];   // item falls down
                writeRow--;
            }
        }
        while (writeRow >= 0) {
            array[writeRow][col] = -2;              // new items at the top
            writeRow--;
        }
    }
}
// @end

// @begin FindAllVariables
static bool isIdentChar(char c) {
    return isLetter(c) || (c >= '0' && c <= '9') || c == '_';
}

// If "int name" / "char name" / "int* name" starts at code[i], copy the
// variable name into 'name' and return true. "int main()" returns false.
static bool declarationAt(const char code[], int i, char name[]) {
    int typeLength;
    if (strncmp(&code[i], "int", 3) == 0) typeLength = 3;
    else if (strncmp(&code[i], "char", 4) == 0) typeLength = 4;
    else return false;
    if (i > 0 && isIdentChar(code[i - 1])) return false;        // e.g. "print"
    int j = i + typeLength;
    if (code[j] != ' ' && code[j] != '\t' && code[j] != '*') return false;  // e.g. "integer"
    while (code[j] == ' ' || code[j] == '\t' || code[j] == '*') j++;
    int n = 0;
    while (isIdentChar(code[j]) && n < 63) name[n++] = code[j++];
    name[n] = '\0';
    if (n == 0) return false;
    while (code[j] == ' ' || code[j] == '\t') j++;
    return code[j] != '(';                                      // '(' means a function
}

static void appendName(char variables[], const char name[]) {
    if (variables[0] != '\0') strcat(variables, ",");
    strcat(variables, name);
}

// Skips string literals, char literals and // comments. Returns the index of
// the last character skipped (or i itself if nothing was skipped).
static int skipNonCode(const char code[], int i) {
    if (code[i] == '"' || code[i] == '\'') {
        char quote = code[i];
        i++;
        while (code[i] != '\0' && code[i] != quote) {
            if (code[i] == '\\' && code[i + 1] != '\0') i++;
            i++;
        }
        return code[i] == '\0' ? i - 1 : i;
    }
    if (code[i] == '/' && code[i + 1] == '/') {
        while (code[i + 1] != '\0' && code[i + 1] != '\n') i++;
    }
    return i;
}

void FindAllVariables(char variables[], char code[]) {
    variables[0] = '\0';
    for (int i = 0; code[i] != '\0'; i++) {
        int skipped = skipNonCode(code, i);
        if (skipped != i) { i = skipped; continue; }
        char name[64];
        if (declarationAt(code, i, name)) appendName(variables, name);
    }
}
// @end

// @begin FindAllVariablesInScope
void FindAllVariablesInScope(char variables[], char code[], int lineNumber) {
    char names[100][64];        // a STACK of declared variables...
    int depths[100];            // ...and the brace depth of each one
    int count = 0;
    int depth = 0;
    int line = 1;
    variables[0] = '\0';

    for (int i = 0; code[i] != '\0' && line <= lineNumber; i++) {
        int skipped = skipNonCode(code, i);
        if (skipped != i) { i = skipped; continue; }
        char c = code[i];
        if (c == '\n') {
            line++;
        } else if (c == '{') {
            depth++;
        } else if (c == '}') {
            depth--;
            while (count > 0 && depths[count - 1] > depth) count--;   // out of scope
        } else if (count < 100 && declarationAt(code, i, names[count])) {
            depths[count] = depth;
            count++;
        }
    }
    for (int k = 0; k < count; k++) appendName(variables, names[k]);
}
// @end
