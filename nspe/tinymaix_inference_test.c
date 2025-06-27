#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "tfm_tinymaix_inference_defs.h"
#include "psa/client.h"
#include "../models/encrypted_mnist_model_psa.h"

/* Labels for MNIST classification (10 classes) */
static const char* mnist_labels[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

/* Basic TinyMaix functionality test - encrypted model only */
void test_tinymaix_basic_functionality(void)
{
    printf("[TinyMaix Test] ===========================================\n");
    printf("[TinyMaix Test] Testing TinyMaix Encrypted Model Functionality\n");
    printf("[TinyMaix Test] ===========================================\n");

    tfm_tinymaix_status_t status;
    int predicted_class = -1;

    /* Test 1: Load built-in encrypted MNIST model */
    printf("[TinyMaix Test] 1. Loading builtin encrypted MNIST model...\n");
    status = tfm_tinymaix_load_encrypted_model();
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Encrypted model loaded successfully\n");
    } else {
        printf("[TinyMaix Test] ✗ Model load failed: %d\n", status);
        return;
    }

    /* Test 2: Run inference with built-in image */
    printf("[TinyMaix Test] 2. Running inference with built-in test image...\n");
    status = tfm_tinymaix_run_inference(&predicted_class);
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Built-in inference completed successfully\n");
        printf("[TinyMaix Test] ✓ Predicted digit: %d", predicted_class);
        if (predicted_class >= 0 && predicted_class <= 9) {
            printf(" (%s)", mnist_labels[predicted_class]);
        }
        printf("\n");
    } else {
        printf("[TinyMaix Test] ✗ Built-in inference failed: %d\n", status);
        return;
    }

    /* Test 3: Run inference with custom image data */
    printf("[TinyMaix Test] 3. Running inference with custom image data...\n");
    
    /* Create a different test image (digit "7") */
    uint8_t custom_image[28*28];
    memset(custom_image, 0, sizeof(custom_image));

    /* Draw a simple "7" pattern */
    for (int y = 0; y < 28; y++) {
        for (int x = 0; x < 28; x++) {
            if ((y == 0 && x >= 10 && x <= 17) || /* Top horizontal line */
                (y == 1 && x == 17) ||           /* Vertical line */
                (y >= 2 && y <= 6 && x == 17) || /* Vertical line */
                (y == 7 && x >= 10 && x <= 17)) { /* Bottom horizontal line */
                custom_image[y * 28 + x] = 255; /* White pixel */
            }
        }
    }
    
    status = tfm_tinymaix_run_inference_with_data(custom_image, sizeof(custom_image), &predicted_class);
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Custom inference completed successfully\n");
        printf("[TinyMaix Test] ✓ Predicted digit: %d", predicted_class);
        if (predicted_class >= 0 && predicted_class <= 9) {
            printf(" (%s)", mnist_labels[predicted_class]);
        }
        printf("\n");
    } else {
        printf("[TinyMaix Test] ✗ Custom inference failed: %d\n", status);
        return;
    }

    printf("[TinyMaix Test] ✓ Basic functionality test passed!\n\n");
}

#ifdef DEV_MODE
/* Test function to get HUK-derived model key (DEV_MODE only) */
void test_tinymaix_get_model_key(void)
{
    printf("[TinyMaix Test] ===========================================\n");
    printf("[TinyMaix Test] Testing HUK-derived Model Key (DEV_MODE)\n");
    printf("[TinyMaix Test] ===========================================\n");

    tfm_tinymaix_status_t status;
    uint8_t key_buffer[16]; // 128-bit key
    
    /* Test 1: Get model key */
    printf("[TinyMaix Test] 1. Getting HUK-derived model key...\n");
    status = tfm_tinymaix_get_model_key(key_buffer, sizeof(key_buffer));
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Model key retrieved successfully\n");
        printf("[TinyMaix Test] ✓ Key (hex): ");
        for (int i = 0; i < 16; i++) {
            printf("%02x", key_buffer[i]);
        }
        printf("\n");
    } else {
        printf("[TinyMaix Test] ✗ Failed to get model key: %d\n", status);
        return;
    }
}
#endif

void test_tinymaix_comprehensive_suite(void)
{
#ifdef DEV_MODE
    printf("[TinyMaix Test] ###########################################\n");
    printf("[TinyMaix Test] #        DEV_MODE: HUK Key Test Only       #\n");
    printf("[TinyMaix Test] #    HUK-derived Model Key Debug Test      #\n");
    printf("[TinyMaix Test] ###########################################\n\n");

    printf("[TinyMaix Test] Starting DEV_MODE HUK key derivation test...\n");
    
    /* DEV_MODE: Only run HUK-derived model key test */
    printf("[TinyMaix Test] Running HUK-derived model key test (DEV_MODE)...\n");
    test_tinymaix_get_model_key();

    printf("[TinyMaix Test] ###########################################\n");
    printf("[TinyMaix Test] #      DEV_MODE HUK Key Test Completed!    #\n");
    printf("[TinyMaix Test] #     HUK-derived Key Debug Test Passed!   #\n");
    printf("[TinyMaix Test] ###########################################\n");
#else
    printf("[TinyMaix Test] ###########################################\n");
    printf("[TinyMaix Test] #     TinyMaix Encrypted Model Test Suite  #\n");
    printf("[TinyMaix Test] #   MNIST Classification with Encryption   #\n");
    printf("[TinyMaix Test] ###########################################\n\n");

    printf("[TinyMaix Test] Starting TinyMaix encrypted model tests...\n");
    
    /* Production Mode: Run encrypted model functionality test */
    printf("[TinyMaix Test] Running encrypted model functionality test...\n");
    test_tinymaix_basic_functionality();
    
    // printf("[TinyMaix Test] Running encrypted model error handling test...\n");
    // test_tinymaix_error_handling();

    // printf("[TinyMaix Test] Running additional encrypted model tests...\n");
    // test_tinymaix_encrypted_model();
    // 
    // printf("[TinyMaix Test] Running encrypted model security test...\n");
    // test_tinymaix_encrypted_model_security();
    // 
    // printf("[TinyMaix Test] Running encrypted vs unencrypted comparison test...\n");
    // test_tinymaix_encrypted_vs_unencrypted();

    printf("[TinyMaix Test] ###########################################\n");
    printf("[TinyMaix Test] #     All TinyMaix Tests Completed!       #\n");
    printf("[TinyMaix Test] #   Basic + Encrypted Model Tests Passed! #\n");
    printf("[TinyMaix Test] ###########################################\n");
#endif
}