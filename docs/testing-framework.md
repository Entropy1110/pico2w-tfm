# Testing Framework Guide

## Overview

The testing framework provides comprehensive validation for TF-M secure partitions, PSA API functionality, and TinyMaix neural network inference. Tests run automatically on device boot and provide detailed feedback via UART/USB serial.

## Test Architecture

### Test Organization
```
tflm_ns/
├── test_runner.c              # Main test orchestrator
├── echo_test_app.c            # Echo service tests
├── psa_crypto_test.c          # PSA crypto API tests  
├── tinymaix_inference_test.c  # TinyMaix ML tests
└── app.h                      # Test declarations
```

### Test Execution Flow
```
Device Boot → RTOS Start → Test Runner → Individual Tests → Serial Output
```

## Test Runner

### Main Test Controller: `test_runner.c`

#### DEV_MODE vs Production Testing
```c
void run_all_tests(void* arg)
{
    UNUSED_VARIABLE(arg);
    
#ifdef DEV_MODE
    LOG_MSG("Starting TF-M Test Suite (DEV_MODE)...\r\n");
    LOG_MSG("DEV_MODE: Only HUK-derived model key test will run\r\n");
    
    /* DEV_MODE: Only run HUK key derivation test */
    test_tinymaix_comprehensive_suite();
    
    LOG_MSG("DEV_MODE tests completed!\r\n");
#else
    LOG_MSG("Starting TF-M Test Suite (Production Mode)...\r\n");
    
    /* Production Mode: Run all tests except DEV_MODE specific ones */
    test_echo_service();
    test_psa_encryption(); 
    test_psa_hash();
    test_tinymaix_comprehensive_suite();
    
    LOG_MSG("All production tests completed!\r\n");
#endif
}
```

### RTOS Integration
Tests run in RTOS context with proper task management:
```c
/* In main_ns.c */
osThreadNew(run_all_tests, NULL, &app_task_attr);
```

## Test Categories

### 1. Echo Service Tests

#### Basic Echo Functionality
```c
void test_echo_service(void)
{
    printf("[Echo Test] Testing Echo Service...\n");
    
    tfm_echo_status_t status;
    const char* test_string = "Hello, Secure World!";
    char output_buffer[256];
    
    /* Test basic echo */
    status = tfm_echo_call((uint8_t*)test_string, strlen(test_string),
                          (uint8_t*)output_buffer, sizeof(output_buffer));
    
    if (status == ECHO_STATUS_SUCCESS) {
        printf("[Echo Test] ✓ Echo successful: %s\n", output_buffer);
    } else {
        printf("[Echo Test] ✗ Echo failed: %d\n", status);
    }
}
```

### 2. PSA Crypto Tests

#### AES-128 Encryption Test
```c
void test_psa_encryption(void)
{
    printf("[PSA Crypto Test] Testing AES-128 encryption...\n");
    
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;
    
    /* Initialize PSA crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS && status != PSA_ERROR_ALREADY_EXISTS) {
        printf("[PSA Crypto Test] ✗ Init failed: %d\n", status);
        return;
    }
    
    /* Generate AES-128 key */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CTR);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    
    status = psa_generate_key(&attributes, &key_id);
    if (status == PSA_SUCCESS) {
        printf("[PSA Crypto Test] ✓ AES-128 key generated\n");
        
        /* Test encryption/decryption */
        test_aes_encrypt_decrypt(key_id);
        
        /* Cleanup */
        psa_destroy_key(key_id);
    } else {
        printf("[PSA Crypto Test] ✗ Key generation failed: %d\n", status);
    }
}
```

#### SHA-256 Hash Test
```c
void test_psa_hash(void)
{
    printf("[PSA Hash Test] Testing SHA-256...\n");
    
    psa_status_t status;
    const char* message = "Hello, TF-M World!";
    uint8_t hash[PSA_HASH_LENGTH(PSA_ALG_SHA_256)];
    size_t hash_length;
    
    /* Compute SHA-256 hash */
    status = psa_hash_compute(PSA_ALG_SHA_256,
                             (const uint8_t*)message, strlen(message),
                             hash, sizeof(hash), &hash_length);
    
    if (status == PSA_SUCCESS) {
        printf("[PSA Hash Test] ✓ SHA-256 computed (%d bytes)\n", hash_length);
        
        /* Print hash (first 8 bytes) */
        printf("[PSA Hash Test] Hash: ");
        for (int i = 0; i < 8; i++) {
            printf("%02x", hash[i]);
        }
        printf("...\n");
    } else {
        printf("[PSA Hash Test] ✗ Hash failed: %d\n", status);
    }
}
```

### 3. TinyMaix Inference Tests

#### Production Mode Tests
```c
void test_tinymaix_basic_functionality(void)
{
    printf("[TinyMaix Test] Testing encrypted model functionality...\n");
    
    tfm_tinymaix_status_t status;
    int predicted_class = -1;
    
    /* Test 1: Load encrypted model */
    printf("[TinyMaix Test] 1. Loading builtin encrypted MNIST model...\n");
    status = tfm_tinymaix_load_encrypted_model();
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Encrypted model loaded successfully\n");
    } else {
        printf("[TinyMaix Test] ✗ Model load failed: %d\n", status);
        return;
    }
    
    /* Test 2: Built-in image inference */
    printf("[TinyMaix Test] 2. Running inference with built-in image...\n");
    status = tfm_tinymaix_run_inference(&predicted_class);
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Built-in inference successful\n");
        printf("[TinyMaix Test] ✓ Predicted digit: %d\n", predicted_class);
    } else {
        printf("[TinyMaix Test] ✗ Built-in inference failed: %d\n", status);
        return;
    }
    
    /* Test 3: Custom image inference */
    printf("[TinyMaix Test] 3. Running inference with custom image...\n");
    
    uint8_t custom_image[28*28];
    create_test_digit_pattern(custom_image, 7); /* Create digit "7" */
    
    status = tfm_tinymaix_run_inference_with_data(custom_image, sizeof(custom_image), 
                                                  &predicted_class);
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Custom inference successful\n");
        printf("[TinyMaix Test] ✓ Predicted digit: %d\n", predicted_class);
    } else {
        printf("[TinyMaix Test] ✗ Custom inference failed: %d\n", status);
    }
}
```

#### DEV_MODE Tests
```c
#ifdef DEV_MODE
void test_tinymaix_get_model_key(void)
{
    printf("[TinyMaix Test] Testing HUK-derived model key (DEV_MODE)...\n");
    
    tfm_tinymaix_status_t status;
    uint8_t key_buffer[16]; /* 128-bit key */
    
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
    
    /* Test 2: Validate key consistency */
    printf("[TinyMaix Test] 2. Verifying key consistency...\n");
    uint8_t key_buffer2[16];
    status = tfm_tinymaix_get_model_key(key_buffer2, sizeof(key_buffer2));
    if (status == TINYMAIX_STATUS_SUCCESS) {
        int keys_match = (memcmp(key_buffer, key_buffer2, 16) == 0);
        if (keys_match) {
            printf("[TinyMaix Test] ✓ Key derivation is consistent\n");
        } else {
            printf("[TinyMaix Test] ✗ Key derivation is inconsistent\n");
        }
    }
    
    /* Test 3: Invalid parameters */
    printf("[TinyMaix Test] 3. Testing invalid parameters...\n");
    status = tfm_tinymaix_get_model_key(NULL, 16);
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Correctly rejected NULL buffer\n");
    }
    
    status = tfm_tinymaix_get_model_key(key_buffer, 8); /* Too small */
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Correctly rejected small buffer\n");
    }
}
#endif
```

## Test Utilities

### MNIST Pattern Generation
```c
static void create_test_digit_pattern(uint8_t* image, int digit)
{
    memset(image, 0, 28*28);
    
    switch (digit) {
        case 0:
            /* Draw circle pattern for "0" */
            for (int y = 8; y < 20; y++) {
                for (int x = 10; x < 18; x++) {
                    if ((y == 8 || y == 19) || (x == 10 || x == 17)) {
                        image[y * 28 + x] = 255;
                    }
                }
            }
            break;
            
        case 1:
            /* Draw vertical line for "1" */
            for (int y = 5; y < 23; y++) {
                image[y * 28 + 14] = 255;
            }
            break;
            
        case 7:
            /* Draw "7" pattern */
            for (int x = 10; x < 18; x++) {
                image[8 * 28 + x] = 255; /* Top line */
            }
            for (int y = 8; y < 20; y++) {
                image[y * 28 + 17] = 255; /* Vertical line */
            }
            break;
            
        default:
            /* Default square pattern */
            for (int y = 10; y < 18; y++) {
                for (int x = 10; x < 18; x++) {
                    image[y * 28 + x] = 255;
                }
            }
            break;
    }
}
```

### Test Result Validation
```c
static bool validate_inference_result(int predicted_class, int expected_class)
{
    if (predicted_class < 0 || predicted_class > 9) {
        printf("[Validation] ✗ Invalid prediction: %d (out of range)\n", predicted_class);
        return false;
    }
    
    if (predicted_class == expected_class) {
        printf("[Validation] ✓ Prediction matches expected: %d\n", predicted_class);
        return true;
    } else {
        printf("[Validation] ⚠ Prediction mismatch: got %d, expected %d\n", 
               predicted_class, expected_class);
        return false; /* Could be acceptable for some test patterns */
    }
}
```

## Test Output and Logging

### Serial Output Format
Tests output structured logs via UART/USB:
```
[Component Test] Operation description...
[Component Test] ✓ Success message
[Component Test] ✗ Error message: error_code
[Component Test] ⚠ Warning message
```

### Example Test Output
```
Starting TF-M Test Suite (Production Mode)...

[Echo Test] ===========================================
[Echo Test] Testing Echo Service
[Echo Test] ===========================================
[Echo Test] ✓ Echo successful: Hello, Secure World!

[PSA Crypto Test] ===========================================
[PSA Crypto Test] Testing PSA Crypto Operations
[PSA Crypto Test] ===========================================
[PSA Crypto Test] ✓ AES-128 key generated
[PSA Crypto Test] ✓ Encryption successful (16 bytes)
[PSA Crypto Test] ✓ Decryption successful (16 bytes)
[PSA Crypto Test] ✓ Data integrity verified

[PSA Hash Test] ===========================================
[PSA Hash Test] Testing SHA-256
[PSA Hash Test] ===========================================
[PSA Hash Test] ✓ SHA-256 computed (32 bytes)
[PSA Hash Test] Hash: 2c26b46b...

[TinyMaix Test] ###########################################
[TinyMaix Test] #     TinyMaix Encrypted Model Test Suite  #
[TinyMaix Test] #   MNIST Classification with Encryption   #
[TinyMaix Test] ###########################################

[TinyMaix Test] 1. Loading builtin encrypted MNIST model...
[TinyMaix Test] ✓ Encrypted model loaded successfully
[TinyMaix Test] 2. Running inference with built-in image...
[TinyMaix Test] ✓ Built-in inference successful
[TinyMaix Test] ✓ Predicted digit: 2
[TinyMaix Test] 3. Running inference with custom image...
[TinyMaix Test] ✓ Custom inference successful
[TinyMaix Test] ✓ Predicted digit: 7

All production tests completed!
```

## Adding New Tests

### Step 1: Create Test Function
```c
/* In appropriate test file */
void test_my_new_feature(void)
{
    printf("[My Feature Test] Testing new feature...\n");
    
    /* Setup */
    setup_test_environment();
    
    /* Test execution */
    int result = call_feature_under_test();
    
    /* Validation */
    if (result == EXPECTED_VALUE) {
        printf("[My Feature Test] ✓ Test passed\n");
    } else {
        printf("[My Feature Test] ✗ Test failed: got %d, expected %d\n", 
               result, EXPECTED_VALUE);
    }
    
    /* Cleanup */
    cleanup_test_environment();
}
```

### Step 2: Add to Test Runner
```c
/* In test_runner.c */
void run_all_tests(void* arg)
{
    /* ... existing tests ... */
    
    /* Add new test */
    test_my_new_feature();
    
    /* ... */
}
```

### Step 3: Add Declaration
```c
/* In app.h */
void test_my_new_feature(void);
```

### Step 4: Update Build
```cmake
# In app_broker/CMakeLists.txt
target_sources(tfm_tflm_broker
    PRIVATE
        # ... existing sources ...
        ../tflm_ns/my_new_feature_test.c
)
```

## Performance Testing

### Timing Measurements
```c
void test_performance_timing(void)
{
    printf("[Performance Test] Measuring operation timing...\n");
    
    /* Get system tick before operation */
    uint32_t start_tick = osKernelGetTickCount();
    
    /* Execute operation under test */
    perform_operation();
    
    /* Get system tick after operation */
    uint32_t end_tick = osKernelGetTickCount();
    
    /* Calculate elapsed time */
    uint32_t elapsed_ms = end_tick - start_tick;
    printf("[Performance Test] Operation took %d ms\n", elapsed_ms);
    
    /* Validate timing requirements */
    if (elapsed_ms <= MAX_ALLOWED_TIME_MS) {
        printf("[Performance Test] ✓ Timing requirement met\n");
    } else {
        printf("[Performance Test] ✗ Timing requirement exceeded\n");
    }
}
```

### Memory Usage Testing
```c
void test_memory_usage(void)
{
    printf("[Memory Test] Checking memory usage...\n");
    
    /* Get initial memory stats */
    size_t initial_free = get_free_memory();
    
    /* Allocate resources */
    void* resource = allocate_test_resource();
    
    /* Check memory consumption */
    size_t after_alloc = get_free_memory();
    size_t consumed = initial_free - after_alloc;
    
    printf("[Memory Test] Memory consumed: %d bytes\n", consumed);
    
    /* Free resources */
    free_test_resource(resource);
    
    /* Verify cleanup */
    size_t final_free = get_free_memory();
    if (final_free == initial_free) {
        printf("[Memory Test] ✓ No memory leaks detected\n");
    } else {
        printf("[Memory Test] ✗ Memory leak: %d bytes\n", 
               initial_free - final_free);
    }
}
```

## Error Testing

### Invalid Parameter Testing
```c
void test_error_handling(void)
{
    printf("[Error Test] Testing error handling...\n");
    
    /* Test NULL parameters */
    tfm_service_status_t status = tfm_service_call(NULL, 0, NULL, 0);
    if (status == SERVICE_ERROR_INVALID_PARAM) {
        printf("[Error Test] ✓ NULL parameter rejected\n");
    } else {
        printf("[Error Test] ✗ NULL parameter not handled\n");
    }
    
    /* Test oversized input */
    uint8_t oversized_data[MAX_SIZE + 1];
    status = tfm_service_call(oversized_data, sizeof(oversized_data), NULL, 0);
    if (status == SERVICE_ERROR_BUFFER_TOO_LARGE) {
        printf("[Error Test] ✓ Oversized input rejected\n");
    } else {
        printf("[Error Test] ✗ Oversized input not handled\n");
    }
}
```

### Stress Testing
```c
void test_stress_operations(void)
{
    printf("[Stress Test] Running stress test...\n");
    
    const int num_iterations = 100;
    int success_count = 0;
    
    for (int i = 0; i < num_iterations; i++) {
        tfm_service_status_t status = tfm_service_call(...);
        if (status == SERVICE_SUCCESS) {
            success_count++;
        }
        
        /* Brief delay between operations */
        osDelay(10);
    }
    
    printf("[Stress Test] Success rate: %d/%d (%.1f%%)\n", 
           success_count, num_iterations, 
           (float)success_count * 100.0f / num_iterations);
    
    if (success_count == num_iterations) {
        printf("[Stress Test] ✓ All operations successful\n");
    } else {
        printf("[Stress Test] ⚠ Some operations failed\n");
    }
}
```

## Test Configuration

### Compile-Time Test Control
```c
/* Enable/disable specific test categories */
#define ENABLE_ECHO_TESTS        1
#define ENABLE_CRYPTO_TESTS      1
#define ENABLE_TINYMAIX_TESTS    1
#define ENABLE_PERFORMANCE_TESTS 0
#define ENABLE_STRESS_TESTS      0

#ifdef DEV_MODE
#define ENABLE_DEBUG_TESTS       1
#else
#define ENABLE_DEBUG_TESTS       0
#endif
```

### Runtime Test Selection
```c
void run_selected_tests(uint32_t test_mask)
{
    if (test_mask & TEST_ECHO) {
        test_echo_service();
    }
    
    if (test_mask & TEST_CRYPTO) {
        test_psa_encryption();
        test_psa_hash();
    }
    
    if (test_mask & TEST_TINYMAIX) {
        test_tinymaix_comprehensive_suite();
    }
}
```

## Debugging Tests

### Debug Output
```c
#ifdef DEBUG_TESTS
#define TEST_DEBUG(fmt, ...) printf("[TEST DEBUG] " fmt, ##__VA_ARGS__)
#else
#define TEST_DEBUG(fmt, ...)
#endif

void debug_test_function(void)
{
    TEST_DEBUG("Entering test function\n");
    TEST_DEBUG("Parameter value: %d\n", param);
    /* ... test logic ... */
    TEST_DEBUG("Test result: %d\n", result);
}
```

### Test Isolation
```c
void isolated_test(void)
{
    /* Save system state */
    save_system_state();
    
    /* Run test */
    run_test_logic();
    
    /* Restore system state */
    restore_system_state();
}
```

## Next Steps

- **[Secure Partitions](./secure-partitions.md)**: Test custom secure services
- **[PSA API](./psa-api.md)**: Test PSA client-service communication
- **[Troubleshooting](./troubleshooting.md)**: Debug failing tests