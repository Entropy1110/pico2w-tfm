# PSA API Usage Guide

## Overview

Platform Security Architecture (PSA) provides standardized APIs for secure communication between non-secure and secure environments. This guide covers PSA client-service patterns used in the TF-M TinyMaix project.

## PSA Communication Model

### Client-Service Architecture
```
Non-Secure Client              Secure Service
      │                             │
      ▼                             ▼
┌─────────────┐               ┌─────────────┐
│ psa_connect │──────────────▶│ PSA_IPC_    │
│             │               │ CONNECT     │
└─────────────┘               └─────────────┘
      │                             │
      ▼                             ▼
┌─────────────┐               ┌─────────────┐
│ psa_call    │──────────────▶│ Service     │
│             │               │ Handler     │
└─────────────┘               └─────────────┘
      │                             │
      ▼                             ▼
┌─────────────┐               ┌─────────────┐
│ psa_close   │──────────────▶│ PSA_IPC_    │
│             │               │ DISCONNECT  │
└─────────────┘               └─────────────┘
```

### Communication Types

#### 1. Connection-based Services
- **State**: Maintain state between calls
- **Lifecycle**: connect → call(s) → close
- **Use Cases**: Complex services with context
- **Examples**: TinyMaix inference, Echo service

#### 2. Stateless Services  
- **State**: No state maintained
- **Lifecycle**: call only
- **Use Cases**: Simple operations
- **Examples**: Some crypto operations

## Client-Side PSA API

### Basic Connection Pattern
```c
#include "psa/client.h"

tfm_service_status_t call_secure_service(void)
{
    psa_handle_t handle;
    psa_status_t status;
    
    /* Step 1: Connect to service */
    handle = psa_connect(SERVICE_SID, SERVICE_VERSION);
    if (handle <= 0) {
        return SERVICE_ERROR_CONNECTION_FAILED;
    }
    
    /* Step 2: Call service operation */
    status = psa_call(handle, OPERATION_TYPE, NULL, 0, NULL, 0);
    
    /* Step 3: Close connection */
    psa_close(handle);
    
    if (status != PSA_SUCCESS) {
        return SERVICE_ERROR_CALL_FAILED;
    }
    
    return SERVICE_SUCCESS;
}
```

### Data Transfer Patterns

#### Input Data Transfer
```c
tfm_service_status_t send_data_to_service(const uint8_t* input_data, 
                                          size_t input_size)
{
    psa_handle_t handle;
    psa_status_t status;
    
    /* Connect to service */
    handle = psa_connect(SERVICE_SID, 1);
    if (handle <= 0) {
        return SERVICE_ERROR_CONNECTION_FAILED;
    }
    
    /* Setup input vector */
    psa_invec input_vec[] = {
        {.base = input_data, .len = input_size}
    };
    
    /* Call service with input data */
    status = psa_call(handle, OPERATION_WITH_INPUT, 
                      input_vec, 1,  /* 1 input vector */
                      NULL, 0);      /* No output vectors */
    
    psa_close(handle);
    return (status == PSA_SUCCESS) ? SERVICE_SUCCESS : SERVICE_ERROR_CALL_FAILED;
}
```

#### Output Data Transfer
```c
tfm_service_status_t get_data_from_service(uint8_t* output_data, 
                                           size_t output_size,
                                           size_t* actual_size)
{
    psa_handle_t handle;
    psa_status_t status;
    
    /* Connect to service */
    handle = psa_connect(SERVICE_SID, 1);
    if (handle <= 0) {
        return SERVICE_ERROR_CONNECTION_FAILED;
    }
    
    /* Setup output vector */
    psa_outvec output_vec[] = {
        {.base = output_data, .len = output_size}
    };
    
    /* Call service to get output data */
    status = psa_call(handle, OPERATION_WITH_OUTPUT,
                      NULL, 0,       /* No input vectors */
                      output_vec, 1); /* 1 output vector */
    
    psa_close(handle);
    
    if (status == PSA_SUCCESS) {
        *actual_size = output_vec[0].len; /* Actual bytes written */
        return SERVICE_SUCCESS;
    }
    
    return SERVICE_ERROR_CALL_FAILED;
}
```

#### Bidirectional Data Transfer
```c
tfm_service_status_t process_data(const uint8_t* input_data, size_t input_size,
                                  uint8_t* output_data, size_t output_size)
{
    psa_handle_t handle;
    psa_status_t status;
    
    /* Connect to service */
    handle = psa_connect(SERVICE_SID, 1);
    if (handle <= 0) {
        return SERVICE_ERROR_CONNECTION_FAILED;
    }
    
    /* Setup input and output vectors */
    psa_invec input_vec[] = {
        {.base = input_data, .len = input_size}
    };
    
    psa_outvec output_vec[] = {
        {.base = output_data, .len = output_size}
    };
    
    /* Call service with both input and output */
    status = psa_call(handle, OPERATION_PROCESS_DATA,
                      input_vec, 1,   /* 1 input vector */
                      output_vec, 1); /* 1 output vector */
    
    psa_close(handle);
    return (status == PSA_SUCCESS) ? SERVICE_SUCCESS : SERVICE_ERROR_CALL_FAILED;
}
```

### Multiple Data Vectors
```c
tfm_service_status_t complex_operation(const uint8_t* data1, size_t size1,
                                       const uint8_t* data2, size_t size2,
                                       uint8_t* result1, size_t result1_size,
                                       uint8_t* result2, size_t result2_size)
{
    psa_handle_t handle;
    psa_status_t status;
    
    /* Connect to service */
    handle = psa_connect(SERVICE_SID, 1);
    if (handle <= 0) {
        return SERVICE_ERROR_CONNECTION_FAILED;
    }
    
    /* Multiple input vectors */
    psa_invec input_vec[] = {
        {.base = data1, .len = size1},
        {.base = data2, .len = size2}
    };
    
    /* Multiple output vectors */
    psa_outvec output_vec[] = {
        {.base = result1, .len = result1_size},
        {.base = result2, .len = result2_size}
    };
    
    /* Call with multiple vectors */
    status = psa_call(handle, OPERATION_COMPLEX,
                      input_vec, 2,   /* 2 input vectors */
                      output_vec, 2); /* 2 output vectors */
    
    psa_close(handle);
    return (status == PSA_SUCCESS) ? SERVICE_SUCCESS : SERVICE_ERROR_CALL_FAILED;
}
```

## Service-Side PSA API

### Service Entry Point Structure
```c
#include "psa/service.h"
#include "psa_manifest/my_service_manifest.h"

void my_service_entry(void)
{
    psa_msg_t msg;
    psa_status_t status;
    
    /* Service main loop */
    while (1) {
        /* Wait for messages */
        psa_wait(MY_SERVICE_SIGNAL, PSA_BLOCK);
        
        /* Get the message */
        if (psa_get(MY_SERVICE_SIGNAL, &msg) != PSA_SUCCESS) {
            continue;
        }
        
        /* Process message by type */
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                handle_connect(&msg);
                break;
                
            case OPERATION_TYPE_1:
                handle_operation_1(&msg);
                break;
                
            case OPERATION_TYPE_2:
                handle_operation_2(&msg);
                break;
                
            case PSA_IPC_DISCONNECT:
                handle_disconnect(&msg);
                break;
                
            default:
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}
```

### Connection Handling
```c
static void handle_connect(psa_msg_t* msg)
{
    /* Validate connection request */
    if (msg->client_id < 0) {
        /* Reject secure clients if not supported */
        psa_reply(msg->handle, PSA_ERROR_CONNECTION_REFUSED);
        return;
    }
    
    /* Initialize service state for this client (if needed) */
    /* ... service-specific initialization ... */
    
    /* Accept connection */
    psa_reply(msg->handle, PSA_SUCCESS);
}

static void handle_disconnect(psa_msg_t* msg)
{
    /* Cleanup service state for this client (if needed) */
    /* ... service-specific cleanup ... */
    
    /* Acknowledge disconnection */
    psa_reply(msg->handle, PSA_SUCCESS);
}
```

### Input Data Handling
```c
static void handle_operation_with_input(psa_msg_t* msg)
{
    uint8_t input_buffer[MAX_INPUT_SIZE];
    size_t bytes_read;
    psa_status_t status = PSA_SUCCESS;
    
    /* Validate input size */
    if (msg->in_size[0] > MAX_INPUT_SIZE) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto reply;
    }
    
    /* Read input data */
    if (msg->in_size[0] > 0) {
        bytes_read = psa_read(msg->handle, 0, input_buffer, msg->in_size[0]);
        if (bytes_read != msg->in_size[0]) {
            status = PSA_ERROR_COMMUNICATION_FAILURE;
            goto reply;
        }
        
        /* Process input data */
        status = process_input_data(input_buffer, bytes_read);
    }
    
reply:
    psa_reply(msg->handle, status);
}
```

### Output Data Handling  
```c
static void handle_operation_with_output(psa_msg_t* msg)
{
    uint8_t output_buffer[MAX_OUTPUT_SIZE];
    size_t output_length;
    psa_status_t status;
    
    /* Generate output data */
    status = generate_output_data(output_buffer, sizeof(output_buffer), 
                                  &output_length);
    if (status != PSA_SUCCESS) {
        goto reply;
    }
    
    /* Write output data if client provided buffer */
    if (msg->out_size[0] > 0) {
        size_t bytes_to_write = MIN(output_length, msg->out_size[0]);
        psa_write(msg->handle, 0, output_buffer, bytes_to_write);
    }
    
reply:
    psa_reply(msg->handle, status);
}
```

### Complex Data Processing
```c
static void handle_complex_operation(psa_msg_t* msg)
{
    uint8_t input_buffer1[MAX_INPUT1_SIZE];
    uint8_t input_buffer2[MAX_INPUT2_SIZE];
    uint8_t output_buffer1[MAX_OUTPUT1_SIZE];
    uint8_t output_buffer2[MAX_OUTPUT2_SIZE];
    size_t bytes_read;
    psa_status_t status = PSA_SUCCESS;
    
    /* Read first input */
    if (msg->in_size[0] > 0) {
        if (msg->in_size[0] > MAX_INPUT1_SIZE) {
            status = PSA_ERROR_INVALID_ARGUMENT;
            goto reply;
        }
        
        bytes_read = psa_read(msg->handle, 0, input_buffer1, msg->in_size[0]);
        if (bytes_read != msg->in_size[0]) {
            status = PSA_ERROR_COMMUNICATION_FAILURE;
            goto reply;
        }
    }
    
    /* Read second input */
    if (msg->in_size[1] > 0) {
        if (msg->in_size[1] > MAX_INPUT2_SIZE) {
            status = PSA_ERROR_INVALID_ARGUMENT;
            goto reply;
        }
        
        bytes_read = psa_read(msg->handle, 1, input_buffer2, msg->in_size[1]);
        if (bytes_read != msg->in_size[1]) {
            status = PSA_ERROR_COMMUNICATION_FAILURE;
            goto reply;
        }
    }
    
    /* Process data */
    status = process_complex_data(input_buffer1, msg->in_size[0],
                                  input_buffer2, msg->in_size[1],
                                  output_buffer1, sizeof(output_buffer1),
                                  output_buffer2, sizeof(output_buffer2));
    if (status != PSA_SUCCESS) {
        goto reply;
    }
    
    /* Write outputs */
    if (msg->out_size[0] > 0) {
        psa_write(msg->handle, 0, output_buffer1, 
                  MIN(sizeof(output_buffer1), msg->out_size[0]));
    }
    
    if (msg->out_size[1] > 0) {
        psa_write(msg->handle, 1, output_buffer2,
                  MIN(sizeof(output_buffer2), msg->out_size[1]));
    }
    
reply:
    psa_reply(msg->handle, status);
}
```

## Real-World Examples

### TinyMaix Inference Service

#### Client Side
```c
/* Load encrypted model */
tfm_tinymaix_status_t tfm_tinymaix_load_encrypted_model(void)
{
    psa_handle_t handle;
    psa_status_t status;
    uint32_t result = 1;
    
    /* Connect to TinyMaix service */
    handle = psa_connect(TFM_TINYMAIX_INFERENCE_SID, 1);
    if (handle <= 0) {
        return TINYMAIX_STATUS_ERROR_GENERIC;
    }
    
    /* Setup output vector for result */
    psa_outvec out_vec[] = {
        {.base = &result, .len = sizeof(result)}
    };
    
    /* Call load model operation */
    status = psa_call(handle, TINYMAIX_IPC_LOAD_ENCRYPTED_MODEL, 
                      NULL, 0, out_vec, 1);
    
    psa_close(handle);
    
    if (status != PSA_SUCCESS || result != 0) {
        return TINYMAIX_STATUS_ERROR_MODEL_LOAD_FAILED;
    }
    
    return TINYMAIX_STATUS_SUCCESS;
}

/* Run inference with custom data */
tfm_tinymaix_status_t tfm_tinymaix_run_inference_with_data(
    const uint8_t* image_data, size_t image_size, int* predicted_class)
{
    psa_handle_t handle;
    psa_status_t status;
    int result = -1;
    
    if (!image_data || !predicted_class) {
        return TINYMAIX_STATUS_ERROR_INVALID_PARAM;
    }
    
    /* Connect to service */
    handle = psa_connect(TFM_TINYMAIX_INFERENCE_SID, 1);
    if (handle <= 0) {
        return TINYMAIX_STATUS_ERROR_GENERIC;
    }
    
    /* Setup input and output vectors */
    psa_invec in_vec[] = {
        {.base = image_data, .len = image_size}
    };
    
    psa_outvec out_vec[] = {
        {.base = &result, .len = sizeof(result)}
    };
    
    /* Call inference operation */
    status = psa_call(handle, TINYMAIX_IPC_RUN_INFERENCE,
                      in_vec, 1, out_vec, 1);
    
    psa_close(handle);
    
    if (status != PSA_SUCCESS || result < 0) {
        return TINYMAIX_STATUS_ERROR_INFERENCE_FAILED;
    }
    
    *predicted_class = result;
    return TINYMAIX_STATUS_SUCCESS;
}
```

#### Service Side
```c
/* TinyMaix service message handler */
void tinymaix_inference_entry(void)
{
    psa_msg_t msg;
    psa_status_t status;
    
    while (1) {
        psa_wait(TFM_TINYMAIX_INFERENCE_SIGNAL, PSA_BLOCK);
        
        if (psa_get(TFM_TINYMAIX_INFERENCE_SIGNAL, &msg) != PSA_SUCCESS) {
            continue;
        }
        
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case TINYMAIX_IPC_LOAD_ENCRYPTED_MODEL:
                status = handle_load_model(&msg);
                psa_reply(msg.handle, status);
                break;
                
            case TINYMAIX_IPC_RUN_INFERENCE:
                status = handle_run_inference(&msg);
                psa_reply(msg.handle, status);
                break;
                
            case PSA_IPC_DISCONNECT:
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            default:
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

static psa_status_t handle_run_inference(psa_msg_t* msg)
{
    uint8_t image_data[28*28];
    size_t bytes_read;
    int result = -1;
    
    /* Check if model is loaded */
    if (!g_model_loaded) {
        return PSA_ERROR_BAD_STATE;
    }
    
    /* Read image data if provided */
    if (msg->in_size[0] == 28*28) {
        bytes_read = psa_read(msg->handle, 0, image_data, msg->in_size[0]);
        if (bytes_read != msg->in_size[0]) {
            return PSA_ERROR_COMMUNICATION_FAILURE;
        }
        
        /* Run inference with custom image */
        result = run_tinymaix_inference(image_data);
    } else if (msg->in_size[0] == 0) {
        /* Use built-in test image */
        result = run_tinymaix_inference(built_in_test_image);
    } else {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* Write result if output buffer provided */
    if (msg->out_size[0] >= sizeof(int) && result >= 0) {
        psa_write(msg->handle, 0, &result, sizeof(result));
    }
    
    return (result >= 0) ? PSA_SUCCESS : PSA_ERROR_GENERIC_ERROR;
}
```

### Echo Service Example

#### Client Side
```c
tfm_echo_status_t tfm_echo_call(const uint8_t* input, size_t input_len,
                                uint8_t* output, size_t output_len)
{
    psa_handle_t handle;
    psa_status_t status;
    
    /* Connect to echo service */
    handle = psa_connect(TFM_ECHO_SID, 1);
    if (handle <= 0) {
        return ECHO_STATUS_ERROR_GENERIC;
    }
    
    /* Setup vectors */
    psa_invec in_vec[] = {{.base = input, .len = input_len}};
    psa_outvec out_vec[] = {{.base = output, .len = output_len}};
    
    /* Call echo operation */
    status = psa_call(handle, ECHO_IPC_CALL, in_vec, 1, out_vec, 1);
    
    psa_close(handle);
    
    return (status == PSA_SUCCESS) ? ECHO_STATUS_SUCCESS : ECHO_STATUS_ERROR_GENERIC;
}
```

#### Service Side
```c
void echo_service_entry(void)
{
    psa_msg_t msg;
    
    while (1) {
        psa_wait(TFM_ECHO_SIGNAL, PSA_BLOCK);
        
        if (psa_get(TFM_ECHO_SIGNAL, &msg) != PSA_SUCCESS) {
            continue;
        }
        
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case ECHO_IPC_CALL:
                handle_echo_call(&msg);
                break;
                
            case PSA_IPC_DISCONNECT:
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            default:
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

static void handle_echo_call(psa_msg_t* msg)
{
    uint8_t echo_buffer[ECHO_MAX_SIZE];
    size_t bytes_read;
    
    /* Read input data */
    if (msg->in_size[0] > 0 && msg->in_size[0] <= ECHO_MAX_SIZE) {
        bytes_read = psa_read(msg->handle, 0, echo_buffer, msg->in_size[0]);
        
        /* Echo data back if output buffer available */
        if (msg->out_size[0] > 0) {
            size_t bytes_to_write = MIN(bytes_read, msg->out_size[0]);
            psa_write(msg->handle, 0, echo_buffer, bytes_to_write);
        }
        
        psa_reply(msg->handle, PSA_SUCCESS);
    } else {
        psa_reply(msg->handle, PSA_ERROR_INVALID_ARGUMENT);
    }
}
```

## Error Handling

### PSA Status Codes
```c
/* Common PSA status codes */
#define PSA_SUCCESS                     0
#define PSA_ERROR_GENERIC_ERROR         -132
#define PSA_ERROR_NOT_SUPPORTED         -134
#define PSA_ERROR_INVALID_ARGUMENT      -135
#define PSA_ERROR_INVALID_HANDLE        -136
#define PSA_ERROR_BAD_STATE             -137
#define PSA_ERROR_BUFFER_TOO_SMALL      -138
#define PSA_ERROR_COMMUNICATION_FAILURE -145
#define PSA_ERROR_CONNECTION_REFUSED    -150
#define PSA_ERROR_CONNECTION_BUSY       -151
```

### Error Handling Patterns
```c
tfm_service_status_t robust_service_call(void)
{
    psa_handle_t handle;
    psa_status_t status;
    int retry_count = 0;
    const int max_retries = 3;
    
    /* Retry loop for transient errors */
    while (retry_count < max_retries) {
        /* Connect to service */
        handle = psa_connect(SERVICE_SID, 1);
        if (handle <= 0) {
            if (handle == PSA_ERROR_CONNECTION_BUSY && retry_count < max_retries - 1) {
                /* Retry on busy */
                retry_count++;
                continue;
            }
            return SERVICE_ERROR_CONNECTION_FAILED;
        }
        
        /* Call service */
        status = psa_call(handle, OPERATION_TYPE, NULL, 0, NULL, 0);
        
        /* Close connection */
        psa_close(handle);
        
        /* Handle status */
        switch (status) {
            case PSA_SUCCESS:
                return SERVICE_SUCCESS;
                
            case PSA_ERROR_COMMUNICATION_FAILURE:
                /* Retry communication failures */
                if (retry_count < max_retries - 1) {
                    retry_count++;
                    continue;
                }
                return SERVICE_ERROR_COMMUNICATION;
                
            case PSA_ERROR_INVALID_ARGUMENT:
                /* Don't retry invalid arguments */
                return SERVICE_ERROR_INVALID_PARAM;
                
            case PSA_ERROR_NOT_SUPPORTED:
                return SERVICE_ERROR_NOT_SUPPORTED;
                
            default:
                return SERVICE_ERROR_GENERIC;
        }
    }
    
    return SERVICE_ERROR_MAX_RETRIES_EXCEEDED;
}
```

## Best Practices

### Client-Side Best Practices

#### 1. Resource Management
```c
/* Always close connections */
psa_handle_t handle = psa_connect(SERVICE_SID, 1);
if (handle > 0) {
    /* ... use service ... */
    psa_close(handle);  /* Always close */
}

/* Check return values */
psa_status_t status = psa_call(handle, op, in_vec, 1, out_vec, 1);
if (status != PSA_SUCCESS) {
    /* Handle error appropriately */
}
```

#### 2. Buffer Management
```c
/* Validate buffer sizes */
if (input_size > MAX_SERVICE_INPUT) {
    return SERVICE_ERROR_BUFFER_TOO_LARGE;
}

/* Use appropriate buffer sizes */
uint8_t buffer[SERVICE_MAX_BUFFER_SIZE];
psa_outvec out_vec[] = {{.base = buffer, .len = sizeof(buffer)}};
```

#### 3. Error Recovery
```c
/* Implement retry logic for transient errors */
/* Validate inputs before making PSA calls */
/* Provide meaningful error messages to callers */
```

### Service-Side Best Practices

#### 1. Input Validation
```c
static psa_status_t handle_operation(psa_msg_t* msg)
{
    /* Validate input sizes */
    if (msg->in_size[0] > MAX_ALLOWED_INPUT) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* Validate client permissions */
    if (msg->client_id < 0 && !service_allows_secure_clients) {
        return PSA_ERROR_CONNECTION_REFUSED;
    }
    
    /* ... process request ... */
}
```

#### 2. Resource Management
```c
/* Use static allocation in secure partitions */
static uint8_t service_buffer[SERVICE_BUFFER_SIZE];

/* Clean up resources on disconnect */
static void handle_disconnect(psa_msg_t* msg)
{
    cleanup_client_resources(msg->client_id);
    psa_reply(msg->handle, PSA_SUCCESS);
}
```

#### 3. Security Considerations
```c
/* Clear sensitive data after use */
memset(sensitive_buffer, 0, sizeof(sensitive_buffer));

/* Validate data integrity */
if (!validate_input_integrity(input_data, input_size)) {
    return PSA_ERROR_INVALID_ARGUMENT;
}

/* Fail securely */
if (security_violation_detected) {
    /* Log security event */
    /* Return generic error */
    return PSA_ERROR_GENERIC_ERROR;
}
```

## Performance Optimization

### Minimize Context Switches
```c
/* BAD: Multiple separate calls */
status1 = call_service_operation_1();
status2 = call_service_operation_2();
status3 = call_service_operation_3();

/* GOOD: Batch operations */
status = call_service_batch_operations(op1_data, op2_data, op3_data);
```

### Efficient Data Transfer
```c
/* Use multiple vectors for complex data */
psa_invec input_vec[] = {
    {.base = header, .len = sizeof(header)},
    {.base = payload, .len = payload_size}
};

/* Avoid unnecessary data copying */
/* Use shared buffers for large transfers when possible */
```

### Connection Reuse
```c
/* For multiple operations, consider keeping connection open */
handle = psa_connect(SERVICE_SID, 1);
for (int i = 0; i < num_operations; i++) {
    status = psa_call(handle, operations[i], ...);
    /* ... handle status ... */
}
psa_close(handle);
```

## Next Steps

- **[Secure Partitions Guide](./secure-partitions.md)**: Implement secure services
- **[TinyMaix Integration](./tinymaix-integration.md)**: ML-specific PSA usage
- **[Testing Framework](./testing-framework.md)**: Test PSA client-service pairs