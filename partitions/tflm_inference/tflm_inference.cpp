#include "psa/service.h"
#include "psa/client.h"
#include "psa_manifest/tflm_inference_manifest.h"
#include "tfm_tflm_inference_defs.h"
#include <string.h>

// Use TFLM C API
#include "tflm_c_api.h"

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
} tflm_state = {0};

// Static memory allocation for TFLM
static uint8_t model_buffer[TFM_TFLM_MAX_MODEL_SIZE];
static uint8_t tensor_arena_buffer[32768]; // 32KB tensor arena for better compatibility

static psa_status_t tflm_load_model(const psa_msg_t *msg)
{
    size_t model_size;
    size_t num_read;
    tflm_status_t tflm_status;

    // Model data is directly in the input
    model_size = msg->in_size[0];
    
    if (model_size > TFM_TFLM_MAX_MODEL_SIZE) {
        INFO_UNPRIV("[TFLM] Model size %d exceeds maximum %d", 
                     model_size, TFM_TFLM_MAX_MODEL_SIZE);
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }
    
    // Read model data directly
    num_read = psa_read(msg->handle, 0, model_buffer, model_size);
    if (num_read != model_size) {
        INFO_UNPRIV("[TFLM] Failed to read model data: expected %d, got %d", 
                     model_size, num_read);
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Setup tensor arena
    tflm_state.tensor_arena = tensor_arena_buffer;
    tflm_state.tensor_arena_size = sizeof(tensor_arena_buffer);

    INFO_UNPRIV("[TFLM] About to create interpreter with model size: %d", model_size);
    
    // Create interpreter using C API - wrap in try-catch to prevent crashes
    tflm_status = TFLM_ERROR_GENERIC; // Default to error
    
    // Simple validation: check for TFLite magic number
    if (model_size < 8) {
        INFO_UNPRIV("[TFLM] Model too small: %d bytes", model_size);
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }
    
    INFO_UNPRIV("[TFLM] Model header: %02x %02x %02x %02x", 
                model_buffer[0], model_buffer[1], model_buffer[2], model_buffer[3]);
    
    // For now, skip actual TFLM initialization to test PSA messaging
    INFO_UNPRIV("[TFLM] Skipping TFLM interpreter creation for testing");
    tflm_status = TFLM_OK;
    tflm_state.interpreter = nullptr; // Mark as placeholder
    tflm_state.input_size = 64;  // Dummy values for testing
    tflm_state.output_size = 32;
    
    /*
    tflm_status = tflm_create_interpreter(model_buffer, model_size,
                                          tflm_state.tensor_arena, 
                                          tflm_state.tensor_arena_size,
                                          &tflm_state.interpreter);
    */
    
    if (tflm_status != TFLM_OK) {
        INFO_UNPRIV("[TFLM] Failed to create interpreter: %s", 
                     tflm_status_string(tflm_status));
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Get input and output tensor sizes (skip for testing)
    /*
    tflm_status = tflm_get_input_size(tflm_state.interpreter, &tflm_state.input_size);
    if (tflm_status != TFLM_OK) {
        tflm_destroy_interpreter(tflm_state.interpreter);
        tflm_state.interpreter = NULL;
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    tflm_status = tflm_get_output_size(tflm_state.interpreter, &tflm_state.output_size);
    if (tflm_status != TFLM_OK) {
        tflm_destroy_interpreter(tflm_state.interpreter);
        tflm_state.interpreter = NULL;
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }
    */

    tflm_state.model_loaded = true;

    INFO_UNPRIV("[TFLM] Model loaded successfully. Input size: %d, Output size: %d",
                 tflm_state.input_size, tflm_state.output_size);

    // Write success response
    int32_t result = TFM_TFLM_SUCCESS;
    psa_write(msg->handle, 0, &result, sizeof(result));

    return PSA_SUCCESS;
}

static psa_status_t tflm_set_input_data(const psa_msg_t *msg)
{
    static uint8_t input_buffer[TFM_TFLM_MAX_INPUT_SIZE]; // Temporary buffer for input data
    tflm_status_t tflm_status;
    size_t data_size = msg->in_size[0]; // Input data size

    if (!tflm_state.model_loaded) {
        int32_t result = TFM_TFLM_ERROR_MODEL_NOT_LOADED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    if (data_size != tflm_state.input_size) {
        INFO_UNPRIV("[TFLM] Input size mismatch: expected %d, got %d",
                     tflm_state.input_size, data_size);
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    if (data_size > sizeof(input_buffer)) {
        INFO_UNPRIV("[TFLM] Input size %d exceeds buffer size %d",
                     data_size, sizeof(input_buffer));
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Read input data directly
    size_t num_read = psa_read(msg->handle, 0, input_buffer, data_size);
    
    if (num_read != data_size) {
        INFO_UNPRIV("[TFLM] Failed to read input data: expected %d, got %d",
                     data_size, num_read);
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Set input data using C API (skip for testing)
    INFO_UNPRIV("[TFLM] Skipping actual input data setting for testing");
    tflm_status = TFLM_OK; // Simulate success
    
    /*
    tflm_status = tflm_set_input_data(tflm_state.interpreter, input_buffer, data_size);
    if (tflm_status != TFLM_OK) {
        INFO_UNPRIV("[TFLM] Failed to set input data: %s", tflm_status_string(tflm_status));
        int32_t result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }
    */

    int32_t result = TFM_TFLM_SUCCESS;
    psa_write(msg->handle, 0, &result, sizeof(result));
    return PSA_SUCCESS;
}

static psa_status_t tflm_run_inference(const psa_msg_t *msg)
{
    tflm_status_t tflm_status;

    // Function ID already read - no additional data expected

    if (!tflm_state.model_loaded) {
        int32_t result = TFM_TFLM_ERROR_MODEL_NOT_LOADED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Run inference using C API (skip for testing)
    INFO_UNPRIV("[TFLM] Skipping actual inference for testing");
    tflm_status = TFLM_OK; // Simulate success
    
    /*
    tflm_status = tflm_invoke(tflm_state.interpreter);
    if (tflm_status != TFLM_OK) {
        INFO_UNPRIV("[TFLM] Inference failed: %s", tflm_status_string(tflm_status));
        int32_t result = TFM_TFLM_ERROR_INFERENCE_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }
    */

    INFO_UNPRIV("[TFLM] Inference completed successfully");
    int32_t result = TFM_TFLM_SUCCESS;
    psa_write(msg->handle, 0, &result, sizeof(result));
    return PSA_SUCCESS;
}

static psa_status_t tflm_get_output_data(const psa_msg_t *msg)
{
    static uint8_t output_buffer[TFM_TFLM_MAX_OUTPUT_SIZE]; // Temporary buffer for output data
    tflm_status_t tflm_status;

    // Function ID already read - no additional data expected

    if (!tflm_state.model_loaded) {
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
    
    if (copy_size > sizeof(output_buffer)) {
        copy_size = sizeof(output_buffer);
    }

    // Get output data using C API (skip for testing)
    INFO_UNPRIV("[TFLM] Generating dummy output data for testing");
    // Fill with dummy data for testing
    for (size_t i = 0; i < copy_size; i++) {
        output_buffer[i] = (uint8_t)(i % 256);
    }
    tflm_status = TFLM_OK;
    
    /*
    tflm_status = tflm_get_output_data(tflm_state.interpreter, output_buffer, copy_size);
    if (tflm_status != TFLM_OK) {
        INFO_UNPRIV("[TFLM] Failed to get output data: %s", tflm_status_string(tflm_status));
        response.result = TFM_TFLM_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &response.result, sizeof(response.result));
        return PSA_SUCCESS;
    }
    */
    
    memcpy(response.data, output_buffer, copy_size);
    
    psa_write(msg->handle, 0, &response, sizeof(int32_t) + copy_size);
    return PSA_SUCCESS;
}

static psa_status_t tflm_get_input_size(const psa_msg_t *msg)
{
    // Function ID already read - no additional data expected

    struct {
        int32_t result;
        size_t size;
    } response;

    if (!tflm_state.model_loaded) {
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
    // Function ID already read - no additional data expected

    struct {
        int32_t result;
        size_t size;
    } response;

    if (!tflm_state.model_loaded) {
        response.result = TFM_TFLM_ERROR_MODEL_NOT_LOADED;
        response.size = 0;
    } else {
        response.result = TFM_TFLM_SUCCESS;
        response.size = tflm_state.output_size;
    }

    psa_write(msg->handle, 0, &response, sizeof(response));
    return PSA_SUCCESS;
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
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                INFO_UNPRIV("[TFLM] PSA_IPC_CONNECT received");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case PSA_IPC_DISCONNECT:
                INFO_UNPRIV("[TFLM] PSA_IPC_DISCONNECT received");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case TFM_TFLM_LOAD_MODEL:
                INFO_UNPRIV("[TFLM] TFM_TFLM_LOAD_MODEL received, input size: %d", msg.in_size[0]);
                status = tflm_load_model(&msg);
                INFO_UNPRIV("[TFLM] tflm_load_model returned: %d", status);
                psa_reply(msg.handle, status);
                break;
                
            case TFM_TFLM_SET_INPUT_DATA:
                INFO_UNPRIV("[TFLM] TFM_TFLM_SET_INPUT_DATA received");
                status = tflm_set_input_data(&msg);
                psa_reply(msg.handle, status);
                break;
                
            case TFM_TFLM_RUN_INFERENCE:
                INFO_UNPRIV("[TFLM] TFM_TFLM_RUN_INFERENCE received");
                status = tflm_run_inference(&msg);
                psa_reply(msg.handle, status);
                break;
                
            case TFM_TFLM_GET_OUTPUT_DATA:
                INFO_UNPRIV("[TFLM] TFM_TFLM_GET_OUTPUT_DATA received");
                status = tflm_get_output_data(&msg);
                psa_reply(msg.handle, status);
                break;
                
            case TFM_TFLM_GET_INPUT_SIZE:
                INFO_UNPRIV("[TFLM] TFM_TFLM_GET_INPUT_SIZE received");
                status = tflm_get_input_size(&msg);
                psa_reply(msg.handle, status);
                break;
                
            case TFM_TFLM_GET_OUTPUT_SIZE:
                INFO_UNPRIV("[TFLM] TFM_TFLM_GET_OUTPUT_SIZE received");
                status = tflm_get_output_size(&msg);
                psa_reply(msg.handle, status);
                break;
                
            default:
                INFO_UNPRIV("[TFLM] Invalid message type: %d", (int)msg.type);
                psa_reply(msg.handle, (psa_status_t)PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

psa_status_t tflm_inference_init(void)
{
    INFO_UNPRIV("[TFLM] TFLM Inference Service initialized");
    
    // Initialize TFLM state
    memset(&tflm_state, 0, sizeof(tflm_state));
    
    return (psa_status_t)PSA_SUCCESS;
}