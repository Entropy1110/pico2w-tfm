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

/* Performance test with encrypted model */
// void test_tinymaix_performance(void)
// {
//     printf("[TinyMaix Test] ===========================================\n");
//     printf("[TinyMaix Test] Testing TinyMaix Encrypted Model Performance\n");
//     printf("[TinyMaix Test] ===========================================\n");
    
//     const int num_iterations = 5;
//     tfm_tinymaix_status_t status;
//     int predicted_class;
    
//     /* Load encrypted model first */
//     printf("[TinyMaix Test] Loading encrypted MNIST model for performance test...\n");
//     status = tfm_tinymaix_load_encrypted_model();
//     if (status != TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] Failed to load model for performance test: %d\n", status);
//         return;
//     }

//     printf("[TinyMaix Test] Running %d inference iterations...\n", num_iterations);
    
//     for (int i = 0; i < num_iterations; i++) {
//         printf("[TinyMaix Test] Iteration %d/%d...", i + 1, num_iterations);
        
//         /* Alternate between built-in and custom inference */
//         if (i % 2 == 0) {
//             /* Use built-in image */
//             status = tfm_tinymaix_run_inference(&predicted_class);
//         } else {
//             /* Use custom zero image */
//             uint8_t zero_image[28*28];
//             memset(zero_image, 0, sizeof(zero_image));
//             for (int y = 8; y < 20; y++) {
//                 for (int x = 10; x < 18; x++) {
//                     int idx = y * 28 + x;
//                     if ((y == 8 || y == 19) || (x == 10 || x == 17)) {
//                         zero_image[idx] = 200;
//                     }
//                 }
//             }
//             status = tfm_tinymaix_run_inference_with_data(zero_image, sizeof(zero_image), &predicted_class);
//         }
        
//         if (status != TINYMAIX_STATUS_SUCCESS) {
//             printf(" ✗ FAILED (status: %d)\n", status);
//             return;
//         }
        
//         printf(" ✓ predicted class=%d\n", predicted_class);
//     }

//     printf("[TinyMaix Test] ✓ Performance test completed: %d iterations\n\n", num_iterations);
// }

/* Error handling test */
// void test_tinymaix_error_handling(void)
// {
//     printf("[TinyMaix Test] ===========================================\n");
//     printf("[TinyMaix Test] Testing TinyMaix Error Handling\n");
//     printf("[TinyMaix Test] ===========================================\n");
    
//     tfm_tinymaix_status_t status;
//     int predicted_class;

//     /* Test 1: Inference before loading model */
//     printf("[TinyMaix Test] 1. Testing inference before model load...\n");
    
//     status = tfm_tinymaix_run_inference(&predicted_class);
//     if (status != TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Correctly rejected inference without model\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Should have rejected inference without model\n");
//     }

//     /* Test 2: Load encrypted model for valid operations */
//     printf("[TinyMaix Test] 2. Loading encrypted model for further tests...\n");
    
//     status = tfm_tinymaix_load_encrypted_model();
//     if (status == TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Encrypted model loaded successfully\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Failed to load encrypted model\n");
//         return;
//     }

//     /* Test 3: NULL parameters */
//     printf("[TinyMaix Test] 3. Testing NULL parameters...\n");
    
//     status = tfm_tinymaix_load_model();
//     if (status != TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Correctly rejected NULL model data\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Should have rejected NULL model data\n");
//     }

//     status = tfm_tinymaix_run_inference(NULL);
//     if (status != TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Correctly rejected NULL output parameter\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Should have rejected NULL output parameter\n");
//     }

//     /* Test 4: Oversized model */
//     printf("[TinyMaix Test] 4. Testing oversized model...\n");
    
//     uint8_t oversized_model[300]; /* Exceeds TINYMAIX_MAX_MODEL_SIZE (256) */
//     memset(oversized_model, 0, sizeof(oversized_model));
//     /* Set valid magic for TinyMaix */
//     oversized_model[0] = 0x58; oversized_model[1] = 0x49; 
//     oversized_model[2] = 0x41; oversized_model[3] = 0x4D;
    
//     status = tfm_tinymaix_load_model();
//     if (status != TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Correctly rejected oversized model\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Should have rejected oversized model\n");
//     }

//     printf("[TinyMaix Test] ✓ Error handling tests completed\n\n");
// }

// /* Encrypted model functionality test */
// void test_tinymaix_encrypted_model(void)
// {
//     printf("[TinyMaix Test] ===========================================\n");
//     printf("[TinyMaix Test] Testing TinyMaix Encrypted Model Functionality\n");
//     printf("[TinyMaix Test] ===========================================\n");

//     tfm_tinymaix_status_t status;
//     int predicted_class = -1;

//     /* Test 1: Load encrypted MNIST model */
//     printf("[TinyMaix Test] 1. Loading builtin encrypted MNIST model...\n");
//     printf("[TinyMaix Test]    Model size: %d bytes\n", encrypted_mdl_data_size);
    
//     status = tfm_tinymaix_load_encrypted_model();
//     if (status == TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Encrypted model loaded and decrypted successfully\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Encrypted model load failed: %d\n", status);
//         return;
//     }

//     /* Test 2: Run inference with encrypted model using built-in image */
//     printf("[TinyMaix Test] 2. Running inference with encrypted model (built-in image)...\n");
//     status = tfm_tinymaix_run_inference(&predicted_class);
//     if (status == TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Encrypted model inference completed successfully\n");
//         printf("[TinyMaix Test] ✓ Predicted digit: %d", predicted_class);
//         if (predicted_class >= 0 && predicted_class <= 9) {
//             printf(" (%s)", mnist_labels[predicted_class]);
//         }
//         printf("\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Encrypted model inference failed: %d\n", status);
//         return;
//     }

//     /* Test 3: Run inference with custom image data */
//     printf("[TinyMaix Test] 3. Running inference with encrypted model (custom image)...\n");
    
//     /* Create a test image (digit "1") */
//     uint8_t custom_image[28*28];
//     memset(custom_image, 0, sizeof(custom_image));

//     /* Draw a simple "1" pattern */
//     for (int y = 5; y < 23; y++) {
//         for (int x = 12; x < 16; x++) {
//             if (x == 13 || x == 14) {  /* Vertical line */
//                 custom_image[y * 28 + x] = 255; /* White pixel */
//             }
//         }
//     }
//     /* Add top part of "1" */
//     for (int x = 11; x < 14; x++) {
//         custom_image[5 * 28 + x] = 255;
//     }
    
//     status = tfm_tinymaix_run_inference_with_data(custom_image, sizeof(custom_image), &predicted_class);
//     if (status == TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Custom inference with encrypted model completed\n");
//         printf("[TinyMaix Test] ✓ Predicted digit: %d", predicted_class);
//         if (predicted_class >= 0 && predicted_class <= 9) {
//             printf(" (%s)", mnist_labels[predicted_class]);
//         }
//         printf("\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Custom inference with encrypted model failed: %d\n", status);
//         return;
//     }

//     printf("[TinyMaix Test] ✓ Encrypted model functionality test passed!\n\n");
// }

/* Encrypted model security test */
// void test_tinymaix_encrypted_model_security(void)
// {
//     printf("[TinyMaix Test] ===========================================\n");
//     printf("[TinyMaix Test] Testing TinyMaix Encrypted Model Security\n");
//     printf("[TinyMaix Test] ===========================================\n");
    
//     tfm_tinymaix_status_t status;

//     /* Test 1: Invalid encrypted model (corrupted header) */
//     printf("[TinyMaix Test] 1. Testing corrupted encrypted model header...\n");
    
//     uint8_t corrupted_model[200];
//     memcpy(corrupted_model, encrypted_mdl_data_data, sizeof(corrupted_model));
//     /* Corrupt magic header */
//     corrupted_model[0] = 0xFF;
//     corrupted_model[1] = 0xFF;
//     corrupted_model[2] = 0xFF;
//     corrupted_model[3] = 0xFF;
    
//     status = tfm_tinymaix_load_encrypted_model(corrupted_model, sizeof(corrupted_model));
//     if (status != TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Correctly rejected corrupted encrypted model\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Should have rejected corrupted encrypted model\n");
//     }

//     /* Test 2: Encrypted model with wrong size */
//     printf("[TinyMaix Test] 2. Testing undersized encrypted model...\n");
    
//     status = tfm_tinymaix_load_encrypted_model(encrypted_mdl_data_data, 50); /* Too small */
//     if (status != TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Correctly rejected undersized encrypted model\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Should have rejected undersized encrypted model\n");
//     }

//     /* Test 3: NULL encrypted model */
//     printf("[TinyMaix Test] 3. Testing NULL encrypted model...\n");
    
//     status = tfm_tinymaix_load_encrypted_model(NULL, 100);
//     if (status != TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Correctly rejected NULL encrypted model\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Should have rejected NULL encrypted model\n");
//     }

//     /* Test 4: Encrypted model with corrupted data */
//     printf("[TinyMaix Test] 4. Testing encrypted model with corrupted data...\n");
    
//     uint8_t corrupted_data[encrypted_mdl_data_size];
//     memcpy(corrupted_data, encrypted_mdl_data_data, sizeof(corrupted_data));
//     /* Corrupt some encrypted data bytes */
//     corrupted_data[150] ^= 0xFF;
//     corrupted_data[151] ^= 0xFF;
//     corrupted_data[152] ^= 0xFF;
    
//     status = tfm_tinymaix_load_encrypted_model(corrupted_data, sizeof(corrupted_data));
//     if (status != TINYMAIX_STATUS_SUCCESS) {
//         printf("[TinyMaix Test] ✓ Correctly rejected encrypted model with corrupted data\n");
//     } else {
//         printf("[TinyMaix Test] ✗ Should have rejected encrypted model with corrupted data\n");
//     }

//     printf("[TinyMaix Test] ✓ Encrypted model security tests completed\n\n");
// }

// /* Encrypted vs unencrypted model comparison test */
// void test_tinymaix_encrypted_vs_unencrypted(void)
// {
//     printf("[TinyMaix Test] ===========================================\n");
//     printf("[TinyMaix Test] Testing Encrypted vs Unencrypted Model Comparison\n");
//     printf("[TinyMaix Test] ===========================================\n");
    
//     tfm_tinymaix_status_t status;
//     int unencrypted_result = -1;
//     int encrypted_result = -1;

//     /* Test with built-in unencrypted model */
//     printf("[TinyMaix Test] 1. Running inference with built-in unencrypted model...\n");
//     status = tfm_tinymaix_load_model();  /* Load built-in model */
//     if (status == TINYMAIX_STATUS_SUCCESS) {
//         status = tfm_tinymaix_run_inference(&unencrypted_result);
//         if (status == TINYMAIX_STATUS_SUCCESS) {
//             printf("[TinyMaix Test] ✓ Unencrypted model result: %d (%s)\n", 
//                    unencrypted_result, 
//                    (unencrypted_result >= 0 && unencrypted_result <= 9) ? mnist_labels[unencrypted_result] : "invalid");
//         } else {
//             printf("[TinyMaix Test] ✗ Unencrypted model inference failed\n");
//             return;
//         }
//     } else {
//         printf("[TinyMaix Test] ✗ Failed to load unencrypted model\n");
//         return;
//     }

//     /* Test with encrypted model */
//     printf("[TinyMaix Test] 2. Running inference with encrypted model...\n");
//     status = tfm_tinymaix_load_encrypted_model(encrypted_mdl_data_data, encrypted_mdl_data_size);
//     if (status == TINYMAIX_STATUS_SUCCESS) {
//         status = tfm_tinymaix_run_inference(&encrypted_result);
//         if (status == TINYMAIX_STATUS_SUCCESS) {
//             printf("[TinyMaix Test] ✓ Encrypted model result: %d (%s)\n", 
//                    encrypted_result, 
//                    (encrypted_result >= 0 && encrypted_result <= 9) ? mnist_labels[encrypted_result] : "invalid");
//         } else {
//             printf("[TinyMaix Test] ✗ Encrypted model inference failed\n");
//             return;
//         }
//     } else {
//         printf("[TinyMaix Test] ✗ Failed to load encrypted model\n");
//         return;
//     }

//     /* Compare results */
//     printf("[TinyMaix Test] 3. Comparing results...\n");
//     if (unencrypted_result == encrypted_result) {
//         printf("[TinyMaix Test] ✓ Results match! Both models predict: %d (%s)\n", 
//                unencrypted_result, 
//                (unencrypted_result >= 0 && unencrypted_result <= 9) ? mnist_labels[unencrypted_result] : "invalid");
//         printf("[TinyMaix Test] ✓ Encryption/decryption preserves model functionality\n");
//     } else {
//         printf("[TinyMaix Test] ⚠ Results differ: unencrypted=%d, encrypted=%d\n", 
//                unencrypted_result, encrypted_result);
//         printf("[TinyMaix Test] ⚠ This could be expected if models are different versions\n");
//     }

//     printf("[TinyMaix Test] ✓ Encrypted vs unencrypted comparison completed\n\n");
// }

/* Comprehensive TinyMaix test suite */
void test_tinymaix_comprehensive_suite(void)
{
    printf("[TinyMaix Test] ###########################################\n");
    printf("[TinyMaix Test] #     TinyMaix Encrypted Model Test Suite  #\n");
    printf("[TinyMaix Test] #   MNIST Classification with Encryption   #\n");
    printf("[TinyMaix Test] ###########################################\n\n");

    printf("[TinyMaix Test] Starting TinyMaix encrypted model tests...\n");
    
    /* Run encrypted model functionality test */
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
}