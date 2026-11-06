// ============================================================================
// exam2_pointers.h  -  Coding Exam 2, Part 1: pointer functions.
// Rule from the brief: NO array indexing (arr[i]) - only pointer arithmetic.
// ============================================================================
#pragma once

void swap_ints(int *a, int *b);
void swap_chars(char *a, char *b);
int  find_max(int *arr, int size);
void reverse_array(int *arr, int size);
int  strlen_ptr(char *str);
int  count_vowels(char *str);
void copy_string(char *dest, char *src);
void concat_strings(char *dest, char *src);
int  sum_array(int *arr, int size);
int  compare_strings(char *str1, char *str2);
