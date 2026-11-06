// ============================================================================
// exam2_pointers.cpp  -  Reference solutions, Coding Exam 2 Part 1.
// Every function uses pointer arithmetic only: no arr[i] anywhere.
// ============================================================================
#include "exam2_pointers.h"

// @begin swap_ints
void swap_ints(int *a, int *b) {
    int temp = *a;      // value stored at address a
    *a = *b;
    *b = temp;
}
// @end

// @begin swap_chars
void swap_chars(char *a, char *b) {
    char temp = *a;
    *a = *b;
    *b = temp;
}
// @end

// @begin find_max
int find_max(int *arr, int size) {
    if (arr == nullptr || size <= 0) return 0;     // nothing to search
    int max = *arr;                                // first element
    for (int *p = arr + 1; p < arr + size; p++) {  // walk the pointer
        if (*p > max) max = *p;
    }
    return max;
}
// @end

// @begin reverse_array
void reverse_array(int *arr, int size) {
    if (arr == nullptr || size < 2) return;
    for (int *left = arr, *right = arr + size - 1; left < right; left++, right--) {
        int temp = *left;
        *left = *right;
        *right = temp;
    }
}
// @end

// @begin strlen_ptr
int strlen_ptr(char *str) {
    char *p = str;
    while (*p != '\0') {
        p++;                    // move to the next character
    }
    return (int)(p - str);      // pointer difference = number of chars
}
// @end

// @begin count_vowels
int count_vowels(char *str) {
    int count = 0;
    for (char *p = str; *p != '\0'; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = c + ('a' - 'A');     // to lowercase
        if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') count++;
    }
    return count;
}
// @end

// @begin copy_string
void copy_string(char *dest, char *src) {
    while (*src != '\0') {
        *dest = *src;           // copy one character
        dest++;
        src++;
    }
    *dest = '\0';               // never forget the terminator
}
// @end

// @begin concat_strings
void concat_strings(char *dest, char *src) {
    while (*dest != '\0') {
        dest++;                 // 1) walk to the end of dest
    }
    while (*src != '\0') {
        *dest = *src;           // 2) copy src onto the end
        dest++;
        src++;
    }
    *dest = '\0';
}
// @end

// @begin sum_array
int sum_array(int *arr, int size) {
    int sum = 0;
    for (int *p = arr; p < arr + size; p++) {
        sum += *p;
    }
    return sum;
}
// @end

// @begin compare_strings
int compare_strings(char *str1, char *str2) {
    while (*str1 != '\0' && *str1 == *str2) {
        str1++;                 // skip the common prefix
        str2++;
    }
    return (unsigned char)*str1 - (unsigned char)*str2;   // <0, 0 or >0
}
// @end
