/*
 * Copyright (c) 2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include "tfm_echo_defs.h"
#include "tfm_ns_interface.h"

#define TEST_STRING "Hello, TF-M Echo Service!"
#define BUFFER_SIZE 256

void test_echo_service(void)
{
    psa_status_t status;
    uint8_t input_buffer[BUFFER_SIZE];
    uint8_t output_buffer[BUFFER_SIZE];
    size_t output_size;

    printf("\n=== TF-M Echo Service Test ===\n");

    /* Test 1: Basic echo test */
    printf("Test 1: Basic echo test\n");
    strncpy((char*)input_buffer, TEST_STRING, sizeof(input_buffer) - 1);
    input_buffer[sizeof(input_buffer) - 1] = '\0';
    
    size_t input_len = strlen((char*)input_buffer);
    printf("Input:  '%s' (length: %zu)\n", (char*)input_buffer, input_len);

    /* Clear output buffer */
    memset(output_buffer, 0, sizeof(output_buffer));

    /* Call echo service */
    status = tfm_echo_service(input_buffer, input_len, 
                              output_buffer, sizeof(output_buffer), 
                              &output_size);

    if (status == PSA_SUCCESS) {
        printf("Output: '%s' (length: %zu)\n", (char*)output_buffer, output_size);
        
        if (memcmp(input_buffer, output_buffer, input_len) == 0 && output_size == input_len) {
            printf("✓ Test 1 PASSED\n");
        } else {
            printf("✗ Test 1 FAILED - data mismatch\n");
        }
    } else {
        printf("✗ Test 1 FAILED - status: %d\n", (int)status);
    }

    /* Test 2: Empty data test */
    printf("\nTest 2: Empty data test\n");
    status = tfm_echo_service(NULL, 0, output_buffer, sizeof(output_buffer), &output_size);
    if (status == PSA_ERROR_INVALID_ARGUMENT) {
        printf("✓ Test 2 PASSED - correctly rejected NULL input\n");
    } else {
        printf("✗ Test 2 FAILED - should reject NULL input\n");
    }

    /* Test 3: Maximum size test */
    printf("\nTest 3: Maximum size test\n");
    memset(input_buffer, 'A', TFM_ECHO_MAX_DATA_SIZE);
    status = tfm_echo_service(input_buffer, TFM_ECHO_MAX_DATA_SIZE, 
                              output_buffer, sizeof(output_buffer), 
                              &output_size);
    
    if (status == PSA_SUCCESS && output_size == TFM_ECHO_MAX_DATA_SIZE) {
        printf("✓ Test 3 PASSED - maximum size data echoed correctly\n");
    } else {
        printf("✗ Test 3 FAILED - status: %d, output_size: %zu\n", (int)status, output_size);
    }

    /* Test 4: Oversized data test */
    printf("\nTest 4: Oversized data test\n");
    status = tfm_echo_service(input_buffer, TFM_ECHO_MAX_DATA_SIZE + 1, 
                              output_buffer, sizeof(output_buffer), 
                              &output_size);
    
    if (status == PSA_ERROR_INVALID_ARGUMENT) {
        printf("✓ Test 4 PASSED - correctly rejected oversized input\n");
    } else {
        printf("✗ Test 4 FAILED - should reject oversized input\n");
    }

    printf("=== Echo Service Tests Complete ===\n\n");
}
