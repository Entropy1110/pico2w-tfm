#ifndef CMSIS_NN_WRAPPER_H
#define CMSIS_NN_WRAPPER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// CMSIS-NN wrapper status codes
typedef enum {
    CMSIS_NN_SUCCESS = 0,
    CMSIS_NN_ERROR = -1,
    CMSIS_NN_ERROR_INVALID_PARAMETER = -2,
    CMSIS_NN_ERROR_MODEL_NOT_LOADED = -3,
    CMSIS_NN_ERROR_INFERENCE_FAILED = -4,
    CMSIS_NN_ERROR_MEMORY_ALLOCATION = -5,
    CMSIS_NN_ERROR_MODEL_LOADING_FAILED = -6
} cmsis_nn_status_t;

// Neural network model structure
typedef struct {
    // Model metadata
    uint8_t* model_data;
    size_t model_size;
    
    // Input/output tensors
    int8_t* input_data;
    int8_t* output_data;
    size_t input_size;
    size_t output_size;
    
    // Tensor dimensions
    int input_batches;
    int input_height;
    int input_width;
    int input_channels;
    
    int output_batches;
    int output_height;
    int output_width;
    int output_channels;
    
    // Quantization parameters
    int32_t input_offset;
    int32_t output_offset;
    float input_scale;
    float output_scale;
    
    // Working memory
    int8_t* buffer_a;
    int8_t* buffer_b;
    size_t buffer_size;
    
    // Model state
    int model_loaded;
} cmsis_nn_model_t;

// Core API functions
cmsis_nn_status_t cmsis_nn_create_model(cmsis_nn_model_t** model, 
                                       uint8_t* buffer_a, size_t buffer_a_size,
                                       uint8_t* buffer_b, size_t buffer_b_size);

cmsis_nn_status_t cmsis_nn_destroy_model(cmsis_nn_model_t* model);

cmsis_nn_status_t cmsis_nn_load_model(cmsis_nn_model_t* model, 
                                     const uint8_t* model_data, size_t model_size);

cmsis_nn_status_t cmsis_nn_set_input_data(cmsis_nn_model_t* model, 
                                         const int8_t* input_data, size_t data_size);

cmsis_nn_status_t cmsis_nn_run_inference(cmsis_nn_model_t* model);

cmsis_nn_status_t cmsis_nn_get_output_data(cmsis_nn_model_t* model, 
                                          int8_t* output_data, size_t data_size);

cmsis_nn_status_t cmsis_nn_get_input_size(cmsis_nn_model_t* model, size_t* size);

cmsis_nn_status_t cmsis_nn_get_output_size(cmsis_nn_model_t* model, size_t* size);

// Utility functions
const char* cmsis_nn_status_string(cmsis_nn_status_t status);

// Neural network layer operations (examples)
cmsis_nn_status_t cmsis_nn_conv2d(const int8_t* input,
                                 const int8_t* weights,
                                 const int32_t* bias,
                                 int8_t* output,
                                 const int input_height,
                                 const int input_width,
                                 const int input_channels,
                                 const int output_channels,
                                 const int kernel_height,
                                 const int kernel_width,
                                 const int stride_h,
                                 const int stride_w,
                                 const int pad_h,
                                 const int pad_w,
                                 const int32_t input_offset,
                                 const int32_t output_offset,
                                 const int32_t input_multiplier,
                                 const int input_shift,
                                 const int32_t output_multiplier,
                                 const int output_shift,
                                 const int32_t output_activation_min,
                                 const int32_t output_activation_max);

cmsis_nn_status_t cmsis_nn_fully_connected(const int8_t* input,
                                          const int8_t* weights,
                                          const int32_t* bias,
                                          int8_t* output,
                                          const int input_size,
                                          const int output_size,
                                          const int32_t input_offset,
                                          const int32_t output_offset,
                                          const int32_t input_multiplier,
                                          const int input_shift,
                                          const int32_t output_multiplier,
                                          const int output_shift,
                                          const int32_t output_activation_min,
                                          const int32_t output_activation_max);

#ifdef __cplusplus
}
#endif

#endif // CMSIS_NN_WRAPPER_H