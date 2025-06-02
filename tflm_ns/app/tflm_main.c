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
#include "psa_tflm_client.h"

/* Test data */
static const uint8_t dummy_model_data[] = {
    0x0B,0xB9,
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

static void print_hex(const char *label, const uint8_t *data, size_t len)
{
    printf("[NS] %s (%zu bytes): ", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n    ");
    }
    printf("\n");
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
int tflm_main(void)
{
    printf("\n");
    printf("=====================================\n");
    printf("  TFLM Echo Test (NS)               \n");
    printf("  Pure TF-M Implementation          \n");
    printf("=====================================\n");
    printf("\n");

    printf("[NS] Starting simple echo test...\n");
    simple_delay_ms(100);
    
    psa_status_t status;
    uint8_t output_data[64];
    size_t output_size = 0;
    
    const char *test_message = "Hello TFLM!";
    const uint8_t *test_data = (const uint8_t *)test_message;
    size_t test_data_size = strlen(test_message);
    
    printf("\n=== Echo Test ===\n");
    printf("[NS] Sending message to secure world: '%s'\n", test_message);
    print_hex("Input data", test_data, test_data_size);
    
    status = psa_tflm_echo(test_data, test_data_size, 
                           output_data, sizeof(output_data), 
                           &output_size);
    
    if (status != PSA_SUCCESS) {
        printf("[NS] ERROR: Echo test failed: %d\n", status);
    } else {
        printf("[NS] ✓ Echo test successful!\n");
        printf("[NS] Received %zu bytes from secure world\n", output_size);
        print_hex("Output data", output_data, output_size);
        
        output_data[output_size] = '\0';
        printf("[NS] Received message: '%s'\n", (char *)output_data);
    }
    
    test_tflm_service();
    
    printf("\n[NS] Entering main loop...\n");
    
    int counter = 0;
    while (1) {
        printf("[NS] Heartbeat #%d - All systems operational\n", ++counter);
        simple_delay_ms(5000);
    }

    return 0;
}
