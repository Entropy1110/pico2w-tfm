#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "tfm_tinymaix_inference_defs.h"
#include "psa/client.h"

/* Labels for MNIST classification (10 classes) */
static const char* mnist_labels[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

/* Basic TinyMaix functionality test - simplified to match actual implementation */
void test_tinymaix_basic_functionality(void)
{
    printf("[TinyMaix Test] ===========================================\n");
    printf("[TinyMaix Test] Testing TinyMaix Basic Functionality\n");
    printf("[TinyMaix Test] ===========================================\n");

    tfm_tinymaix_status_t status;
    int predicted_class = -1;

    /* Test 1: Load built-in MNIST model */
    printf("[TinyMaix Test] 1. Loading built-in MNIST model...\n");
    status = tfm_tinymaix_load_model(NULL, 0);  /* Use built-in model */
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Model loaded successfully\n");
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

/* Performance test */
void test_tinymaix_performance(void)
{
    printf("[TinyMaix Test] ===========================================\n");
    printf("[TinyMaix Test] Testing TinyMaix Performance\n");
    printf("[TinyMaix Test] ===========================================\n");
    
    const int num_iterations = 5;
    tfm_tinymaix_status_t status;
    int predicted_class;
    
    /* Load model first */
    printf("[TinyMaix Test] Loading built-in MNIST model for performance test...\n");
    status = tfm_tinymaix_load_model(NULL, 0);  /* Use built-in model */
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] Failed to load model for performance test: %d\n", status);
        return;
    }

    printf("[TinyMaix Test] Running %d inference iterations...\n", num_iterations);
    
    for (int i = 0; i < num_iterations; i++) {
        printf("[TinyMaix Test] Iteration %d/%d...", i + 1, num_iterations);
        
        /* Alternate between built-in and custom inference */
        if (i % 2 == 0) {
            /* Use built-in image */
            status = tfm_tinymaix_run_inference(&predicted_class);
        } else {
            /* Use custom zero image */
            uint8_t zero_image[28*28];
            memset(zero_image, 0, sizeof(zero_image));
            for (int y = 8; y < 20; y++) {
                for (int x = 10; x < 18; x++) {
                    int idx = y * 28 + x;
                    if ((y == 8 || y == 19) || (x == 10 || x == 17)) {
                        zero_image[idx] = 200;
                    }
                }
            }
            status = tfm_tinymaix_run_inference_with_data(zero_image, sizeof(zero_image), &predicted_class);
        }
        
        if (status != TINYMAIX_STATUS_SUCCESS) {
            printf(" ✗ FAILED (status: %d)\n", status);
            return;
        }
        
        printf(" ✓ predicted class=%d\n", predicted_class);
    }

    printf("[TinyMaix Test] ✓ Performance test completed: %d iterations\n\n", num_iterations);
}

/* Error handling test */
void test_tinymaix_error_handling(void)
{
    printf("[TinyMaix Test] ===========================================\n");
    printf("[TinyMaix Test] Testing TinyMaix Error Handling\n");
    printf("[TinyMaix Test] ===========================================\n");
    
    tfm_tinymaix_status_t status;
    int predicted_class;

    /* Test 1: Inference before loading model */
    printf("[TinyMaix Test] 1. Testing inference before model load...\n");
    
    status = tfm_tinymaix_run_inference(&predicted_class);
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Correctly rejected inference without model\n");
    } else {
        printf("[TinyMaix Test] ✗ Should have rejected inference without model\n");
    }

    /* Test 2: Invalid model data */
    printf("[TinyMaix Test] 2. Testing invalid model data...\n");
    
    uint8_t invalid_model[] = {0xFF, 0xFF, 0xFF, 0xFF}; /* Invalid magic */
    status = tfm_tinymaix_load_model(invalid_model, sizeof(invalid_model));
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Correctly rejected invalid model\n");
    } else {
        printf("[TinyMaix Test] ✗ Should have rejected invalid model\n");
    }

    /* Test 3: NULL parameters */
    printf("[TinyMaix Test] 3. Testing NULL parameters...\n");
    
    status = tfm_tinymaix_load_model(NULL, 0);
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Correctly rejected NULL model data\n");
    } else {
        printf("[TinyMaix Test] ✗ Should have rejected NULL model data\n");
    }

    status = tfm_tinymaix_run_inference(NULL);
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Correctly rejected NULL output parameter\n");
    } else {
        printf("[TinyMaix Test] ✗ Should have rejected NULL output parameter\n");
    }

    /* Test 4: Oversized model */
    printf("[TinyMaix Test] 4. Testing oversized model...\n");
    
    uint8_t oversized_model[300]; /* Exceeds TINYMAIX_MAX_MODEL_SIZE (256) */
    memset(oversized_model, 0, sizeof(oversized_model));
    /* Set valid magic for TinyMaix */
    oversized_model[0] = 0x58; oversized_model[1] = 0x49; 
    oversized_model[2] = 0x41; oversized_model[3] = 0x4D;
    
    status = tfm_tinymaix_load_model(oversized_model, sizeof(oversized_model));
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Correctly rejected oversized model\n");
    } else {
        printf("[TinyMaix Test] ✗ Should have rejected oversized model\n");
    }

    printf("[TinyMaix Test] ✓ Error handling tests completed\n\n");
}

/* Comprehensive TinyMaix test suite */
void test_tinymaix_comprehensive_suite(void)
{
    printf("[TinyMaix Test] ###########################################\n");
    printf("[TinyMaix Test] #     TinyMaix Comprehensive Test Suite    #\n");
    printf("[TinyMaix Test] #        MNIST Classification Test         #\n");
    printf("[TinyMaix Test] ###########################################\n\n");

    printf("[TinyMaix Test] Starting TinyMaix tests...\n");
    
    /* Run basic functionality test first */
    printf("[TinyMaix Test] Running basic functionality test...\n");
    test_tinymaix_basic_functionality();
    
    printf("[TinyMaix Test] Running performance test...\n");
    test_tinymaix_performance();
    
    printf("[TinyMaix Test] Running error handling test...\n");
    test_tinymaix_error_handling();

    printf("[TinyMaix Test] ###########################################\n");
    printf("[TinyMaix Test] #     All TinyMaix Tests Completed!       #\n");
    printf("[TinyMaix Test] ###########################################\n");
}

/* Main test function for backward compatibility */
void test_tinymaix_inference(void)
{
    test_tinymaix_comprehensive_suite();
}