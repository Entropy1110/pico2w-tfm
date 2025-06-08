#include <cstdio>
#include <cstdarg>

// Implementation of MicroPrintf for TFLM host test (C++ linkage to match header)
void MicroPrintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

// VMicroPrintf function needed by micro_error_reporter (C++ linkage)
void VMicroPrintf(const char* format, va_list args) {
    vprintf(format, args);
}

// Snprintf functions for completeness
int MicroSnprintf(char* buffer, size_t buf_size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, buf_size, format, args);
    va_end(args);
    return result;
}

int MicroVsnprintf(char* buffer, size_t buf_size, const char* format, va_list vlist) {
    return vsnprintf(buffer, buf_size, format, vlist);
}

// Additional debug functions for TFLM
extern "C" {
void DebugLog(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

int DebugVsnprintf(char* buffer, size_t buf_size, const char* format, va_list vlist) {
    return vsnprintf(buffer, buf_size, format, vlist);
}
}