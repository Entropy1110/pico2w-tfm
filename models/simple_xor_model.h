#ifndef SIMPLE_XOR_MODEL_H
#define SIMPLE_XOR_MODEL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Model metadata
extern const char* MODEL_NAME;
extern const char* MODEL_VERSION;
#define MODEL_INPUT_SIZE 2
#define MODEL_OUTPUT_SIZE 1
#define MODEL_HIDDEN_SIZE 4

// Quantization parameters
extern const int32_t INPUT_ZERO_POINT;
extern const float INPUT_SCALE;
extern const int32_t OUTPUT_ZERO_POINT;
extern const float OUTPUT_SCALE;

// Test data functions
const uint8_t* get_simple_xor_model_data(size_t* size);
const int8_t* get_xor_test_input(int test_case);
int8_t get_xor_expected_output(int test_case);
int validate_xor_output(int test_case, int8_t actual_output);

// Helper functions for quantization
static inline int8_t quantize_input(float value) {
    int32_t quantized = (int32_t)(value / INPUT_SCALE) + INPUT_ZERO_POINT;
    if (quantized < -128) quantized = -128;
    if (quantized > 127) quantized = 127;
    return (int8_t)quantized;
}

static inline float dequantize_output(int8_t quantized) {
    return (float)(quantized - OUTPUT_ZERO_POINT) * OUTPUT_SCALE;
}

#ifdef __cplusplus
}
#endif

#endif // SIMPLE_XOR_MODEL_H