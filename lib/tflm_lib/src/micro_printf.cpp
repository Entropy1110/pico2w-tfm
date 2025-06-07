#include <cstdio>
#include <cstdarg>

extern "C" {

// Simple implementation of MicroPrintf for TFLM
int MicroPrintf(const char* format, ...) {
    // Simple no-op implementation for embedded systems
    (void)format;
    return 0;
}

} // extern "C"