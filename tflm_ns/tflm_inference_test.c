#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "tfm_tflm_inference_defs.h"
#include "psa/client.h"

// Include the real audio preprocessor model
#include "../models/audio_preprocessor_int8.h"

// Forward declarations
extern psa_status_t tfm_tflm_load_model(const uint8_t* model_data, size_t model_size);
extern psa_status_t tfm_tflm_set_input_data(const uint8_t* input_data, size_t input_size);
extern psa_status_t tfm_tflm_run_inference(void);
extern psa_status_t tfm_tflm_get_output_data(uint8_t* output_data, size_t output_size, size_t* actual_size);
extern psa_status_t tfm_tflm_get_input_size(size_t* input_size);
extern psa_status_t tfm_tflm_get_output_size(size_t* output_size);
extern void tfm_tflm_cleanup(void);

static void print_test_result(const char* test_name, psa_status_t result)
{
    printf("[TFLM Test] %s: %s (status: %d)\n", 
           test_name, 
           (result == PSA_SUCCESS) ? "PASS" : "FAIL", 
           (int)result);
}

static void print_hex_data(const char* label, const uint8_t* data, size_t size)
{
    printf("[TFLM Test] %s: ", label);
    for (size_t i = 0; i < size && i < 16; i++) {
        printf("%02x ", data[i]);
    }
    if (size > 16) {
        printf("... (%d bytes total)", (int)size);
    }
    printf("\n");
}

// Generate realistic audio input data for testing
static void generate_audio_test_data(uint8_t* buffer, size_t size)
{
    // Audio preprocessor typically expects audio frame data
    // For testing, generate a simple pattern that simulates audio data
    for (size_t i = 0; i < size; i++) {
        // Generate a simple sawtooth wave pattern with some variation
        // This avoids needing math library functions like sin()
        uint8_t base = (uint8_t)((i * 255) / size); // Sawtooth from 0 to 255
        uint8_t noise = (uint8_t)((i * 17 + 123) % 32); // Simple pseudo-random noise
        buffer[i] = (base + noise) & 0xFF; // Keep in range
    }
}

void tflm_inference_test(void)
{
    printf("\n========================================\n");
    printf("TensorFlow Lite Micro Inference Test\n");
    printf("========================================\n");

    psa_status_t status;
    size_t input_size = 0;
    size_t output_size = 0;
    uint8_t output_buffer[TFM_TFLM_MAX_OUTPUT_SIZE];
    size_t actual_output_size = 0;

    // Test 1: Load model
    printf("\n--- Test 1: Load Audio Preprocessor Model ---\n");
    printf("[TFLM Test] Loading audio preprocessor model (%d bytes)...\n", (int)audio_preprocessor_int8_tflite_len);
    status = tfm_tflm_load_model(audio_preprocessor_int8_tflite, audio_preprocessor_int8_tflite_len);
    print_test_result("Model Loading", status);

    if (status != PSA_SUCCESS) {
        printf("[TFLM Test] Model loading failed, skipping remaining tests\n");
        goto cleanup;
    }

    // Test 2: Get input size
    printf("\n--- Test 2: Get Input Size ---\n");
    status = tfm_tflm_get_input_size(&input_size);
    print_test_result("Get Input Size", status);
    if (status == PSA_SUCCESS) {
        printf("[TFLM Test] Input tensor size: %d bytes\n", (int)input_size);
    }

    // Test 3: Get output size
    printf("\n--- Test 3: Get Output Size ---\n");
    status = tfm_tflm_get_output_size(&output_size);
    print_test_result("Get Output Size", status);
    if (status == PSA_SUCCESS) {
        printf("[TFLM Test] Output tensor size: %d bytes\n", (int)output_size);
    }

    // Test 4: Set input data (if we got valid input size)
    if (input_size > 0 && input_size <= TFM_TFLM_MAX_INPUT_SIZE) {
        printf("\n--- Test 4: Set Input Data ---\n");
        
        // Create realistic audio input data
        uint8_t* input_data = malloc(input_size);
        if (input_data) {
            // Generate audio test data
            generate_audio_test_data(input_data, input_size);
            
            printf("[TFLM Test] Setting audio input data (%d bytes)...\n", (int)input_size);
            print_hex_data("Audio input data", input_data, input_size);
            
            status = tfm_tflm_set_input_data(input_data, input_size);
            print_test_result("Set Input Data", status);
            
            free(input_data);
        } else {
            printf("[TFLM Test] Failed to allocate input buffer\n");
            status = PSA_ERROR_INSUFFICIENT_MEMORY;
            print_test_result("Set Input Data", status);
        }
    } else {
        printf("\n--- Test 4: Set Input Data ---\n");
        printf("[TFLM Test] Invalid input size (%d), skipping input data test\n", (int)input_size);
    }

    // Test 5: Run inference
    printf("\n--- Test 5: Run Inference ---\n");
    printf("[TFLM Test] Running inference...\n");
    status = tfm_tflm_run_inference();
    print_test_result("Run Inference", status);

    // Test 6: Get output data (if inference succeeded)
    if (status == PSA_SUCCESS && output_size > 0) {
        printf("\n--- Test 6: Get Output Data ---\n");
        printf("[TFLM Test] Getting output data (%d bytes)...\n", (int)output_size);
        
        memset(output_buffer, 0, sizeof(output_buffer));
        status = tfm_tflm_get_output_data(output_buffer, sizeof(output_buffer), &actual_output_size);
        print_test_result("Get Output Data", status);
        
        if (status == PSA_SUCCESS) {
            printf("[TFLM Test] Retrieved %d bytes of output data\n", (int)actual_output_size);
            print_hex_data("Output data", output_buffer, actual_output_size);
        }
    } else {
        printf("\n--- Test 6: Get Output Data ---\n");
        printf("[TFLM Test] Skipping output data test (inference failed or invalid output size)\n");
    }

    // Test 7: Error handling - invalid operations
    printf("\n--- Test 7: Error Handling ---\n");
    
    // Try to set input data with wrong size
    uint8_t dummy_data[10] = {0};
    status = tfm_tflm_set_input_data(dummy_data, sizeof(dummy_data));
    printf("[TFLM Test] Set input with wrong size: %s (expected failure)\n", 
           (status != PSA_SUCCESS) ? "FAIL (as expected)" : "UNEXPECTED PASS");

cleanup:
    // Cleanup
    printf("\n--- Cleanup ---\n");
    tfm_tflm_cleanup();
    printf("[TFLM Test] Cleanup completed\n");

    printf("\n========================================\n");
    printf("TFLM Inference Test Completed\n");
    printf("========================================\n");
}

// Additional utility function for loading actual model file
void tflm_load_audio_model_test(void)
{
    printf("\n========================================\n");
    printf("Audio Model Loading Test\n");
    printf("========================================\n");

    // NOTE: In a real implementation, you would read the actual model file
    // from flash memory or include it as a binary array
    printf("[TFLM Test] Audio model loading test not implemented\n");
    printf("[TFLM Test] To implement: include audio_preprocessor_int8.tflite as binary data\n");
    
    // Example of how to include the model:
    // 1. Convert .tflite file to C array using xxd or similar tool
    // 2. Include the array in this file
    // 3. Call tfm_tflm_load_model() with the actual model data
    
    printf("========================================\n");
}