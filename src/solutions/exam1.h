// ============================================================================
// exam1.h  -  Coding Exam 1 (Lab1.cpp) : C fundamentals, arrays, 2D arrays,
//             and parsing C code stored in a char array.
// ============================================================================
#pragma once

bool isLeapYear(int year);
int  Reversed(int testNumber);
bool isAPalindrome(int testNumber);
bool isAPrimeNumber(int numbertoTest);
int  convertBinarytoDecimal(int binaryNumber);

void drawRightAngledTriangle();
void drawIsocelesTriangle();
void drawIsocelesTriangle2();

int  find(int size, int arr[], int toFind);
int  find2ndLargest(int size, int arr[]);
void copyArraytoArray(int size, int arr1[], int arr2[]);
bool insertElement(int& size, int& count, int arr[], int elementToInsert, int insertIndex);
bool deleteElement(int& size, int& count, int arr[], int deleteIndex);
int  frequencyCount(int size, int arr[], int value);
int  countDuplicates(int size, int arr[]);
void reverse(int size, int arr[]);
int  rotateLeft(int size, int arr[]);
bool twoMovies(int flightLength, int movieLengths[], int size);
int  wordCounter(int size, char characters[]);

void PrintArray(int array[4][6]);
int  ReplaceWithMinusOne(int array[4][6]);
void FallDownAndReplace(int array[4][6]);

void FindAllVariables(char variables[], char code[]);
void FindAllVariablesInScope(char variables[], char code[], int lineNumber);
