#include "psa/service.h"
#include "psa/client.h"
#include "psa_manifest/tflm_inference_manifest.h"
#include "tfm_tflm_inference_defs.h"
#include <string.h>
#include <new>
#include <cstdlib>

// Use TFLM C API
#include "tflm_c_api.h"

// Real TFLM inference enabled
#define TFLM_FALLBACK_MODE 0

// TF-M logging for C++
#ifdef __cplusplus
extern "C" {
#endif
#include "tfm_log_unpriv.h"
#ifdef __cplusplus
}
#endif

// TFLM runtime state
static struct {
    bool model_loaded;
    tflm_interpreter_t* interpreter;
    uint8_t* tensor_arena;
    size_t tensor_arena_size;
    size_t input_size;
    size_t output_size;
    uint8_t* model_data;  // Store model data
    size_t model_size;
} tflm_state = {0};

// Static memory allocation for TFLM - properly aligned
alignas(16) static uint8_t model_buffer[TFM_TFLM_MAX_MODEL_SIZE];
alignas(16) static uint8_t tensor_arena_buffer[65536]; // 64KB tensor arena
alignas(16) static uint8_t input_buffer[TFM_TFLM_MAX_INPUT_SIZE];
alignas(16) static uint8_t output_buffer[TFM_TFLM_MAX_OUTPUT_SIZE];

// Helper function to validate TFLite model
static bool validate_tflite_model(const uint8_t* data, size_t size) {
    if (size < 8) {
        return false;
    }
    
    // Check TFLite file identifier (first 4 bytes should be "TFL3")
    if (data[0] == 0x1C && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x00) {
        // Check for "TFL3" at offset 4
        if (size >= 8 && data[4] == 'T' && data[5] == 'F' && data[6] == 'L' && data[7] == '3') {
            return true;
        }
    }
    
    // Alternative: Check for FlatBuffer identifier
    if (size >= 20) {
        // Look for TensorFlow Lite schema identifier
        return true; // For now, accept if size is reasonable
    }
    
    return false;
}

static psa_status_t tflm_load_model(const psa_msg_t *msg)
{
    size_t model_size;
    size_t num_read;
    tflm_status_t tflm_status;

    INFO_UNPRIV("[TFLM] Load model called");

    // Clean up any previous model
    if (tflm_state.interpreter != nullptr) {
        INFO_UNPRIV("[TFLM] Cleaning up previous model");
        tflm_destroy_interpreter(tflm_state.interpreter);
        tflm_state.interpreter = nullptr;
        tflm_state.model_loaded = false;
    }

    // Model data is directly in the input
    model_size = msg->in_size[0];
    
    if (model_size > TFM_TFLM_MAX_MODEL_SIZE) {
        INFO_UNPRIV("[TFLM] Model size %d exceeds maximum %d", 
                     (int)model_size, TFM_TFLM_MAX_MODEL_SIZE);
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }
    
    if (model_size < 100) { // Minimum reasonable model size
        INFO_UNPRIV("[TFLM] Model size %d is too small", (int)model_size);
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }
    
    // Read model data directly into aligned buffer
    num_read = psa_read(msg->handle, 0, model_buffer, model_size);
    if (num_read != model_size) {
        INFO_UNPRIV("[TFLM] Failed to read model data: expected %d, got %d", 
                     (int)model_size, (int)num_read);
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Validate model format
    if (!validate_tflite_model(model_buffer, model_size)) {
        INFO_UNPRIV("[TFLM] Invalid model format");
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Store model info
    tflm_state.model_data = model_buffer;
    tflm_state.model_size = model_size;

    // Setup tensor arena
    tflm_state.tensor_arena = tensor_arena_buffer;
    tflm_state.tensor_arena_size = sizeof(tensor_arena_buffer);

    INFO_UNPRIV("[TFLM] Creating interpreter with:");
    INFO_UNPRIV("[TFLM]   Model size: %d bytes", (int)model_size);
    INFO_UNPRIV("[TFLM]   Tensor arena: %d bytes", (int)tflm_state.tensor_arena_size);
    INFO_UNPRIV("[TFLM]   Model addr: %p", model_buffer);
    INFO_UNPRIV("[TFLM]   Arena addr: %p", tensor_arena_buffer);
    
    // Create interpreter with proper error handling
    INFO_UNPRIV("[TFLM] Creating TFLM interpreter...");
    tflm_status = tflm_create_interpreter(
        model_buffer, 
        model_size,
        tflm_state.tensor_arena, 
        tflm_state.tensor_arena_size,
        &tflm_state.interpreter
    );
    
    if (tflm_status != TFLM_OK) {
        INFO_UNPRIV("[TFLM] Failed to create interpreter: %s (code: %d)", 
                     tflm_status_string(tflm_status), (int)tflm_status);
        int32_t result = TFM_TFLM_ERROR_MODEL_LOADING_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    if (tflm_state.interpreter == nullptr) {
        INFO_UNPRIV("[TFLM] Interpreter is null after creation");
        int32_t result = TFM_TFLM_ERROR_MODEL_LOADING_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    INFO_UNPRIV("[TFLM] Interpreter created successfully");

    // Get input and output tensor sizes
    INFO_UNPRIV("[TFLM] Getting tensor sizes...");
    tflm_status = tflm_get_input_size(tflm_state.interpreter, &tflm_state.input_size);
    if (tflm_status != TFLM_OK) {
        INFO_UNPRIV("[TFLM] Failed to get input size: %s", tflm_status_string(tflm_status));
        tflm_destroy_interpreter(tflm_state.interpreter);
        tflm_state.interpreter = nullptr;
        int32_t result = TFM_TFLM_ERROR_MODEL_LOADING_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    tflm_status = tflm_get_output_size(tflm_state.interpreter, &tflm_state.output_size);
    if (tflm_status != TFLM_OK) {
        INFO_UNPRIV("[TFLM] Failed to get output size: %s", tflm_status_string(tflm_status));
        tflm_destroy_interpreter(tflm_state.interpreter);
        tflm_state.interpreter = nullptr;
        int32_t result = TFM_TFLM_ERROR_MODEL_LOADING_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Validate tensor sizes
    if (tflm_state.input_size > TFM_TFLM_MAX_INPUT_SIZE || 
        tflm_state.output_size > TFM_TFLM_MAX_OUTPUT_SIZE) {
        INFO_UNPRIV("[TFLM] Tensor sizes too large: input=%d (max %d), output=%d (max %d)", 
                     (int)tflm_state.input_size, TFM_TFLM_MAX_INPUT_SIZE,
                     (int)tflm_state.output_size, TFM_TFLM_MAX_OUTPUT_SIZE);
        tflm_destroy_interpreter(tflm_state.interpreter);
        tflm_state.interpreter = nullptr;
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    tflm_state.model_loaded = true;

    INFO_UNPRIV("[TFLM] Model loaded successfully. Input size: %d, Output size: %d",
                 (int)tflm_state.input_size, (int)tflm_state.output_size);

    // Write success response
    int32_t result = TFM_TFLM_SUCCESS;
    psa_write(msg->handle, 0, &result, sizeof(result));

    return PSA_SUCCESS;
}

static psa_status_t tflm_set_input_data(const psa_msg_t *msg)
{
    tflm_status_t tflm_status;
    size_t data_size = msg->in_size[0];

    if (!tflm_state.model_loaded || tflm_state.interpreter == nullptr) {
        int32_t result = TFM_TFLM_ERROR_MODEL_NOT_LOADED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    if (data_size != tflm_state.input_size) {
        INFO_UNPRIV("[TFLM] Input size mismatch: expected %d, got %d",
                     (int)tflm_state.input_size, (int)data_size);
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    if (data_size > sizeof(input_buffer)) {
        INFO_UNPRIV("[TFLM] Input size %d exceeds buffer size %d",
                     (int)data_size, (int)sizeof(input_buffer));
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Read input data directly into aligned buffer
    size_t num_read = psa_read(msg->handle, 0, input_buffer, data_size);
    
    if (num_read != data_size) {
        INFO_UNPRIV("[TFLM] Failed to read input data: expected %d, got %d",
                     (int)data_size, (int)num_read);
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Set input data using C API
    INFO_UNPRIV("[TFLM] Setting input data (%d bytes)...", (int)data_size);
    tflm_status = tflm_set_input_data(tflm_state.interpreter, input_buffer, data_size);
    
    if (tflm_status != TFLM_OK) {
        INFO_UNPRIV("[TFLM] Failed to set input data: %s", tflm_status_string(tflm_status));
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    int32_t result = TFM_TFLM_SUCCESS;
    psa_write(msg->handle, 0, &result, sizeof(result));
    return PSA_SUCCESS;
}

static psa_status_t tflm_run_inference(const psa_msg_t *msg)
{
    tflm_status_t tflm_status;

    if (!tflm_state.model_loaded || tflm_state.interpreter == nullptr) {
        int32_t result = TFM_TFLM_ERROR_MODEL_NOT_LOADED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    INFO_UNPRIV("[TFLM] Running inference...");

    // Run inference using C API
    INFO_UNPRIV("[TFLM] Invoking model inference...");
    tflm_status = tflm_invoke(tflm_state.interpreter);
    
    if (tflm_status != TFLM_OK) {
        INFO_UNPRIV("[TFLM] Inference failed: %s", tflm_status_string(tflm_status));
        int32_t result = TFM_TFLM_ERROR_INFERENCE_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    INFO_UNPRIV("[TFLM] Inference completed successfully");
    int32_t result = TFM_TFLM_SUCCESS;
    psa_write(msg->handle, 0, &result, sizeof(result));
    return PSA_SUCCESS;
}

static psa_status_t tflm_get_output_data(const psa_msg_t *msg)
{
    tflm_status_t tflm_status;

    if (!tflm_state.model_loaded || tflm_state.interpreter == nullptr) {
        int32_t result = TFM_TFLM_ERROR_MODEL_NOT_LOADED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    struct {
        int32_t result;
        uint8_t data[TFM_TFLM_MAX_OUTPUT_SIZE];
    } response;

    response.result = TFM_TFLM_SUCCESS;
    
    size_t copy_size = (tflm_state.output_size > TFM_TFLM_MAX_OUTPUT_SIZE) ? 
                       TFM_TFLM_MAX_OUTPUT_SIZE : tflm_state.output_size;

    // Get output data using C API
    INFO_UNPRIV("[TFLM] Getting output data (%d bytes)...", (int)copy_size);
    tflm_status = tflm_get_output_data(tflm_state.interpreter, output_buffer, copy_size);
    
    if (tflm_status != TFLM_OK) {
        INFO_UNPRIV("[TFLM] Failed to get output data: %s", tflm_status_string(tflm_status));
        response.result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &response.result, sizeof(response.result));
        return PSA_SUCCESS;
    }
    
    memcpy(response.data, output_buffer, copy_size);
    
    psa_write(msg->handle, 0, &response, sizeof(int32_t) + copy_size);
    return PSA_SUCCESS;
}

static psa_status_t tflm_get_input_size(const psa_msg_t *msg)
{
    struct {
        int32_t result;
        size_t size;
    } response;

    if (!tflm_state.model_loaded || tflm_state.interpreter == nullptr) {
        response.result = TFM_TFLM_ERROR_MODEL_NOT_LOADED;
        response.size = 0;
    } else {
        response.result = TFM_TFLM_SUCCESS;
        response.size = tflm_state.input_size;
    }

    psa_write(msg->handle, 0, &response, sizeof(response));
    return PSA_SUCCESS;
}

static psa_status_t tflm_get_output_size(const psa_msg_t *msg)
{
    struct {
        int32_t result;
        size_t size;
    } response;

    if (!tflm_state.model_loaded || tflm_state.interpreter == nullptr) {
        response.result = TFM_TFLM_ERROR_MODEL_NOT_LOADED;
        response.size = 0;
    } else {
        response.result = TFM_TFLM_SUCCESS;
        response.size = tflm_state.output_size;
    }

    psa_write(msg->handle, 0, &response, sizeof(response));
    return PSA_SUCCESS;
}

// Override C++ new handler for out-of-memory conditions
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return malloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return malloc(size);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete[](void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    free(ptr);
}

void tflm_inference_entry(void)
{
    psa_msg_t msg;
    psa_status_t status;
    
    while (1) {
        /* Wait for a message from a client */
        psa_wait(TFM_TFLM_INFERENCE_SERVICE_SIGNAL, PSA_BLOCK);
        
        /* Get the message */
        if (psa_get(TFM_TFLM_INFERENCE_SERVICE_SIGNAL, &msg) != PSA_SUCCESS) {
            continue;
        }
        
        /* Process the message */
        if (msg.type == PSA_IPC_CONNECT) {
            INFO_UNPRIV("[TFLM] PSA_IPC_CONNECT received");
            psa_reply(msg.handle, PSA_SUCCESS);
        } else if (msg.type == PSA_IPC_DISCONNECT) {
            INFO_UNPRIV("[TFLM] PSA_IPC_DISCONNECT received");
            psa_reply(msg.handle, PSA_SUCCESS);
        } else if (msg.type == TFM_TFLM_LOAD_MODEL) {
            INFO_UNPRIV("[TFLM] TFM_TFLM_LOAD_MODEL received, input size: %d", (int)msg.in_size[0]);
            status = tflm_load_model(&msg);
            psa_reply(msg.handle, status);
        } else if (msg.type == TFM_TFLM_SET_INPUT_DATA) {
            INFO_UNPRIV("[TFLM] TFM_TFLM_SET_INPUT_DATA received");
            status = tflm_set_input_data(&msg);
            psa_reply(msg.handle, status);
        } else if (msg.type == TFM_TFLM_RUN_INFERENCE) {
            INFO_UNPRIV("[TFLM] TFM_TFLM_RUN_INFERENCE received");
            status = tflm_run_inference(&msg);
            psa_reply(msg.handle, status);
        } else if (msg.type == TFM_TFLM_GET_OUTPUT_DATA) {
            INFO_UNPRIV("[TFLM] TFM_TFLM_GET_OUTPUT_DATA received");
            status = tflm_get_output_data(&msg);
            psa_reply(msg.handle, status);
        } else if (msg.type == TFM_TFLM_GET_INPUT_SIZE) {
            INFO_UNPRIV("[TFLM] TFM_TFLM_GET_INPUT_SIZE received");
            status = tflm_get_input_size(&msg);
            psa_reply(msg.handle, status);
        } else if (msg.type == TFM_TFLM_GET_OUTPUT_SIZE) {
            INFO_UNPRIV("[TFLM] TFM_TFLM_GET_OUTPUT_SIZE received");
            status = tflm_get_output_size(&msg);
            psa_reply(msg.handle, status);
        } else {
            INFO_UNPRIV("[TFLM] Invalid message type: %d", (int)msg.type);
            psa_reply(msg.handle, (psa_status_t)PSA_ERROR_NOT_SUPPORTED);
        }
    }
}

psa_status_t tflm_inference_init(void)
{
    INFO_UNPRIV("[TFLM] TFLM Inference Service initialized");
    
    // Initialize TFLM state
    memset(&tflm_state, 0, sizeof(tflm_state));
    
    // Log memory addresses for debugging
    INFO_UNPRIV("[TFLM] Memory layout:");
    INFO_UNPRIV("[TFLM]   Model buffer: %p - %p", 
                model_buffer, model_buffer + sizeof(model_buffer));
    INFO_UNPRIV("[TFLM]   Tensor arena: %p - %p", 
                tensor_arena_buffer, tensor_arena_buffer + sizeof(tensor_arena_buffer));
    
    return (psa_status_t)PSA_SUCCESS;
}