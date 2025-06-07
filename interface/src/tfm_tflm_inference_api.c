#include "tfm_tflm_inference_defs.h"
#include "psa/client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static psa_handle_t tflm_handle = PSA_NULL_HANDLE;

static psa_status_t tflm_connect(void)
{
    if (tflm_handle != PSA_NULL_HANDLE) {
        return PSA_SUCCESS;
    }
    
    printf("[TFLM API] Attempting to connect to service SID: 0x%08x\n", TFM_TFLM_INFERENCE_SERVICE_SID);
    tflm_handle = psa_connect(TFM_TFLM_INFERENCE_SERVICE_SID, TFM_TFLM_INFERENCE_SERVICE_VERSION);
    printf("[TFLM API] psa_connect returned handle: %d\n", (int)tflm_handle);
    
    if (tflm_handle <= 0) {
        printf("[TFLM API] Connection failed with handle: %d\n", (int)tflm_handle);
        return PSA_ERROR_CONNECTION_REFUSED;
    }
    
    printf("[TFLM API] Successfully connected\n");
    return PSA_SUCCESS;
}

static void tflm_disconnect(void)
{
    if (tflm_handle != PSA_NULL_HANDLE) {
        psa_close(tflm_handle);
        tflm_handle = PSA_NULL_HANDLE;
    }
}

psa_status_t tfm_tflm_load_model(const uint8_t* model_data, size_t model_size)
{
    psa_status_t status;
    int32_t result;
    
    printf("[TFLM API] tfm_tflm_load_model called with size: %d\n", (int)model_size);
    
    status = tflm_connect();
    if (status != PSA_SUCCESS) {
        printf("[TFLM API] Connection failed: %d\n", (int)status);
        return status;
    }
    
    printf("[TFLM API] Calling psa_call with function type: %d\n", TFM_TFLM_LOAD_MODEL);
    
    psa_invec in_vec = {model_data, model_size};
    psa_outvec out_vec = {&result, sizeof(result)};
    
    status = psa_call(tflm_handle, TFM_TFLM_LOAD_MODEL, &in_vec, 1, &out_vec, 1);
    
    printf("[TFLM API] psa_call returned status: %d, result: %d\n", (int)status, result);
    
    if (status != PSA_SUCCESS) {
        printf("[TFLM API] psa_call failed with status: %d\n", (int)status);
        return status;
    }
    
    return (result == TFM_TFLM_SUCCESS) ? PSA_SUCCESS : PSA_ERROR_GENERIC_ERROR;
}

psa_status_t tfm_tflm_set_input_data(const uint8_t* input_data, size_t input_size)
{
    psa_status_t status;
    int32_t result;
    
    status = tflm_connect();
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    psa_invec in_vec = {input_data, input_size};
    psa_outvec out_vec = {&result, sizeof(result)};
    
    status = psa_call(tflm_handle, TFM_TFLM_SET_INPUT_DATA, &in_vec, 1, &out_vec, 1);
    
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    return (result == TFM_TFLM_SUCCESS) ? PSA_SUCCESS : PSA_ERROR_GENERIC_ERROR;
}

psa_status_t tfm_tflm_run_inference(void)
{
    psa_status_t status;
    int32_t result;
    
    status = tflm_connect();
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    psa_invec in_vec = {NULL, 0};
    psa_outvec out_vec = {&result, sizeof(result)};
    
    status = psa_call(tflm_handle, TFM_TFLM_RUN_INFERENCE, &in_vec, 0, &out_vec, 1);
    
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    return (result == TFM_TFLM_SUCCESS) ? PSA_SUCCESS : PSA_ERROR_GENERIC_ERROR;
}

psa_status_t tfm_tflm_get_output_data(uint8_t* output_data, size_t output_size, size_t* actual_size)
{
    psa_status_t status;
    
    status = tflm_connect();
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
    
    status = psa_call(tflm_handle, TFM_TFLM_GET_OUTPUT_DATA, &in_vec, 0, &out_vec, 1);
    
    if (status == PSA_SUCCESS) {
        int32_t result = *(int32_t*)response_buffer;
        if (result == TFM_TFLM_SUCCESS) {
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

psa_status_t tfm_tflm_get_input_size(size_t* input_size)
{
    psa_status_t status;
    struct {
        int32_t result;
        size_t size;
    } response;
    
    status = tflm_connect();
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    psa_invec in_vec = {NULL, 0};
    psa_outvec out_vec = {&response, sizeof(response)};
    
    status = psa_call(tflm_handle, TFM_TFLM_GET_INPUT_SIZE, &in_vec, 0, &out_vec, 1);
    if (status == PSA_SUCCESS && input_size) {
        if (response.result == TFM_TFLM_SUCCESS) {
            *input_size = response.size;
        } else {
            status = PSA_ERROR_GENERIC_ERROR;
        }
    }
    
    return status;
}

psa_status_t tfm_tflm_get_output_size(size_t* output_size)
{
    psa_status_t status;
    struct {
        int32_t result;
        size_t size;
    } response;
    
    status = tflm_connect();
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    psa_invec in_vec = {NULL, 0};
    psa_outvec out_vec = {&response, sizeof(response)};
    
    status = psa_call(tflm_handle, TFM_TFLM_GET_OUTPUT_SIZE, &in_vec, 0, &out_vec, 1);
    if (status == PSA_SUCCESS && output_size) {
        if (response.result == TFM_TFLM_SUCCESS) {
            *output_size = response.size;
        } else {
            status = PSA_ERROR_GENERIC_ERROR;
        }
    }
    
    return status;
}

void tfm_tflm_cleanup(void)
{
    tflm_disconnect();
}