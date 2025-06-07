#ifndef TFLM_C_API_H
#define TFLM_C_API_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Error codes
typedef enum {
    TFLM_OK = 0,
    TFLM_ERROR_INVALID_ARGUMENT = -1,
    TFLM_ERROR_INSUFFICIENT_MEMORY = -2,
    TFLM_ERROR_NOT_SUPPORTED = -3,
    TFLM_ERROR_GENERIC = -4,
    TFLM_ERROR_MODEL_NOT_LOADED = -5,
    TFLM_ERROR_INFERENCE_FAILED = -6
} tflm_status_t;

// Forward declarations
typedef struct tflm_interpreter tflm_interpreter_t;

// Model loading and initialization
tflm_status_t tflm_create_interpreter(const uint8_t* model_data, 
                                      size_t model_size,
                                      uint8_t* tensor_arena,
                                      size_t tensor_arena_size,
                                      tflm_interpreter_t** interpreter);

tflm_status_t tflm_destroy_interpreter(tflm_interpreter_t* interpreter);

// Tensor operations
tflm_status_t tflm_get_input_size(tflm_interpreter_t* interpreter, size_t* size);
tflm_status_t tflm_get_output_size(tflm_interpreter_t* interpreter, size_t* size);

tflm_status_t tflm_set_input_data(tflm_interpreter_t* interpreter, 
                                  const uint8_t* input_data, 
                                  size_t input_size);

tflm_status_t tflm_get_output_data(tflm_interpreter_t* interpreter, 
                                   uint8_t* output_data, 
                                   size_t output_size);

// Inference
tflm_status_t tflm_invoke(tflm_interpreter_t* interpreter);

// Utility functions
const char* tflm_status_string(tflm_status_t status);

#ifdef __cplusplus
}
#endif

#endif // TFLM_C_API_H