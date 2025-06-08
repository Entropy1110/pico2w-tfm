#include "tfm_cmsis_nn_inference_defs.h"
#include "psa/client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static psa_handle_t cmsis_nn_handle = PSA_NULL_HANDLE;

static psa_status_t cmsis_nn_connect(void)
{
    if (cmsis_nn_handle != PSA_NULL_HANDLE) {
        return PSA_SUCCESS;
    }
    
    printf("[CMSIS-NN API] Attempting to connect to service SID: 0x%08x\n", TFM_CMSIS_NN_INFERENCE_SERVICE_SID);
    cmsis_nn_handle = psa_connect(TFM_CMSIS_NN_INFERENCE_SERVICE_SID, TFM_CMSIS_NN_INFERENCE_SERVICE_VERSION);
    printf("[CMSIS-NN API] psa_connect returned handle: %d\n", (int)cmsis_nn_handle);
    
    if (cmsis_nn_handle <= 0) {
        printf("[CMSIS-NN API] Connection failed with handle: %d\n", (int)cmsis_nn_handle);
        return PSA_ERROR_CONNECTION_REFUSED;
    }
    
    printf("[CMSIS-NN API] Successfully connected\n");
    return PSA_SUCCESS;
}

static void cmsis_nn_disconnect(void)
{
    if (cmsis_nn_handle != PSA_NULL_HANDLE) {
        psa_close(cmsis_nn_handle);
        cmsis_nn_handle = PSA_NULL_HANDLE;
    }
}

psa_status_t tfm_cmsis_nn_load_model(const uint8_t* model_data, size_t model_size)
{
    psa_status_t status;
    int32_t result;
    
    printf("[CMSIS-NN API] tfm_cmsis_nn_load_model called with size: %d\n", (int)model_size);
    
    status = cmsis_nn_connect();
    if (status != PSA_SUCCESS) {
        printf("[CMSIS-NN API] Connection failed: %d\n", (int)status);
        return status;
    }
    
    printf("[CMSIS-NN API] Calling psa_call with function type: %d\n", TFM_CMSIS_NN_LOAD_MODEL);
    
    psa_invec in_vec = {model_data, model_size};
    psa_outvec out_vec = {&result, sizeof(result)};
    
    status = psa_call(cmsis_nn_handle, TFM_CMSIS_NN_LOAD_MODEL, &in_vec, 1, &out_vec, 1);
    
    printf("[CMSIS-NN API] psa_call returned status: %d, result: %d\n", (int)status, result);
    
    if (status != PSA_SUCCESS) {
        printf("[CMSIS-NN API] psa_call failed with status: %d\n", (int)status);
        return status;
    }
    
    return (result == TFM_CMSIS_NN_SUCCESS) ? PSA_SUCCESS : PSA_ERROR_GENERIC_ERROR;
}

psa_status_t tfm_cmsis_nn_set_input_data(const uint8_t* input_data, size_t input_size)
{
    psa_status_t status;
    int32_t result;
    
    status = cmsis_nn_connect();
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    psa_invec in_vec = {input_data, input_size};
    psa_outvec out_vec = {&result, sizeof(result)};
    
    status = psa_call(cmsis_nn_handle, TFM_CMSIS_NN_SET_INPUT_DATA, &in_vec, 1, &out_vec, 1);
    
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    return (result == TFM_CMSIS_NN_SUCCESS) ? PSA_SUCCESS : PSA_ERROR_GENERIC_ERROR;
}

psa_status_t tfm_cmsis_nn_run_inference(void)
{
    psa_status_t status;
    int32_t result;
    
    status = cmsis_nn_connect();
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    psa_invec in_vec = {NULL, 0};
    psa_outvec out_vec = {&result, sizeof(result)};
    
    status = psa_call(cmsis_nn_handle, TFM_CMSIS_NN_RUN_INFERENCE, &in_vec, 0, &out_vec, 1);
    
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    return (result == TFM_CMSIS_NN_SUCCESS) ? PSA_SUCCESS : PSA_ERROR_GENERIC_ERROR;
}

psa_status_t tfm_cmsis_nn_get_output_data(uint8_t* output_data, size_t output_size, size_t* actual_size)
{
    psa_status_t status;
    
    status = cmsis_nn_connect();
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    // Create response buffer for result + data
    size_t response_size = sizeof(int32_t) + output_size;
    uint8_t* response_buffer = malloc(response_size);
    if (!response_buffer) {
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }
    
    psa_invec in_vec = {NULL, 0};
    psa_outvec out_vec = {response_buffer, response_size};
    
    status = psa_call(cmsis_nn_handle, TFM_CMSIS_NN_GET_OUTPUT_DATA, &in_vec, 0, &out_vec, 1);
    
    if (status == PSA_SUCCESS) {
        int32_t result = *(int32_t*)response_buffer;
        if (result == TFM_CMSIS_NN_SUCCESS) {
            size_t data_size = out_vec.len - sizeof(int32_t);
            if (data_size > 0 && output_data) {
                memcpy(output_data, response_buffer + sizeof(int32_t), 
                       (data_size < output_size) ? data_size : output_size);
            }
            if (actual_size) {
                *actual_size = data_size;
            }
            status = PSA_SUCCESS;
        } else {
            status = PSA_ERROR_GENERIC_ERROR;
        }
    }
    
    free(response_buffer);
    return status;
}

psa_status_t tfm_cmsis_nn_get_input_size(size_t* input_size)
{
    psa_status_t status;
    struct {
        int32_t result;
        size_t size;
    } response;
    
    status = cmsis_nn_connect();
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    psa_invec in_vec = {NULL, 0};
    psa_outvec out_vec = {&response, sizeof(response)};
    
    status = psa_call(cmsis_nn_handle, TFM_CMSIS_NN_GET_INPUT_SIZE, &in_vec, 0, &out_vec, 1);
    if (status == PSA_SUCCESS && input_size) {
        if (response.result == TFM_CMSIS_NN_SUCCESS) {
            *input_size = response.size;
        } else {
            status = PSA_ERROR_GENERIC_ERROR;
        }
    }
    
    return status;
}

psa_status_t tfm_cmsis_nn_get_output_size(size_t* output_size)
{
    psa_status_t status;
    struct {
        int32_t result;
        size_t size;
    } response;
    
    status = cmsis_nn_connect();
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    psa_invec in_vec = {NULL, 0};
    psa_outvec out_vec = {&response, sizeof(response)};
    
    status = psa_call(cmsis_nn_handle, TFM_CMSIS_NN_GET_OUTPUT_SIZE, &in_vec, 0, &out_vec, 1);
    if (status == PSA_SUCCESS && output_size) {
        if (response.result == TFM_CMSIS_NN_SUCCESS) {
            *output_size = response.size;
        } else {
            status = PSA_ERROR_GENERIC_ERROR;
        }
    }
    
    return status;
}

void tfm_cmsis_nn_cleanup(void)
{
    cmsis_nn_disconnect();
}