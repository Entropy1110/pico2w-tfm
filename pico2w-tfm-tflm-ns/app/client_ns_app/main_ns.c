/*
 * Copyright (c) 2024, TZTZEN Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* TF-M NS API */
#include "psa/client.h"
#include "psa/error.h"

/* TFLM Service API */
#include "psa_tflm_service_api.h"

/* Test data */
static const uint8_t dummy_model_data[] = {
    0xEF, 0xBE, 0xAD, 0xDE,  /* Magic */
    0x01, 0x00, 0x00, 0x00,  /* Version */
    0x10, 0x00, 0x00, 0x00,  /* Input size */
    0x04, 0x00, 0x00, 0x00,  /* Output size */
    /* Additional dummy model data */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

static const uint8_t test_input_data[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
};

/* Simple delay function (TF-M compatible) */
static void simple_delay_ms(uint32_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 100000; i++) {
        __asm volatile("nop");
    }
}

/* Print hex data */
static void print_hex(const char *label, const uint8_t *data, size_t len)
{
    printf("%s: ", label);
    for (size_t i = 0; i < len && i < 16; i++) {
        printf("%02X ", data[i]);
    }
    if (len > 16) printf("... (%zu bytes total)", len);
    printf("\n");
}

/* TFLM Load Model API Implementation */
static psa_status_t psa_tflm_load_model(const uint8_t *encrypted_model_data,
                                         size_t model_size,
                                         uint32_t *model_id)
{
    psa_handle_t handle;
    psa_status_t status;
    uint32_t request_type = TFLM_REQUEST_TYPE_LOAD_MODEL;
    
    psa_invec in_vec[] = {
        { &request_type, sizeof(request_type) },
        { encrypted_model_data, model_size }
    };
    
    psa_outvec out_vec[] = {
        { model_id, sizeof(*model_id) }
    };
    
    handle = psa_connect(TFLM_SECURE_SERVICE_SID, TFLM_SECURE_SERVICE_VERSION);
    if (handle < 0) {
        printf("[NS] ERROR: Failed to connect to TFLM service: %ld\n", handle);
        return (psa_status_t)handle;
    }
    
    status = psa_call(handle, PSA_IPC_CALL, in_vec, 2, out_vec, 1);
    psa_close(handle);
    
    return status;
}

/* TFLM Run Inference API Implementation */
static psa_status_t psa_tflm_run_inference(uint32_t model_id,
                                            const uint8_t *input_data,
                                            size_t input_size,
                                            uint8_t *output_data,
                                            size_t output_size,
                                            size_t *actual_output_size)
{
    psa_handle_t handle;
    psa_status_t status;
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
    if (handle < 0) {
        printf("[NS] ERROR: Failed to connect to TFLM service: %ld\n", handle);
        return (psa_status_t)handle;
    }
    
    status = psa_call(handle, PSA_IPC_CALL, in_vec, 3, out_vec, 2);
    psa_close(handle);
    
    return status;
}

/* TFLM Get Model Info API Implementation */
static psa_status_t psa_tflm_get_model_info(uint32_t model_id,
                                             tflm_model_info_t *model_info)
{
    psa_handle_t handle;
    psa_status_t status;
    uint32_t request_type = TFLM_REQUEST_TYPE_GET_MODEL_INFO;
    
    psa_invec in_vec[] = {
        { &request_type, sizeof(request_type) },
        { &model_id, sizeof(model_id) }
    };
    
    psa_outvec out_vec[] = {
        { model_info, sizeof(*model_info) }
    };
    
    handle = psa_connect(TFLM_SECURE_SERVICE_SID, TFLM_SECURE_SERVICE_VERSION);
    if (handle < 0) {
        printf("[NS] ERROR: Failed to connect to TFLM service: %ld\n", handle);
        return (psa_status_t)handle;
    }
    
    status = psa_call(handle, PSA_IPC_CALL, in_vec, 2, out_vec, 1);
    psa_close(handle);
    
    return status;
}

/* TFLM Unload Model API Implementation */
static psa_status_t psa_tflm_unload_model(uint32_t model_id)
{
    psa_handle_t handle;
    psa_status_t status;
    uint32_t request_type = TFLM_REQUEST_TYPE_UNLOAD_MODEL;
    
    psa_invec in_vec[] = {
        { &request_type, sizeof(request_type) },
        { &model_id, sizeof(model_id) }
    };
    
    handle = psa_connect(TFLM_SECURE_SERVICE_SID, TFLM_SECURE_SERVICE_VERSION);
    if (handle < 0) {
        printf("[NS] ERROR: Failed to connect to TFLM service: %ld\n", handle);
        return (psa_status_t)handle;
    }
    
    status = psa_call(handle, PSA_IPC_CALL, in_vec, 2, NULL, 0);
    psa_close(handle);
    
    return status;
}

/* Test TFLM secure service */
static void test_tflm_service(void)
{
    psa_status_t status;
    uint32_t model_id = 0;
    tflm_model_info_t model_info;
    uint8_t output_data[64];
    size_t output_size = 0;

    printf("\n=== Testing TFLM Secure Service ===\n");
    
    /* Test 1: Load Model */
    printf("\n[NS] Test 1: Loading model...\n");
    print_hex("Model data", dummy_model_data, sizeof(dummy_model_data));
    
    status = psa_tflm_load_model(dummy_model_data, sizeof(dummy_model_data), &model_id);
    if (status != PSA_SUCCESS) {
        printf("[NS] ERROR: Failed to load model: %d\n", status);
        return;
    }
    printf("[NS] ✓ Model loaded successfully with ID: %lu\n", model_id);

    /* Test 2: Get Model Info */
    printf("\n[NS] Test 2: Getting model info...\n");
    status = psa_tflm_get_model_info(model_id, &model_info);
    if (status != PSA_SUCCESS) {
        printf("[NS] ERROR: Failed to get model info: %d\n", status);
    } else {
        printf("[NS] ✓ Model info retrieved:\n");
        printf("    - Model ID: %lu\n", model_info.model_id);
        printf("    - Input size: %lu bytes\n", model_info.input_size);
        printf("    - Output size: %lu bytes\n", model_info.output_size);
        printf("    - Version: %lu\n", model_info.model_version);
    }

    /* Test 3: Run Inference */
    printf("\n[NS] Test 3: Running inference...\n");
    print_hex("Input data", test_input_data, sizeof(test_input_data));
    
    status = psa_tflm_run_inference(model_id,
                                    test_input_data, sizeof(test_input_data),
                                    output_data, sizeof(output_data),
                                    &output_size);
    if (status != PSA_SUCCESS) {
        printf("[NS] ERROR: Failed to run inference: %d\n", status);
    } else {
        printf("[NS] ✓ Inference completed successfully\n");
        printf("[NS] Output size: %zu bytes\n", output_size);
        print_hex("Output data", output_data, output_size);
        
        /* Verify result (sum of input bytes) */
        if (output_size >= sizeof(uint32_t)) {
            uint32_t expected_sum = 0;
            uint32_t actual_sum;
            
            for (size_t i = 0; i < sizeof(test_input_data); i++) {
                expected_sum += test_input_data[i];
            }
            
            memcpy(&actual_sum, output_data, sizeof(actual_sum));
            printf("[NS] Expected sum: %lu, Actual sum: %lu\n", expected_sum, actual_sum);
            
            if (expected_sum == actual_sum) {
                printf("[NS] ✓ Inference result verified!\n");
            } else {
                printf("[NS] ✗ Inference result mismatch!\n");
            }
        }
    }

    /* Test 4: Unload Model */
    printf("\n[NS] Test 4: Unloading model...\n");
    status = psa_tflm_unload_model(model_id);
    if (status != PSA_SUCCESS) {
        printf("[NS] ERROR: Failed to unload model: %d\n", status);
    } else {
        printf("[NS] ✓ Model unloaded successfully\n");
    }
    
    printf("\n=== TFLM Service Test Complete ===\n");
}

/* Main function */
int main(void)
{
    printf("\n");
    printf("=====================================\n");
    printf("  TFLM Secure Service Client (NS)   \n");
    printf("  Pure TF-M Implementation          \n");
    printf("=====================================\n");
    printf("\n");

    printf("[NS] Starting TFLM service tests...\n");
    simple_delay_ms(100);
    
    test_tflm_service();
    
    printf("\n[NS] Entering main loop...\n");
    
    int counter = 0;
    while (1) {
        printf("[NS] Heartbeat #%d - All systems operational\n", ++counter);
        simple_delay_ms(5000);
    }

    return 0;
}
