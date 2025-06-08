#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "tfm_cmsis_nn_inference_defs.h"
#include "psa/client.h"

// Include the simple XOR model for testing
#include "../models/simple_xor_model.h"

// Forward declarations
extern psa_status_t tfm_cmsis_nn_load_model(const uint8_t* model_data, size_t model_size);
extern psa_status_t tfm_cmsis_nn_set_input_data(const uint8_t* input_data, size_t input_size);
extern psa_status_t tfm_cmsis_nn_run_inference(void);
extern psa_status_t tfm_cmsis_nn_get_output_data(uint8_t* output_data, size_t output_size, size_t* actual_size);
extern psa_status_t tfm_cmsis_nn_get_input_size(size_t* input_size);
extern psa_status_t tfm_cmsis_nn_get_output_size(size_t* output_size);
extern void tfm_cmsis_nn_cleanup(void);

static void print_test_result(const char* test_name, psa_status_t result)
{
    printf("[CMSIS-NN Test] %s: %s (status: %d)\n", 
           test_name, 
           (result == PSA_SUCCESS) ? "PASS" : "FAIL", 
           (int)result);
}

static void print_hex_data(const char* label, const uint8_t* data, size_t size)
{
    printf("[CMSIS-NN Test] %s: ", label);
    for (size_t i = 0; i < size && i < 16; i++) {
        printf("%02x ", data[i]);
    }
    if (size > 16) {
        printf("... (%d bytes total)", (int)size);
    }
    printf("\n");
}

static void print_int8_data(const char* label, const int8_t* data, size_t size)
{
    printf("[CMSIS-NN Test] %s: ", label);
    for (size_t i = 0; i < size && i < 8; i++) {
        printf("%d ", data[i]);
    }
    if (size > 8) {
        printf("... (%d values total)", (int)size);
    }
    printf("\n");
}

void cmsis_nn_inference_test(void)
{
    printf("\n========================================\n");
    printf("CMSIS-NN Inference Test\n");
    printf("========================================\n");

    psa_status_t status;
    size_t input_size = 0;
    size_t output_size = 0;
    uint8_t output_buffer[TFM_CMSIS_NN_MAX_OUTPUT_SIZE];
    size_t actual_output_size = 0;

    // Test 1: Load simple XOR model
    printf("\n--- Test 1: Load Simple XOR Model ---\n");
    size_t model_size;
    const uint8_t* model_data = get_simple_xor_model_data(&model_size);
    printf("[CMSIS-NN Test] Loading simple XOR model (%d bytes)...\n", (int)model_size);
    status = tfm_cmsis_nn_load_model(model_data, model_size);
    print_test_result("Model Loading", status);

    if (status != PSA_SUCCESS) {
        printf("[CMSIS-NN Test] Model loading failed, skipping remaining tests\n");
        goto cleanup;
    }

    // Test 2: Get input size
    printf("\n--- Test 2: Get Input Size ---\n");
    status = tfm_cmsis_nn_get_input_size(&input_size);
    print_test_result("Get Input Size", status);
    if (status == PSA_SUCCESS) {
        printf("[CMSIS-NN Test] Input tensor size: %d bytes\n", (int)input_size);
    }

    // Test 3: Get output size
    printf("\n--- Test 3: Get Output Size ---\n");
    status = tfm_cmsis_nn_get_output_size(&output_size);
    print_test_result("Get Output Size", status);
    if (status == PSA_SUCCESS) {
        printf("[CMSIS-NN Test] Output tensor size: %d bytes\n", (int)output_size);
    }

    // Test 4-7: Run all XOR test cases
    printf("\n--- Test 4-7: XOR Operation Tests ---\n");
    
    for (int test_case = 0; test_case < 4; test_case++) {
        printf("\n--- XOR Test Case %d ---\n", test_case);
        
        const int8_t* test_input = get_xor_test_input(test_case);
        int8_t expected_output = get_xor_expected_output(test_case);
        
        if (!test_input) {
            printf("[CMSIS-NN Test] Invalid test case %d\n", test_case);
            continue;
        }
        
        printf("[CMSIS-NN Test] Input: [%d, %d], Expected output: %d\n", 
               test_input[0], test_input[1], expected_output);
        print_int8_data("Input data", test_input, MODEL_INPUT_SIZE);
        
        // Set input data
        status = tfm_cmsis_nn_set_input_data((const uint8_t*)test_input, MODEL_INPUT_SIZE);
        if (status != PSA_SUCCESS) {
            printf("[CMSIS-NN Test] Failed to set input data for test case %d\n", test_case);
            print_test_result("Set Input Data", status);
            continue;
        }
        
        // Run inference
        status = tfm_cmsis_nn_run_inference();
        if (status != PSA_SUCCESS) {
            printf("[CMSIS-NN Test] Inference failed for test case %d\n", test_case);
            print_test_result("Run Inference", status);
            continue;
        }
        
        // Get output data
        memset(output_buffer, 0, sizeof(output_buffer));
        status = tfm_cmsis_nn_get_output_data(output_buffer, sizeof(output_buffer), &actual_output_size);
        if (status != PSA_SUCCESS) {
            printf("[CMSIS-NN Test] Failed to get output data for test case %d\n", test_case);
            print_test_result("Get Output Data", status);
            continue;
        }
        
        // Check output
        if (actual_output_size >= MODEL_OUTPUT_SIZE) {
            int8_t actual_output = ((int8_t*)output_buffer)[0];
            int is_correct = validate_xor_output(test_case, actual_output);
            
            printf("[CMSIS-NN Test] Actual output: %d, Expected: %d, %s\n", 
                   actual_output, expected_output, 
                   is_correct ? "CORRECT" : "INCORRECT");
            
            print_test_result("XOR Test Case", is_correct ? PSA_SUCCESS : PSA_ERROR_GENERIC_ERROR);
        } else {
            printf("[CMSIS-NN Test] Invalid output size: %d (expected at least %d)\n", 
                   (int)actual_output_size, (int)MODEL_OUTPUT_SIZE);
            print_test_result("XOR Test Case", PSA_ERROR_GENERIC_ERROR);
        }
    }

    // Test 8: Error handling - invalid operations
    printf("\n--- Test 8: Error Handling ---\n");
    
    // Try to set input data with wrong size
    uint8_t dummy_data[10] = {0};
    status = tfm_cmsis_nn_set_input_data(dummy_data, sizeof(dummy_data));
    printf("[CMSIS-NN Test] Set input with wrong size: %s (expected failure)\n", 
           (status != PSA_SUCCESS) ? "FAIL (as expected)" : "UNEXPECTED PASS");

    // Test 9: Performance measurement (simple timing)
    printf("\n--- Test 9: Performance Test ---\n");
    printf("[CMSIS-NN Test] Running 10 inferences for performance measurement...\n");
    
    const int8_t* perf_input = get_xor_test_input(0);
    status = tfm_cmsis_nn_set_input_data((const uint8_t*)perf_input, MODEL_INPUT_SIZE);
    
    if (status == PSA_SUCCESS) {
        for (int i = 0; i < 10; i++) {
            status = tfm_cmsis_nn_run_inference();
            if (status != PSA_SUCCESS) {
                printf("[CMSIS-NN Test] Performance test failed at iteration %d\n", i);
                break;
            }
        }
        
        if (status == PSA_SUCCESS) {
            printf("[CMSIS-NN Test] Performance test completed successfully\n");
            print_test_result("Performance Test", status);
        }
    }

cleanup:
    // Cleanup
    printf("\n--- Cleanup ---\n");
    tfm_cmsis_nn_cleanup();
    printf("[CMSIS-NN Test] Cleanup completed\n");

    printf("\n========================================\n");
    printf("CMSIS-NN Inference Test Completed\n");
    printf("========================================\n");
}

