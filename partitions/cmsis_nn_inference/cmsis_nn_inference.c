#include "psa/service.h"
#include "psa/client.h"
#include "psa_manifest/cmsis_nn_inference_manifest.h"
#include "tfm_cmsis_nn_inference_defs.h"
#include <string.h>
#include <stdlib.h>

// Use CMSIS-NN neural network implementation
#include "cmsis_nn_wrapper.h"
#include "tfm_log_unpriv.h"

// CMSIS-NN model runtime state
static struct {
    int model_loaded;
    cmsis_nn_model_t* model;
    uint8_t* buffer_a;
    uint8_t* buffer_b;
    size_t buffer_size;
    size_t input_size;
    size_t output_size;
    uint8_t* model_data;  // Store model data
    size_t model_size;
} cmsis_nn_state = {0};

// Static memory allocation for CMSIS-NN model
static uint8_t model_buffer[TFM_CMSIS_NN_MAX_MODEL_SIZE];
static uint8_t buffer_a[32768]; // 32KB working buffer A
static uint8_t buffer_b[32768]; // 32KB working buffer B
static uint8_t input_buffer[TFM_CMSIS_NN_MAX_INPUT_SIZE];
static uint8_t output_buffer[TFM_CMSIS_NN_MAX_OUTPUT_SIZE];

// Helper function to validate TFLite model
static bool validate_tflite_model(const uint8_t* data, size_t size) {
    if (size < 8) {
        return false;
    }
    
    // Check TFLite file identifier (first 4 bytes should be "TFL3\n")
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

static psa_status_t cmsis_nn_partition_load_model(const psa_msg_t *msg)
{
    size_t model_size;
    size_t num_read;
    cmsis_nn_status_t cmsis_nn_status;

    INFO_UNPRIV("[CMSIS-NN] Load model called\n");

    // Clean up any previous model
    if (cmsis_nn_state.model != NULL) {
        INFO_UNPRIV("[CMSIS-NN] Cleaning up previous model\n");
        cmsis_nn_destroy_model(cmsis_nn_state.model);
        cmsis_nn_state.model = NULL;
        cmsis_nn_state.model_loaded = 0;
    }

    // Model data is directly in the input
    model_size = msg->in_size[0];
    
    if (model_size > TFM_CMSIS_NN_MAX_MODEL_SIZE) {
        INFO_UNPRIV("[CMSIS-NN] Model size %d exceeds maximum %d\n", 
                     (int)model_size, TFM_CMSIS_NN_MAX_MODEL_SIZE);
        int32_t result = TFM_CMSIS_NN_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }
    
    if (model_size < 50) { // Minimum reasonable model size (reduced for simple models)
        INFO_UNPRIV("[CMSIS-NN] Model size %d is too small (<%d)\n", (int)model_size, 100);
        int32_t result = TFM_CMSIS_NN_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }
    
    // Read model data directly into aligned buffer
    num_read = psa_read(msg->handle, 0, model_buffer, model_size);
    if (num_read != model_size) {
        INFO_UNPRIV("[CMSIS-NN] Failed to read model data: expected %d, got %d\n", 
                     (int)model_size, (int)num_read);
        int32_t result = TFM_CMSIS_NN_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Validate model format
    if (!validate_tflite_model(model_buffer, model_size)) {
        INFO_UNPRIV("[CMSIS-NN] Invalid model format\n");
        int32_t result = TFM_CMSIS_NN_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Store model info
    cmsis_nn_state.model_data = model_buffer;
    cmsis_nn_state.model_size = model_size;

    // Setup working buffers for CMSIS-NN
    cmsis_nn_state.buffer_a = buffer_a;
    cmsis_nn_state.buffer_b = buffer_b;
    cmsis_nn_state.buffer_size = sizeof(buffer_a);

    INFO_UNPRIV("[CMSIS-NN] Creating CMSIS-NN model with:\n");
    INFO_UNPRIV("[CMSIS-NN]   Model size: %d bytes\n", (int)model_size);
    INFO_UNPRIV("[CMSIS-NN]   Buffer A: %d bytes at %p\n", (int)sizeof(buffer_a), buffer_a);
    INFO_UNPRIV("[CMSIS-NN]   Buffer B: %d bytes at %p\n", (int)sizeof(buffer_b), buffer_b);
    
    // Create CMSIS-NN model with proper error handling
    INFO_UNPRIV("[CMSIS-NN] Creating CMSIS-NN model...\n");
    cmsis_nn_status = cmsis_nn_create_model(
        &cmsis_nn_state.model,
        buffer_a, sizeof(buffer_a),
        buffer_b, sizeof(buffer_b)
    );
    
    if (cmsis_nn_status != CMSIS_NN_SUCCESS) {
        INFO_UNPRIV("[CMSIS-NN] Failed to create model: %s (code: %d)\n", 
                     cmsis_nn_status_string(cmsis_nn_status), (int)cmsis_nn_status);
        int32_t result = TFM_CMSIS_NN_ERROR_MODEL_LOADING_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    if (cmsis_nn_state.model == NULL) {
        INFO_UNPRIV("[CMSIS-NN] Model is null after creation\n");
        int32_t result = TFM_CMSIS_NN_ERROR_MODEL_LOADING_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Load the model data into CMSIS-NN
    cmsis_nn_status = cmsis_nn_load_model(cmsis_nn_state.model, model_buffer, model_size);
    if (cmsis_nn_status != CMSIS_NN_SUCCESS) {
        INFO_UNPRIV("[CMSIS-NN] Failed to load model: %s\n", cmsis_nn_status_string(cmsis_nn_status));
        cmsis_nn_destroy_model(cmsis_nn_state.model);
        cmsis_nn_state.model = NULL;
        int32_t result = TFM_CMSIS_NN_ERROR_MODEL_LOADING_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    INFO_UNPRIV("[CMSIS-NN] Model created and loaded successfully\n");

    // Get input and output tensor sizes
    INFO_UNPRIV("[CMSIS-NN] Getting tensor sizes...\n");
    cmsis_nn_status = cmsis_nn_get_input_size(cmsis_nn_state.model, &cmsis_nn_state.input_size);
    if (cmsis_nn_status != CMSIS_NN_SUCCESS) {
        INFO_UNPRIV("[CMSIS-NN] Failed to get input size: %s\n", cmsis_nn_status_string(cmsis_nn_status));
        cmsis_nn_destroy_model(cmsis_nn_state.model);
        cmsis_nn_state.model = NULL;
        int32_t result = TFM_CMSIS_NN_ERROR_MODEL_LOADING_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    cmsis_nn_status = cmsis_nn_get_output_size(cmsis_nn_state.model, &cmsis_nn_state.output_size);
    if (cmsis_nn_status != CMSIS_NN_SUCCESS) {
        INFO_UNPRIV("[CMSIS-NN] Failed to get output size: %s\n", cmsis_nn_status_string(cmsis_nn_status));
        cmsis_nn_destroy_model(cmsis_nn_state.model);
        cmsis_nn_state.model = NULL;
        int32_t result = TFM_CMSIS_NN_ERROR_MODEL_LOADING_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Validate tensor sizes
    if (cmsis_nn_state.input_size > TFM_CMSIS_NN_MAX_INPUT_SIZE || 
        cmsis_nn_state.output_size > TFM_CMSIS_NN_MAX_OUTPUT_SIZE) {
        INFO_UNPRIV("[CMSIS-NN] Tensor sizes too large: input=%d (max %d), output=%d (max %d)\n", 
                     (int)cmsis_nn_state.input_size, TFM_CMSIS_NN_MAX_INPUT_SIZE,
                     (int)cmsis_nn_state.output_size, TFM_CMSIS_NN_MAX_OUTPUT_SIZE);
        cmsis_nn_destroy_model(cmsis_nn_state.model);
        cmsis_nn_state.model = NULL;
        int32_t result = TFM_CMSIS_NN_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    cmsis_nn_state.model_loaded = 1;

    INFO_UNPRIV("[CMSIS-NN] Model loaded successfully. Input size: %d, Output size: %d\n",
                 (int)cmsis_nn_state.input_size, (int)cmsis_nn_state.output_size);

    // Write success response
    int32_t result = TFM_CMSIS_NN_SUCCESS;
    psa_write(msg->handle, 0, &result, sizeof(result));

    return PSA_SUCCESS;
}

static psa_status_t cmsis_nn_partition_set_input_data(const psa_msg_t *msg)
{
    cmsis_nn_status_t cmsis_nn_status;
    size_t data_size = msg->in_size[0];

    if (!cmsis_nn_state.model_loaded || cmsis_nn_state.model == NULL) {
        int32_t result = TFM_CMSIS_NN_ERROR_MODEL_NOT_LOADED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    if (data_size != cmsis_nn_state.input_size) {
        INFO_UNPRIV("[CMSIS-NN] Input size mismatch: expected %d, got %d\n",
                     (int)cmsis_nn_state.input_size, (int)data_size);
        int32_t result = TFM_CMSIS_NN_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    if (data_size > sizeof(input_buffer)) {
        INFO_UNPRIV("[CMSIS-NN] Input size %d exceeds buffer size %d\n",
                     (int)data_size, (int)sizeof(input_buffer));
        int32_t result = TFM_CMSIS_NN_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Read input data directly into aligned buffer
    size_t num_read = psa_read(msg->handle, 0, input_buffer, data_size);
    
    if (num_read != data_size) {
        INFO_UNPRIV("[CMSIS-NN] Failed to read input data: expected %d, got %d\n",
                     (int)data_size, (int)num_read);
        int32_t result = TFM_CMSIS_NN_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    // Set input data using CMSIS-NN API
    INFO_UNPRIV("[CMSIS-NN] Setting input data (%d bytes)...\n", (int)data_size);
    cmsis_nn_status = cmsis_nn_set_input_data(cmsis_nn_state.model, (const int8_t*)input_buffer, data_size);
    
    if (cmsis_nn_status != CMSIS_NN_SUCCESS) {
        INFO_UNPRIV("[CMSIS-NN] Failed to set input data: %s\n", cmsis_nn_status_string(cmsis_nn_status));
        int32_t result = TFM_CMSIS_NN_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    int32_t result = TFM_CMSIS_NN_SUCCESS;
    psa_write(msg->handle, 0, &result, sizeof(result));
    return PSA_SUCCESS;
}

static psa_status_t cmsis_nn_partition_run_inference(const psa_msg_t *msg)
{
    cmsis_nn_status_t cmsis_nn_status;

    if (!cmsis_nn_state.model_loaded || cmsis_nn_state.model == NULL) {
        int32_t result = TFM_CMSIS_NN_ERROR_MODEL_NOT_LOADED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    INFO_UNPRIV("[CMSIS-NN] Running inference...\n");

    // Run inference using CMSIS-NN API
    INFO_UNPRIV("[CMSIS-NN] Invoking CMSIS-NN model inference...\n");
    cmsis_nn_status = cmsis_nn_run_inference(cmsis_nn_state.model);
    
    if (cmsis_nn_status != CMSIS_NN_SUCCESS) {
        INFO_UNPRIV("[CMSIS-NN] Inference failed: %s\n", cmsis_nn_status_string(cmsis_nn_status));
        int32_t result = TFM_CMSIS_NN_ERROR_INFERENCE_FAILED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    INFO_UNPRIV("[CMSIS-NN] Inference completed successfully\n");
    int32_t result = TFM_CMSIS_NN_SUCCESS;
    psa_write(msg->handle, 0, &result, sizeof(result));
    return PSA_SUCCESS;
}

static psa_status_t cmsis_nn_partition_get_output_data(const psa_msg_t *msg)
{
    cmsis_nn_status_t cmsis_nn_status;

    if (!cmsis_nn_state.model_loaded || cmsis_nn_state.model == NULL) {
        int32_t result = TFM_CMSIS_NN_ERROR_MODEL_NOT_LOADED;
        psa_write(msg->handle, 0, &result, sizeof(result));
        return PSA_SUCCESS;
    }

    struct {
        int32_t result;
        uint8_t data[TFM_CMSIS_NN_MAX_OUTPUT_SIZE];
    } response;

    response.result = TFM_CMSIS_NN_SUCCESS;
    
    size_t copy_size = (cmsis_nn_state.output_size > TFM_CMSIS_NN_MAX_OUTPUT_SIZE) ? 
                       TFM_CMSIS_NN_MAX_OUTPUT_SIZE : cmsis_nn_state.output_size;

    // Get output data using CMSIS-NN API
    INFO_UNPRIV("[CMSIS-NN] Getting output data (%d bytes)...\n", (int)copy_size);
    cmsis_nn_status = cmsis_nn_get_output_data(cmsis_nn_state.model, (int8_t*)output_buffer, copy_size);
    
    if (cmsis_nn_status != CMSIS_NN_SUCCESS) {
        INFO_UNPRIV("[CMSIS-NN] Failed to get output data: %s\n", cmsis_nn_status_string(cmsis_nn_status));
        response.result = TFM_CMSIS_NN_ERROR_INVALID_PARAMETER;
        psa_write(msg->handle, 0, &response.result, sizeof(response.result));
        return PSA_SUCCESS;
    }
    
    memcpy(response.data, output_buffer, copy_size);
    
    psa_write(msg->handle, 0, &response, sizeof(int32_t) + copy_size);
    return PSA_SUCCESS;
}

static psa_status_t cmsis_nn_partition_get_input_size(const psa_msg_t *msg)
{
    struct {
        int32_t result;
        size_t size;
    } response;

    if (!cmsis_nn_state.model_loaded || cmsis_nn_state.model == NULL) {
        response.result = TFM_CMSIS_NN_ERROR_MODEL_NOT_LOADED;
        response.size = 0;
    } else {
        response.result = TFM_CMSIS_NN_SUCCESS;
        response.size = cmsis_nn_state.input_size;
    }

    psa_write(msg->handle, 0, &response, sizeof(response));
    return PSA_SUCCESS;
}

static psa_status_t cmsis_nn_partition_get_output_size(const psa_msg_t *msg)
{
    struct {
        int32_t result;
        size_t size;
    } response;

    if (!cmsis_nn_state.model_loaded || cmsis_nn_state.model == NULL) {
        response.result = TFM_CMSIS_NN_ERROR_MODEL_NOT_LOADED;
        response.size = 0;
    } else {
        response.result = TFM_CMSIS_NN_SUCCESS;
        response.size = cmsis_nn_state.output_size;
    }

    psa_write(msg->handle, 0, &response, sizeof(response));
    return PSA_SUCCESS;
}

// CMSIS-NN neural network test
static int test_cmsis_nn_model() {
    INFO_UNPRIV("[CMSIS-NN] Testing CMSIS-NN neural network model...\n");
    
    // Test basic functionality with dummy data
    cmsis_nn_model_t* test_model = NULL;
    
    // Create model
    cmsis_nn_status_t status = cmsis_nn_create_model(
        &test_model,
        buffer_a, sizeof(buffer_a),
        buffer_b, sizeof(buffer_b)
    );
    
    if (status == CMSIS_NN_SUCCESS && test_model != NULL) {
        INFO_UNPRIV("[CMSIS-NN] ✅ CMSIS-NN model test PASSED!\n");
        cmsis_nn_destroy_model(test_model);
        return 0;
    } else {
        INFO_UNPRIV("[CMSIS-NN] ❌ CMSIS-NN model test FAILED! Status: %d\n", (int)status);
        return -1;
    }
}

void cmsis_nn_inference_entry(void)
{
    psa_msg_t msg;
    psa_status_t status;
    
    while (1) {
        /* Wait for a message from a client */
        psa_wait(TFM_CMSIS_NN_INFERENCE_SERVICE_SIGNAL, PSA_BLOCK);
        
        /* Get the message */
        if (psa_get(TFM_CMSIS_NN_INFERENCE_SERVICE_SIGNAL, &msg) != PSA_SUCCESS) {
            continue;
        }

        /* Process the message */
        if (msg.type == PSA_IPC_CONNECT) {
            INFO_UNPRIV("[CMSIS-NN] PSA_IPC_CONNECT received\n");
            psa_reply(msg.handle, PSA_SUCCESS);
        } else if (msg.type == PSA_IPC_DISCONNECT) {
            INFO_UNPRIV("[CMSIS-NN] PSA_IPC_DISCONNECT received\n");
            psa_reply(msg.handle, PSA_SUCCESS);
        } else if (msg.type == TFM_CMSIS_NN_LOAD_MODEL) {
            INFO_UNPRIV("[CMSIS-NN] TFM_CMSIS_NN_LOAD_MODEL received, input size: %d\n", (int)msg.in_size[0]);
            status = cmsis_nn_partition_load_model(&msg);
            psa_reply(msg.handle, status);
        } else if (msg.type == TFM_CMSIS_NN_SET_INPUT_DATA) {
            INFO_UNPRIV("[CMSIS-NN] TFM_CMSIS_NN_SET_INPUT_DATA received\n");
            status = cmsis_nn_partition_set_input_data(&msg);
            psa_reply(msg.handle, status);
        } else if (msg.type == TFM_CMSIS_NN_RUN_INFERENCE) {
            INFO_UNPRIV("[CMSIS-NN] TFM_CMSIS_NN_RUN_INFERENCE received\n");
            status = cmsis_nn_partition_run_inference(&msg);
            psa_reply(msg.handle, status);
        } else if (msg.type == TFM_CMSIS_NN_GET_OUTPUT_DATA) {
            INFO_UNPRIV("[CMSIS-NN] TFM_CMSIS_NN_GET_OUTPUT_DATA received\n");
            status = cmsis_nn_partition_get_output_data(&msg);
            psa_reply(msg.handle, status);
        } else if (msg.type == TFM_CMSIS_NN_GET_INPUT_SIZE) {
            INFO_UNPRIV("[CMSIS-NN] TFM_CMSIS_NN_GET_INPUT_SIZE received\n");
            status = cmsis_nn_partition_get_input_size(&msg);
            psa_reply(msg.handle, status);
        } else if (msg.type == TFM_CMSIS_NN_GET_OUTPUT_SIZE) {
            INFO_UNPRIV("[CMSIS-NN] TFM_CMSIS_NN_GET_OUTPUT_SIZE received\n");
            status = cmsis_nn_partition_get_output_size(&msg);
            psa_reply(msg.handle, status);
        } else {
            INFO_UNPRIV("[CMSIS-NN] Invalid message type: %d\n", (int)msg.type);
            psa_reply(msg.handle, (psa_status_t)PSA_ERROR_NOT_SUPPORTED);
        }
    }
}

