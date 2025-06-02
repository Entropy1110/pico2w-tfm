/*
 * Copyright (c) 2024, TZTZEN Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#include "psa_tflm_client.h"
#include <stdio.h>
#include <string.h>

psa_status_t psa_tflm_echo(const uint8_t *input_data, size_t input_size,
                           uint8_t *output_data, size_t output_buffer_size,
                           size_t *actual_output_size)
{
    psa_status_t status;
    psa_handle_t handle;
    uint32_t request_type = TFLM_REQUEST_TYPE_ECHO;
    
    printf("[NS] psa_tflm_echo: Starting echo request\n");
    printf("[NS] psa_tflm_echo: Input size: %zu, Output buffer size: %zu\n", input_size, output_buffer_size);
    fflush(stdout);
    
    psa_invec in_vec[] = {
        { &request_type, sizeof(request_type) },
        { input_data, input_size }
    };
    
    psa_outvec out_vec[] = {
        { output_data, output_buffer_size },
        { actual_output_size, sizeof(*actual_output_size) }
    };
    
    printf("[NS] psa_tflm_echo: About to connect (SID: 0x%08X, IPC model)\n", 
           TFLM_SECURE_SERVICE_SID);
    fflush(stdout);

    handle = psa_connect(TFLM_SECURE_SERVICE_SID, TFLM_SECURE_SERVICE_VERSION);
    if (!PSA_HANDLE_IS_VALID(handle)) {
        printf("[NS] psa_tflm_echo: psa_connect failed with handle: %ld\n", handle);
        fflush(stdout);
        return PSA_HANDLE_TO_ERROR(handle);
    }
    
    printf("[NS] psa_tflm_echo: Connected successfully, making PSA call\n");
    fflush(stdout);

    status = psa_call(handle, PSA_IPC_CALL, in_vec, 2, out_vec, 2);
    
    printf("[NS] psa_tflm_echo: PSA call completed with status: %ld\n", status);
    fflush(stdout);
    
    psa_close(handle);
    
    printf("[NS] psa_tflm_echo: Connection closed, returning status: %ld\n", status);
    fflush(stdout);
    
    return status;
}

psa_status_t psa_tflm_load_model(const uint8_t *encrypted_model_data,
                                 size_t model_size,
                                 uint32_t *model_id)
{
    psa_status_t status;
    psa_handle_t handle;
    uint32_t request_type = TFLM_REQUEST_TYPE_LOAD_MODEL;
    
    psa_invec in_vec[] = {
        { &request_type, sizeof(request_type) },
        { encrypted_model_data, model_size }
    };
    
    psa_outvec out_vec[] = {
        { model_id, sizeof(*model_id) }
    };
    
    handle = psa_connect(TFLM_SECURE_SERVICE_SID, TFLM_SECURE_SERVICE_VERSION);
    if (!PSA_HANDLE_IS_VALID(handle)) {
        return PSA_HANDLE_TO_ERROR(handle);
    }
    
    status = psa_call(handle, PSA_IPC_CALL, in_vec, 2, out_vec, 1);
    
    psa_close(handle);
    
    return status;
}

psa_status_t psa_tflm_run_inference(uint32_t model_id,
                                    const uint8_t *input_data,
                                    size_t input_size,
                                    uint8_t *output_data,
                                    size_t output_size,
                                    size_t *actual_output_size)
{
    psa_status_t status;
    psa_handle_t handle;
    uint32_t request_type = TFLM_REQUEST_TYPE_RUN_INFERENCE;
    
    tflm_inference_request_t request = {
        .model_id = model_id,
        .input_size = input_size,
        .output_size = output_size
    };
    
    psa_invec in_vec[] = {
        { &request_type, sizeof(request_type) },
        { &request, sizeof(request) },
        { input_data, input_size }
    };
    
    psa_outvec out_vec[] = {
        { output_data, output_size },
        { actual_output_size, sizeof(*actual_output_size) }
    };
    
    handle = psa_connect(TFLM_SECURE_SERVICE_SID, TFLM_SECURE_SERVICE_VERSION);
    if (!PSA_HANDLE_IS_VALID(handle)) {
        return PSA_HANDLE_TO_ERROR(handle);
    }
    
    status = psa_call(handle, PSA_IPC_CALL, in_vec, 3, out_vec, 2);
    
    psa_close(handle);
    
    return status;
}

psa_status_t psa_tflm_get_model_info(uint32_t model_id,
                                     tflm_model_info_t *model_info)
{
    psa_status_t status;
    psa_handle_t handle;
    uint32_t request_type = TFLM_REQUEST_TYPE_GET_MODEL_INFO;
    
    psa_invec in_vec[] = {
        { &request_type, sizeof(request_type) },
        { &model_id, sizeof(model_id) }
    };
    
    psa_outvec out_vec[] = {
        { model_info, sizeof(*model_info) }
    };
    
    handle = psa_connect(TFLM_SECURE_SERVICE_SID, TFLM_SECURE_SERVICE_VERSION);
    if (!PSA_HANDLE_IS_VALID(handle)) {
        return PSA_HANDLE_TO_ERROR(handle);
    }
    
    status = psa_call(handle, PSA_IPC_CALL, in_vec, 2, out_vec, 1);
    
    psa_close(handle);
    
    return status;
}

psa_status_t psa_tflm_unload_model(uint32_t model_id)
{
    psa_status_t status;
    psa_handle_t handle;
    uint32_t request_type = TFLM_REQUEST_TYPE_UNLOAD_MODEL;
    
    psa_invec in_vec[] = {
        { &request_type, sizeof(request_type) },
        { &model_id, sizeof(model_id) }
    };
    
    handle = psa_connect(TFLM_SECURE_SERVICE_SID, TFLM_SECURE_SERVICE_VERSION);
    if (!PSA_HANDLE_IS_VALID(handle)) {
        return PSA_HANDLE_TO_ERROR(handle);
    }
    
    status = psa_call(handle, PSA_IPC_CALL, in_vec, 2, NULL, 0);
    
    psa_close(handle);
    
    return status;
}
