#ifndef TFLM_C_INTERFACE_H
#define TFLM_C_INTERFACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration of context structure
typedef struct tflm_context tflm_context_t;

// Status enumeration
typedef enum {
    TFLM_SUCCESS = 0,
    TFLM_ERROR_INIT_FAILED = 1,
    TFLM_ERROR_MODEL_INVALID = 2,
    TFLM_ERROR_MEMORY_ALLOCATION = 3,
    TFLM_ERROR_CONTEXT_NULL = 4,
    TFLM_ERROR_INDEX_OUT_OF_RANGE = 5,
    TFLM_ERROR_INFERENCE_FAILED = 6,
    TFLM_ERROR_TENSOR_NULL = 7,
    TFLM_ERROR_ALREADY_INITIALIZED = 8,
    TFLM_ERROR_NOT_INITIALIZED = 9
} tflm_status_t;

// Tensor information structure
typedef struct {
    int dims[4];
    int num_dims;
    size_t bytes;
    int type;
} tflm_tensor_info_t;

// Generic TFLM interface functions
tflm_status_t tflm_init(void);
tflm_status_t tflm_deinit(void);

// Context management
tflm_context_t* tflm_create_context(const unsigned char* model_data,
                                   size_t model_size,
                                   unsigned char* tensor_arena,
                                   size_t arena_size);
void tflm_destroy_context(tflm_context_t* context);

// Tensor operations
tflm_status_t tflm_get_input_tensor_info(tflm_context_t* context,
                                         int index,
                                         tflm_tensor_info_t* info);
tflm_status_t tflm_get_output_tensor_info(tflm_context_t* context,
                                          int index,
                                          tflm_tensor_info_t* info);
float* tflm_get_input_tensor(tflm_context_t* context, int index);
float* tflm_get_output_tensor(tflm_context_t* context, int index);

// Inference
tflm_status_t tflm_invoke(tflm_context_t* context);

// Utility functions
const char* tflm_get_error_string(tflm_status_t status);

#ifdef __cplusplus
}
#endif

#endif // TFLM_C_INTERFACE_H