// ============================================================================
// exam_output.h
// ----------------------------------------------------------------------------
// The exam functions that "print" (triangles, PrintArray, Display...) send
// their text through examPrintf(). By default it behaves exactly like printf,
// so the code is identical to what you would write in the exam.
//
// The app redirects the output into its on-screen console by changing the
// FUNCTION POINTER `examOut` (see lecture: "Function Pointers").
// ============================================================================
#pragma once

typedef void (*OutputFn)(const char* text);

// Where printed text goes. Default: stdout. The app swaps this pointer.
extern OutputFn examOut;

// printf-style helper used by every exam function that prints.
void examPrintf(const char* format, ...);

// Writes to stdout (the default value of examOut).
void examOutToStdout(const char* text);
