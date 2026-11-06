#include "exam_output.h"
#include <cstdarg>
#include <cstdio>

void examOutToStdout(const char* text) {
    fputs(text, stdout);
}

OutputFn examOut = examOutToStdout;

void examPrintf(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    examOut(buffer);   // call through the function pointer
}
