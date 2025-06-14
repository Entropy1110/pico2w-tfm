# Secure Partitions Development Guide

## Overview

Secure partitions are isolated execution environments within the Secure Processing Environment (SPE) that provide specific services to non-secure applications via the PSA API.

## Partition Architecture

### Partition Types
- **Application Root of Trust (APP-ROT)**: Custom application services
- **PSA Root of Trust (PSA-ROT)**: Platform and crypto services (not covered here)

### Current Partitions
- **Echo Service** (PID: 444, SID: 0x00000105): Simple echo functionality
- **TinyMaix Inference** (PID: 445, SID: 0x00000107): Neural network inference

## Creating a New Partition

### Step 1: Directory Structure
```bash
mkdir -p partitions/my_service
cd partitions/my_service
```

Create the following files:
- `my_service.c` - Service implementation
- `my_service_manifest.yaml` - Service configuration
- `CMakeLists.txt` - Build configuration

### Step 2: Manifest File
Create `my_service_manifest.yaml`:

```yaml
{
  "psa_framework_version": 1.1,
  "name": "TFM_SP_MY_SERVICE",
  "type": "APPLICATION-ROT",
  "priority": "NORMAL",
  "entry_point": "my_service_entry",
  "stack_size": "0x2000",
  "services": [
    {
      "name": "TFM_MY_SERVICE",
      "sid": "0x00000108",  # Choose unique SID
      "connection_based": true,
      "non_secure_clients": true,
      "minor_version": 1,
      "minor_policy": "RELAXED"
    }
  ],
  "mmio_regions": [],
  "irqs": []
}
```

### Step 3: Service Implementation
Create `my_service.c`:

```c
#include "psa/service.h"
#include "psa_manifest/my_service_manifest.h"
#include "tfm_log_unpriv.h"

/* Service IPC message types */
#define MY_SERVICE_IPC_OPERATION_1  (0x2001U)
#define MY_SERVICE_IPC_OPERATION_2  (0x2002U)

/* Service entry point */
void my_service_entry(void)
{
    psa_msg_t msg;
    psa_status_t status;

    while (1) {
        /* Wait for messages */
        psa_wait(TFM_MY_SERVICE_SIGNAL, PSA_BLOCK);
        
        /* Get the message */
        if (psa_get(TFM_MY_SERVICE_SIGNAL, &msg) != PSA_SUCCESS) {
            continue;
        }
        
        /* Process message by type */
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                /* Handle connection request */
                INFO_UNPRIV("My Service: Client connected\n");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case MY_SERVICE_IPC_OPERATION_1:
                /* Handle operation 1 */
                status = handle_operation_1(&msg);
                psa_reply(msg.handle, status);
                break;
                
            case MY_SERVICE_IPC_OPERATION_2:
                /* Handle operation 2 */
                status = handle_operation_2(&msg);
                psa_reply(msg.handle, status);
                break;
                
            case PSA_IPC_DISCONNECT:
                /* Handle disconnection */
                INFO_UNPRIV("My Service: Client disconnected\n");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            default:
                /* Unsupported operation */
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

static psa_status_t handle_operation_1(psa_msg_t* msg)
{
    uint8_t input_buffer[256];
    uint8_t output_buffer[256];
    size_t bytes_read;
    
    /* Read input data */
    if (msg->in_size[0] > 0) {
        bytes_read = psa_read(msg->handle, 0, input_buffer, 
                              MIN(msg->in_size[0], sizeof(input_buffer)));
        
        /* Process input data */
        /* ... your processing logic ... */
        
        /* Write output data */
        if (msg->out_size[0] > 0) {
            psa_write(msg->handle, 0, output_buffer, 
                      MIN(msg->out_size[0], sizeof(output_buffer)));
        }
    }
    
    return PSA_SUCCESS;
}

static psa_status_t handle_operation_2(psa_msg_t* msg)
{
    /* Implement second operation */
    return PSA_SUCCESS;
}
```

### Step 4: CMakeLists.txt
Create partition build configuration:

```cmake
#-------------------------------------------------------------------------------
# Copyright (c) 2025, Arm Limited. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#-------------------------------------------------------------------------------

if (NOT TFM_PARTITION_MY_SERVICE)
    return()
endif()

cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0079 NEW)

add_library(tfm_app_rot_partition_my_service STATIC)

target_sources(tfm_app_rot_partition_my_service
    PRIVATE
        my_service.c
)

# Generated sources
target_sources(tfm_app_rot_partition_my_service
    PRIVATE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/my_service/auto_generated/intermedia_my_service_manifest.c
)

target_sources(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/my_service/auto_generated/load_info_my_service_manifest.c
)

# Include directories
target_include_directories(tfm_app_rot_partition_my_service
    PRIVATE
        .
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/my_service
        ${CMAKE_SOURCE_DIR}/../../interface/include
)

target_include_directories(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/my_service
)

# Link libraries
target_link_libraries(tfm_app_rot_partition_my_service
    PRIVATE
        platform_s
        tfm_sprt
        tfm_log_unpriv
)

target_link_libraries(tfm_partitions
    INTERFACE
        tfm_app_rot_partition_my_service
)

# Compile definitions
target_compile_definitions(tfm_app_rot_partition_my_service
    PRIVATE
        TFM_PARTITION_MY_SERVICE
        $<$<BOOL:${DEV_MODE}>:DEV_MODE>
)
```

### Step 5: Register Partition
Update `partitions/manifest_list.yaml`:

```yaml
{
  "description": "Manifest list",
  "type": "manifest_list",
  "version_major": 0,
  "version_minor": 1,
  "manifest_list": [
    {
      "description": "Echo Service",
      "manifest": "partitions/echo_service/echo_service_manifest.yaml",
      "output_path": "partitions/echo_service",
      "conditional": "TFM_PARTITION_ECHO_SERVICE",
      "version_major": 0,
      "version_minor": 1,
      "pid": 444
    },
    {
      "description": "TinyMaix Inference Service", 
      "manifest": "partitions/tinymaix_inference/tinymaix_inference_manifest.yaml",
      "output_path": "partitions/tinymaix_inference",
      "conditional": "TFM_PARTITION_TINYMAIX_INFERENCE",
      "version_major": 0,
      "version_minor": 1,
      "pid": 445
    },
    {
      "description": "My Custom Service",
      "manifest": "partitions/my_service/my_service_manifest.yaml", 
      "output_path": "partitions/my_service",
      "conditional": "TFM_PARTITION_MY_SERVICE",
      "version_major": 0,
      "version_minor": 1,
      "pid": 446
    }
  ]
}
```

### Step 6: Enable in Build
Add to `tflm_spe/config/config_tflm.cmake`:

```cmake
set(TFM_PARTITION_MY_SERVICE            ON          CACHE BOOL      "Enable My Service partition")
```

### Step 7: Update Root CMakeLists.txt
Add to `partitions/CMakeLists.txt`:

```cmake
add_subdirectory(my_service)
```

## Non-Secure API Development

### Step 1: Define API Interface
Create `interface/include/tfm_my_service_defs.h`:

```c
#ifndef __TFM_MY_SERVICE_DEFS_H__
#define __TFM_MY_SERVICE_DEFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* Service SID */
#define TFM_MY_SERVICE_SID              (0x00000108U)

/* IPC message types */
#define MY_SERVICE_IPC_OPERATION_1      (0x2001U)
#define MY_SERVICE_IPC_OPERATION_2      (0x2002U)

/* Status codes */
typedef enum {
    MY_SERVICE_STATUS_SUCCESS = 0,
    MY_SERVICE_STATUS_ERROR_INVALID_PARAM = -1,
    MY_SERVICE_STATUS_ERROR_GENERIC = -100
} tfm_my_service_status_t;

/* API function declarations */
tfm_my_service_status_t tfm_my_service_operation_1(const uint8_t* input, 
                                                   size_t input_size,
                                                   uint8_t* output, 
                                                   size_t output_size);

tfm_my_service_status_t tfm_my_service_operation_2(void);

#ifdef __cplusplus
}
#endif

#endif /* __TFM_MY_SERVICE_DEFS_H__ */
```

### Step 2: Implement Client API
Create `interface/src/tfm_my_service_api.c`:

```c
#include "psa/client.h"
#include "tfm_my_service_defs.h"

tfm_my_service_status_t tfm_my_service_operation_1(const uint8_t* input, 
                                                   size_t input_size,
                                                   uint8_t* output, 
                                                   size_t output_size)
{
    psa_status_t status;
    psa_handle_t handle;
    
    if (!input || !output) {
        return MY_SERVICE_STATUS_ERROR_INVALID_PARAM;
    }
    
    /* Connect to service */
    handle = psa_connect(TFM_MY_SERVICE_SID, 1);
    if (handle <= 0) {
        return MY_SERVICE_STATUS_ERROR_GENERIC;
    }
    
    /* Setup input/output vectors */
    psa_invec in_vec[] = {
        {.base = input, .len = input_size}
    };
    
    psa_outvec out_vec[] = {
        {.base = output, .len = output_size}
    };
    
    /* Call service */
    status = psa_call(handle, MY_SERVICE_IPC_OPERATION_1, in_vec, 1, out_vec, 1);
    
    /* Close connection */
    psa_close(handle);
    
    if (status != PSA_SUCCESS) {
        return MY_SERVICE_STATUS_ERROR_GENERIC;
    }
    
    return MY_SERVICE_STATUS_SUCCESS;
}

tfm_my_service_status_t tfm_my_service_operation_2(void)
{
    psa_status_t status;
    psa_handle_t handle;
    
    /* Connect to service */
    handle = psa_connect(TFM_MY_SERVICE_SID, 1);
    if (handle <= 0) {
        return MY_SERVICE_STATUS_ERROR_GENERIC;
    }
    
    /* Call service without parameters */
    status = psa_call(handle, MY_SERVICE_IPC_OPERATION_2, NULL, 0, NULL, 0);
    
    /* Close connection */
    psa_close(handle);
    
    if (status != PSA_SUCCESS) {
        return MY_SERVICE_STATUS_ERROR_GENERIC;
    }
    
    return MY_SERVICE_STATUS_SUCCESS;
}
```

## Testing Your Partition

### Step 1: Create Test Application
Create `tflm_ns/my_service_test.c`:

```c
#include <stdio.h>
#include "tfm_my_service_defs.h"

void test_my_service(void)
{
    printf("[My Service Test] ===================\n");
    printf("[My Service Test] Testing My Service\n");
    printf("[My Service Test] ===================\n");

    tfm_my_service_status_t status;
    uint8_t input_data[] = "Hello, Secure World!";
    uint8_t output_data[256];
    
    /* Test operation 1 */
    printf("[My Service Test] Testing operation 1...\n");
    status = tfm_my_service_operation_1(input_data, sizeof(input_data),
                                        output_data, sizeof(output_data));
    if (status == MY_SERVICE_STATUS_SUCCESS) {
        printf("[My Service Test] ✓ Operation 1 successful\n");
    } else {
        printf("[My Service Test] ✗ Operation 1 failed: %d\n", status);
    }
    
    /* Test operation 2 */
    printf("[My Service Test] Testing operation 2...\n");
    status = tfm_my_service_operation_2();
    if (status == MY_SERVICE_STATUS_SUCCESS) {
        printf("[My Service Test] ✓ Operation 2 successful\n");
    } else {
        printf("[My Service Test] ✗ Operation 2 failed: %d\n", status);
    }
    
    printf("[My Service Test] ✓ All tests completed\n\n");
}
```

### Step 2: Add to Test Runner
Update `tflm_ns/test_runner.c`:

```c
// Add include
#include "my_service_test.h"

// Add to run_all_tests()
void run_all_tests(void* arg)
{
    // ... existing tests ...
    
    /* Test: My Service */
    test_my_service();
    
    // ... rest of tests ...
}
```

### Step 3: Update Build Configuration
Add to `app_broker/CMakeLists.txt`:

```cmake
target_sources(tfm_tflm_broker
    PRIVATE
        # ... existing sources ...
        ../tflm_ns/my_service_test.c
        ../interface/src/tfm_my_service_api.c
)
```

## Best Practices

### Memory Management
- **Static Allocation**: Prefer static over dynamic allocation in secure partitions
- **Stack Size**: Set appropriate stack size in manifest (typical: 0x1000-0x4000)
- **Buffer Management**: Use fixed-size buffers, validate input sizes

### Error Handling
- **PSA Status Codes**: Use standard PSA status codes for inter-world communication
- **Input Validation**: Always validate input parameters and sizes
- **Graceful Degradation**: Handle errors without crashing the partition

### Security Considerations
- **Data Sanitization**: Clear sensitive data after use
- **Input Validation**: Validate all inputs from non-secure world
- **Resource Limits**: Implement appropriate resource limits and timeouts
- **Side Channel Protection**: Be aware of timing and power analysis attacks

### Performance Optimization
- **Minimize Context Switches**: Batch operations when possible
- **Efficient Data Transfer**: Use shared buffers for large data
- **Static Resources**: Pre-allocate resources during initialization

## Debugging

### Logging
```c
#include "tfm_log_unpriv.h"

/* Use INFO_UNPRIV for debug messages */
INFO_UNPRIV("My Service: Processing request, size=%d\n", input_size);
```

### Common Issues
1. **Build Errors**: Ensure all manifest files are properly formatted YAML
2. **Link Errors**: Check that partition is added to manifest_list.yaml
3. **Runtime Errors**: Verify SID uniqueness and proper PSA API usage
4. **Memory Issues**: Check stack size in manifest file

### DEV_MODE Support
```c
#ifdef DEV_MODE
static void debug_print_buffer(const uint8_t* buffer, size_t size)
{
    INFO_UNPRIV("Debug buffer (%d bytes): ", size);
    for (size_t i = 0; i < size && i < 16; i++) {
        INFO_UNPRIV("%02x ", buffer[i]);
    }
    INFO_UNPRIV("\n");
}
#endif
```

## Advanced Features

### Using PSA Crypto Services
```c
#include "psa/crypto.h"

static psa_status_t encrypt_data(const uint8_t* plaintext, size_t plaintext_len,
                                 uint8_t* ciphertext, size_t ciphertext_size)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;
    size_t output_length;
    
    /* Initialize crypto */
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    /* Configure key attributes */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CTR);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    
    /* Generate key */
    status = psa_generate_key(&attributes, &key_id);
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    /* Encrypt data */
    status = psa_cipher_encrypt(key_id, PSA_ALG_CTR,
                                plaintext, plaintext_len,
                                ciphertext, ciphertext_size, &output_length);
    
    /* Clean up */
    psa_destroy_key(key_id);
    return status;
}
```

### Integration with TinyMaix
- See [TinyMaix Integration Guide](./tinymaix-integration.md) for neural network services
- Use model encryption for secure ML inference
- Implement proper resource management for ML operations

## Next Steps

- **[PSA API Reference](./psa-api.md)**: Detailed PSA API usage patterns
- **[TinyMaix Integration](./tinymaix-integration.md)**: ML-specific partition development
- **[Testing Framework](./testing-framework.md)**: Comprehensive testing strategies