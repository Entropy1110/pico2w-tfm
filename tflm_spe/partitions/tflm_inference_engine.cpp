/*
 * Copyright (c) 2024, Your Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tflm_inference_engine.h"
#include "tfm_log_unpriv.h"
#include <string.h>
#include <stdlib.h>

#if TFLM_LIBRARY_AVAILABLE

/* TFLM includes */
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

/* Global model and interpreter state */
static const tflite::Model* g_model = nullptr;
static tflite::MicroInterpreter* g_interpreter = nullptr;
static bool g_model_initialized = false;
static uint32_t g_current_model_id = 1;

/* Model arena */
constexpr int kArenaSize = 64 * 1024;  // 64KB arena
static uint8_t g_arena[kArenaSize];

/* Op resolver type */
typedef tflite::MicroMutableOpResolver<10> SimpleOpResolver;

/* Model storage structure */
typedef struct {
    uint32_t model_id;
    uint32_t input_size;
    uint32_t output_size;
    uint32_t model_version;
    bool is_initialized;
} model_info_cache_t;

static model_info_cache_t g_model_cache = {0};

extern "C" {

psa_status_t tflm_init_model(const uint8_t *model_data,
                              size_t model_size,
                              uint32_t *model_id)
{
    INFO_UNPRIV_RAW("[TFLM Engine] Initializing model, size: %d", model_size);

    if (!model_data || !model_id || model_size == 0) {
        INFO_UNPRIV_RAW("[TFLM Engine] Invalid parameters");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* Cleanup any existing model */
    if (g_interpreter != nullptr) {
        delete g_interpreter;
        g_interpreter = nullptr;
    }
    g_model_initialized = false;

    /* Map the model into a usable data structure */
    g_model = tflite::GetModel(model_data);
    if (g_model->version() != TFLITE_SCHEMA_VERSION) {
        INFO_UNPRIV_RAW("[TFLM Engine] Model schema version mismatch. Expected: %d, Got: %d", 
                   TFLITE_SCHEMA_VERSION, g_model->version());
        return PSA_ERROR_NOT_SUPPORTED;
    }

    /* Create and register operations resolver */
    static SimpleOpResolver op_resolver;
    /* Add common ops - for now just keeping it simple */
    
    /* Create interpreter */
    static tflite::MicroInterpreter static_interpreter(g_model, op_resolver, 
                                                        g_arena, kArenaSize);
    g_interpreter = &static_interpreter;

    /* Allocate tensors */
    if (g_interpreter->AllocateTensors() != kTfLiteOk) {
        INFO_UNPRIV_RAW("[TFLM Engine] Failed to allocate tensors");
        g_interpreter = nullptr;
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }

    /* Get input and output tensor info */
    TfLiteTensor* input = g_interpreter->input(0);
    TfLiteTensor* output = g_interpreter->output(0);

    if (input == nullptr || output == nullptr) {
        INFO_UNPRIV_RAW("[TFLM Engine] Invalid input/output tensors");
        g_interpreter = nullptr;
        return PSA_ERROR_GENERIC_ERROR;
    }

    /* Fill model cache */
    g_model_cache.model_id = g_current_model_id++;
    g_model_cache.input_size = input->bytes;
    g_model_cache.output_size = output->bytes;
    g_model_cache.model_version = 1;
    g_model_cache.is_initialized = true;

    /* Return model ID */
    *model_id = g_model_cache.model_id;

    g_model_initialized = true;

    INFO_UNPRIV_RAW("[TFLM Engine] Model initialized successfully with ID: %d", *model_id);
    INFO_UNPRIV_RAW("[TFLM Engine] Arena used: %u bytes", g_interpreter->arena_used_bytes());
    INFO_UNPRIV_RAW("[TFLM Engine] Input size: %d bytes, Output size: %d bytes",
               g_model_cache.input_size, g_model_cache.output_size);
    
    return PSA_SUCCESS;
}

psa_status_t tflm_get_model_info(uint32_t model_id,
                                 uint32_t *input_size,
                                 uint32_t *output_size,
                                 uint32_t *model_version)
{
    INFO_UNPRIV_RAW("[TFLM Engine] Getting model info for ID: %d", model_id);

    if (!input_size || !output_size || !model_version) {
        INFO_UNPRIV_RAW("[TFLM Engine] Invalid parameters");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    if (!g_model_cache.is_initialized || g_model_cache.model_id != model_id) {
        INFO_UNPRIV_RAW("[TFLM Engine] Model not found or not initialized");
        return PSA_ERROR_DOES_NOT_EXIST;
    }

    *input_size = g_model_cache.input_size;
    *output_size = g_model_cache.output_size;
    *model_version = g_model_cache.model_version;

    INFO_UNPRIV_RAW("[TFLM Engine] Model info retrieved successfully");
    return PSA_SUCCESS;
}

psa_status_t tflm_run_inference(uint32_t model_id,
                                const uint8_t *input_data,
                                size_t input_size,
                                uint8_t *output_data,
                                size_t output_buffer_size,
                                size_t *actual_output_size)
{
    INFO_UNPRIV_RAW("[TFLM Engine] Running inference for model ID: %d, input size: %d", model_id, input_size);

    if (!input_data || !output_data || !actual_output_size) {
        INFO_UNPRIV_RAW("[TFLM Engine] Invalid parameters");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    if (!g_interpreter || !g_model_initialized || !g_model_cache.is_initialized || g_model_cache.model_id != model_id) {
        INFO_UNPRIV_RAW("[TFLM Engine] Model not initialized or model ID mismatch");
        return PSA_ERROR_BAD_STATE;
    }

    /* Get input tensor */
    TfLiteTensor* input_tensor = g_interpreter->input(0);
    if (!input_tensor) {
        INFO_UNPRIV_RAW("[TFLM Engine] Invalid input tensor");
        return PSA_ERROR_GENERIC_ERROR;
    }

    /* Check input size */
    if (input_size != input_tensor->bytes) {
        INFO_UNPRIV_RAW("[TFLM Engine] Input size mismatch: expected %d, got %d",
                   input_tensor->bytes, input_size);
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* Copy input data to tensor - using TFLM pattern */
    memcpy(tflite::GetTensorData<uint8_t>(input_tensor), input_data, input_size);

    /* Run inference */
    if (g_interpreter->Invoke() != kTfLiteOk) {
        INFO_UNPRIV_RAW("[TFLM Engine] Inference failed");
        return PSA_ERROR_GENERIC_ERROR;
    }

    /* Get output tensor */
    TfLiteTensor* output_tensor = g_interpreter->output(0);
    if (!output_tensor) {
        INFO_UNPRIV_RAW("[TFLM Engine] Invalid output tensor");
        return PSA_ERROR_GENERIC_ERROR;
    }

    /* Check output buffer size */
    if (output_buffer_size < output_tensor->bytes) {
        INFO_UNPRIV_RAW("[TFLM Engine] Output buffer too small: need %d, got %d",
                   output_tensor->bytes, output_buffer_size);
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }

    /* Copy output data - using TFLM pattern */
    memcpy(output_data, tflite::GetTensorData<uint8_t>(output_tensor), output_tensor->bytes);
    *actual_output_size = output_tensor->bytes;

    INFO_UNPRIV_RAW("[TFLM Engine] Inference completed, output size: %d", 
               *actual_output_size);
    
    return PSA_SUCCESS;
}

/* Cleanup function */
void tflm_cleanup_model(void)
{
    INFO_UNPRIV_RAW("[TFLM Engine] Cleaning up model");
    
    g_model_initialized = false;
    g_model = nullptr;
    g_model_cache.is_initialized = false;
    g_model_cache.model_id = 0;
    
    /* Note: g_interpreter points to static_interpreter, so we don't delete it */
    g_interpreter = nullptr;
}

/* Decrypt model function - for now just returns the same data */
psa_status_t tflm_decrypt_model(const uint8_t *encrypted_model_data,
                                size_t encrypted_model_size,
                                uint8_t **decrypted_model_data,
                                size_t *decrypted_model_size)
{
    INFO_UNPRIV_RAW("[TFLM Engine] Decrypting model (stub implementation)");
    
    if (!encrypted_model_data || !decrypted_model_data || !decrypted_model_size) {
        INFO_UNPRIV_RAW("[TFLM Engine] Invalid parameters");
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* For now, just allocate memory and copy the data */
    *decrypted_model_data = (uint8_t*)malloc(encrypted_model_size);
    if (!*decrypted_model_data) {
        INFO_UNPRIV_RAW("[TFLM Engine] Memory allocation failed");
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }
    
    memcpy(*decrypted_model_data, encrypted_model_data, encrypted_model_size);
    *decrypted_model_size = encrypted_model_size;
    
    INFO_UNPRIV_RAW("[TFLM Engine] Model decrypted successfully");
    return PSA_SUCCESS;
}

} // extern "C"

#else /* TFLM_LIBRARY_AVAILABLE */

/* TFLM library not available - provide stub implementations for echo test */

extern "C" {

psa_status_t tflm_init_model(const uint8_t *model_data,
                             size_t model_size,
                             uint32_t *model_id)
{
    INFO_UNPRIV_RAW("[SPE] tflm_init_model: TFLM library not available, returning error");
    (void)model_data;
    (void)model_size;
    (void)model_id;
    return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t tflm_get_model_info(uint32_t model_id,
                                 uint32_t *input_size,
                                 uint32_t *output_size,
                                 uint32_t *model_version)
{
    INFO_UNPRIV_RAW("[SPE] tflm_get_model_info: TFLM library not available, returning error");
    (void)model_id;
    (void)input_size;
    (void)output_size;
    (void)model_version;
    return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t tflm_run_inference(uint32_t model_id,
                                const uint8_t *input_data,
                                size_t input_size,
                                uint8_t *output_data,
                                size_t output_size,
                                size_t *actual_output_size)
{
    INFO_UNPRIV_RAW("[SPE] tflm_run_inference: TFLM library not available, returning error");
    (void)model_id;
    (void)input_data;
    (void)input_size;
    (void)output_data;
    (void)output_size;
    (void)actual_output_size;
    return PSA_ERROR_NOT_SUPPORTED;
}

void tflm_cleanup_model(void)
{
    INFO_UNPRIV_RAW("[SPE] tflm_cleanup_model: TFLM library not available");
    /* Nothing to clean up */
}

psa_status_t tflm_decrypt_model(const uint8_t *encrypted_model_data,
                                size_t encrypted_model_size,
                                uint8_t **decrypted_model_data,
                                size_t *decrypted_model_size)
{
    INFO_UNPRIV_RAW("[SPE] tflm_decrypt_model: TFLM library not available, returning error");
    (void)encrypted_model_data;
    (void)encrypted_model_size;
    (void)decrypted_model_data;
    (void)decrypted_model_size;
    return PSA_ERROR_NOT_SUPPORTED;
}

}

#endif /* TFLM_LIBRARY_AVAILABLE */
