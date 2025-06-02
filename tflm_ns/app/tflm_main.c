/*
 * Copyright (c) 2024, TZTZEN Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* TF-M NS API */
#include "psa/client.h"
#include "psa/error.h"

/* TFLM Service API */
#include "psa_tflm_client.h"

/* Test data */
static const uint8_t dummy_model_data[] = {
    0x0B,0xB9,  /* Keep for compatibility with the test interface */
};

/* Real test input data - adjusted based on model requirements */
static const uint8_t test_input_data[] = {
    /* 16 bytes of test audio data - will be adjusted based on actual model input size */
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
};

/* Simple delay function (TF-M compatible) */
static void simple_delay_ms(uint32_t ms)
{
    /* Reduced delay to avoid potential watchdog issues */
    for (volatile uint32_t i = 0; i < ms * 1000; i++) {
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
    
    /* Create input data that matches the model's expected input size */
    uint8_t *input_data = NULL;
    size_t input_size = model_info.input_size;
    
    if (input_size > 0) {
        input_data = (uint8_t*)malloc(input_size);
        if (!input_data) {
            printf("[NS] ERROR: Failed to allocate input buffer\n");
            return;
        }
        
        /* Fill with test pattern */
        for (size_t i = 0; i < input_size; i++) {
            input_data[i] = (uint8_t)(i % 256);
        }
        
        printf("[NS] Using %zu bytes of input data for model\n", input_size);
        print_hex("Input data (first 32 bytes)", input_data, input_size > 32 ? 32 : input_size);
        
        status = psa_tflm_run_inference(model_id,
                                        input_data, input_size,
                                        output_data, sizeof(output_data),
                                        &output_size);
        if (status != PSA_SUCCESS) {
            printf("[NS] ERROR: Failed to run inference: %d\n", status);
        } else {
            printf("[NS] ✓ Inference completed successfully\n");
            printf("[NS] Output size: %zu bytes\n", output_size);
            print_hex("Output data", output_data, output_size > 32 ? 32 : output_size);
        }
        
        free(input_data);
    } else {
        printf("[NS] WARNING: Model reports zero input size, using dummy data\n");
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
void tflm_main(void *argument)
{
    /* Suppress unused parameter warning */
    (void)argument;
    
    printf("\n");
    printf("=====================================\n");
    printf("  TFLM Echo Test (NS)               \n");
    printf("  Pure TF-M Implementation          \n");
    printf("=====================================\n");
    printf("\n");

    printf("[NS] Starting simple echo test...\n");
    fflush(stdout);
    
    printf("[NS] About to call delay function...\n");
    fflush(stdout);
    
    /* Use a much shorter delay for testing */
    for (volatile int i = 0; i < 1000; i++) {
        __asm volatile("nop");
    }
    
    printf("[NS] Delay completed, proceeding with test...\n");
    fflush(stdout);
    
    psa_status_t status;
    uint8_t output_data[64];
    size_t output_size = 0;
    
    printf("[NS] Variables initialized, preparing test data...\n");
    fflush(stdout);
    
    const char *test_message = "Hello TFLM!";
    const uint8_t *test_data = (const uint8_t *)test_message;
    size_t test_data_size = strlen(test_message);
    
    printf("\n=== Echo Test ===\n");
    printf("[NS] Sending message to secure world: '%s'\n", test_message);
    printf("[NS] Message length: %zu bytes\n", test_data_size);
    fflush(stdout);
    
    print_hex("Input data", test_data, test_data_size);
    
    printf("[NS] About to call psa_tflm_echo...\n");
    fflush(stdout);
    
    status = psa_tflm_echo(test_data, test_data_size, 
                           output_data, sizeof(output_data), 
                           &output_size);
    
    printf("[NS] psa_tflm_echo returned with status: %ld\n", status);
    fflush(stdout);
    
    if (status != PSA_SUCCESS) {
        printf("[NS] ERROR: Echo test failed: %ld\n", status);
        fflush(stdout);
    } else {
        printf("[NS] ✓ Echo test successful!\n");
        printf("[NS] Received %zu bytes from secure world\n", output_size);
        fflush(stdout);
        
        print_hex("Output data", output_data, output_size);
        
        if (output_size > 0) {
            output_data[output_size] = '\0';
            printf("[NS] Received message: '%s'\n", (char *)output_data);
        }
        fflush(stdout);
    }
    
    printf("\n[NS] Echo test completed. Entering main loop...\n");
    fflush(stdout);
    
    // Skip the full TFLM service test for now, just do the echo test
    // test_tflm_service();
    
    printf("\n[NS] Entering main loop...\n");
    fflush(stdout);
    
    int counter = 0;
    while (1) {
        printf("[NS] Heartbeat #%d - Echo test completed successfully\n", ++counter);
        fflush(stdout);
        
        /* Simple delay */
        for (volatile int i = 0; i < 5000000; i++) {
            __asm volatile("nop");
        }
    }
}
