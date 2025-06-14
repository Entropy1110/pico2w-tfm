# Secure Partitions Development Guide

## 1. Overview

Secure Partitions (SPs) are the fundamental building blocks for creating isolated, secure services within the Trusted Firmware-M (TF-M) framework. Each SP operates in its own sandboxed environment in the Secure Processing Environment (SPE), offering its functionalities to the Non-secure Processing Environment (NSPE) or other SPs through well-defined PSA Functional APIs.

This guide provides a comprehensive walkthrough for developing, integrating, and testing Secure Partitions in the `pico2w-tfm-tflm` project.

## 2. Partition Architecture

### 2.1. Partition Types in TF-M

TF-M categorizes partitions based on their role and trust level:

```
TF-M Partition Types:
┌─────────────────────────────────────┐
│           PSA-ROT                   │  ← Platform Root of Trust (Highest Trust)
│  • PSA Crypto Service               │    (Manages core cryptographic operations)
│  • PSA Protected Storage Service    │    (Manages secure data storage)
│  • PSA Internal Trusted Storage     │    (Manages internal secure data)
│  • PSA Initial Attestation Service  │    (Provides device identity evidence)
│  • Platform Services (e.g., Reset)  │    (Core platform functionalities)
├─────────────────────────────────────┤
│        APPLICATION-ROT              │  ← Application Root of Trust (Application-specific Trust)
│  • Echo Service (PID 444)           │    (Example service for basic IPC)
│  • TinyMaix Inference (PID 445)     │    (Service for running ML models)
│  • Custom Secure Services           │    (User-defined application services)
└─────────────────────────────────────┘
```

-   **PSA Root of Trust (PSA-ROT):** These are foundational services provided by TF-M itself, forming the core security capabilities of the system. They are highly privileged.
-   **Application Root of Trust (APP-ROT):** These are services developed for specific application needs. While still running in the SPE, they typically have fewer privileges than PSA-ROT services and are tailored to particular use cases, like the ML inference service in this project.

### 2.2. Memory Isolation

A key security feature of TF-M is the strict memory isolation between partitions, enforced by the Secure Memory Protection Unit (S-MPU) or equivalent hardware. Each partition has its own dedicated stack, heap, and code regions, preventing unauthorized access from other partitions or the NSPE.

```
Conceptual Memory Layout for Partitions:
┌─────────────────────────────────────┐
│        Partition N Stack            │
├─────────────────────────────────────┤
│        Partition N Heap             │
├─────────────────────────────────────┤
│        Partition N Code & RO Data   │
├─────────────────────────────────────┤
│           PSA IPC Router            │ (Handles inter-partition communication)
├─────────────────────────────────────┤
│        Partition 1 Code & RO Data   │
├─────────────────────────────────────┤
│        Partition 1 Heap             │
├─────────────────────────────────────┤
│        Partition 1 Stack            │
└─────────────────────────────────────┘
```
This isolation ensures that a fault or compromise in one partition does not directly affect others, enhancing overall system robustness and security.

## 3. Existing Partitions in `pico2w-tfm-tflm`

This project includes pre-built APP-ROT partitions:

-   **Echo Service** (PID: 444, SID: `0x00000105`): A simple service that echoes back data received from the NSPE. Useful for testing basic PSA IPC mechanisms.
-   **TinyMaix Inference Service** (PID: 445, SID: `0x00000107`): A service that performs neural network inference using the TinyMaix library. It handles model loading (including decryption) and execution.

### 3.1. Echo Service Deep Dive

#### 3.1.1. Manifest File: `partitions/echo_service/echo_service_manifest.yaml`

The manifest file defines the properties and requirements of the partition.

```yaml
{
  "description": "TF-M Echo Service",
  "psa_framework_version": "1.1",
  "name": "TFM_SP_ECHO_SERVICE",        # Unique name for the partition
  "type": "APPLICATION-ROT",           # Partition type
  "priority": "NORMAL",                # Execution priority
  "id": "0x00000105",                  # Partition ID (matches SID for single-service partitions)
  "entry_point": "tfm_echo_service_main", # Function to be called at startup
  "stack_size": "0x1000",              # Stack size (e.g., 4KB)
  "services": [
    {
      "name": "TFM_ECHO_SERVICE",       # Service name
      "sid": "0x00000105",             # Service ID (globally unique)
      "connection_based": true,        # Requires psa_connect/psa_close
      "non_secure_clients": true,      # Accessible from NSPE
      "version": 1,
      "version_policy": "STRICT"      # Strict version matching
    }
  ],
  "mmio_regions": [],                  # No direct hardware access needed
  "irqs": [],                          # No interrupts handled by this partition
  "linker_pattern": {
    "library_list": []
  }
}
```

#### 3.1.2. Service Implementation: `partitions/echo_service/echo_service.c`

```c
#include "tfm_sp_log.h" // For logging (use LOG_INFSIG, LOG_DBGSIG, LOG_ERRSIG)
#include "psa/service.h"
#include "psa_manifest/tfm_echo_service.h" // Auto-generated from manifest

#define ECHO_BUFFER_SIZE 256 // Define a reasonable buffer size

// Forward declaration
static void handle_echo_request(psa_msg_t *msg);

/* Echo Service main entry point, called by TF-M scheduler */
void tfm_echo_service_main(void)
{
    psa_msg_t msg;

    LOG_INFSIG("[Echo Service] Started and waiting for messages...\r\n");

    while (1) {
        /* Wait for any message to arrive - this is a blocking call */
        psa_status_t status = psa_get(TFM_ECHO_SERVICE_SIGNAL, &msg); // Use signal from manifest
        if (status != PSA_SUCCESS) {
            LOG_ERRSIG("[Echo Service] Failed to get message: %d\r\n", status);
            continue; // Or handle error more robustly
        }

        /* Process the message based on its type */
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                LOG_INFSIG("[Echo Service] Client connected (handle: %d)\r\n", msg.handle);
                psa_reply(msg.handle, PSA_SUCCESS); // Acknowledge connection
                break;

            case PSA_IPC_CALL: // This is where actual service requests are handled
                LOG_DBGSIG("[Echo Service] Received CALL (rhandle: %p)\r\n", msg.rhandle);
                handle_echo_request(&msg);
                break;

            case PSA_IPC_DISCONNECT:
                LOG_INFSIG("[Echo Service] Client disconnected (handle: %d)\r\n", msg.handle);
                psa_reply(msg.handle, PSA_SUCCESS); // Acknowledge disconnection
                break;

            default:
                LOG_ERRSIG("[Echo Service] Unknown message type: %d\r\n", msg.type);
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

/* Handles the actual echo operation for a PSA_IPC_CALL message */
static void handle_echo_request(psa_msg_t *msg)
{
    uint8_t input_buffer[ECHO_BUFFER_SIZE];
    uint8_t output_buffer[ECHO_BUFFER_SIZE]; // In this case, same as input
    size_t bytes_read;
    psa_status_t status = PSA_SUCCESS;

    /* Check if input and output buffers are provided by the client */
    if (msg->in_size[0] == 0) {
        LOG_ERRSIG("[Echo Service] No input data provided.\r\n");
        psa_reply(msg.handle, PSA_ERROR_INVALID_ARGUMENT);
        return;
    }

    /* Read data from the client's input buffer (in_vec[0]) */
    bytes_read = psa_read(msg->handle, 0, input_buffer, MIN(msg->in_size[0], ECHO_BUFFER_SIZE));
    if (bytes_read == 0 && msg->in_size[0] > 0) { // psa_read returns 0 on error or if size is 0
        LOG_ERRSIG("[Echo Service] Failed to read input data.\r\n");
        psa_reply(msg.handle, PSA_ERROR_COMMUNICATION_FAILURE);
        return;
    }

    LOG_DBGSIG("[Echo Service] Read %d bytes from client.\r\n", bytes_read);

    /* Perform the "echo" operation: copy input to output */
    memcpy(output_buffer, input_buffer, bytes_read);

    /* Write the echoed data back to the client's output buffer (out_vec[0]) */
    if (msg->out_size[0] > 0) {
        psa_write(msg->handle, 0, output_buffer, MIN(bytes_read, msg->out_size[0]));
    } else {
        LOG_DBGSIG("[Echo Service] Client did not provide an output buffer.\r\n");
    }

    /* Reply to the client indicating success or failure */
    psa_reply(msg.handle, status);
    LOG_INFSIG("[Echo Service] Echoed %d bytes.\r\n", bytes_read);
}

```

#### 3.1.3. CMake Build Configuration: `partitions/echo_service/CMakeLists.txt`

```cmake
# CMake configuration for the Echo Service partition
if (NOT TFM_PARTITION_ECHO_SERVICE)
    return()
endif()

# Create a static library for this partition
add_library(tfm_app_rot_partition_echo_service STATIC)

# Specify source files for the library
target_sources(tfm_app_rot_partition_echo_service
    PRIVATE
        echo_service.c
)

# Specify include directories
target_include_directories(tfm_app_rot_partition_echo_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} # For local headers
        ${CMAKE_SOURCE_DIR}/interface/include # For PSA client API headers
        # Add other necessary TF-M core include paths if needed
)

# Link against necessary TF-M libraries
target_link_libraries(tfm_app_rot_partition_echo_service
    PRIVATE
        tfm_secure_api  # For PSA client calls from SPE (if any)
        tfm_sprt        # For PSA service runtime support (psa_get, psa_reply etc.)
        platform_s      # For platform-specific secure functionalities
        tfm_sp_log_s    # For secure logging utilities
)

# Define compile-time definitions specific to this partition
target_compile_definitions(tfm_app_rot_partition_echo_service
    PRIVATE
        TFM_PARTITION_ECHO_SERVICE # To enable conditional compilation
)

# Add this partition to the global list of TF-M partitions
# This is typically handled by TF-M's build system using the manifest
# and ensuring this CMakeLists.txt is included by a higher-level CMake file.
```

### 3.2. TinyMaix Inference Service Deep Dive

This service is more complex, involving ML model handling and cryptographic operations for model decryption.

#### 3.2.1. Manifest File: `partitions/tinymaix_inference/tinymaix_inference_manifest.yaml`

```yaml
{
  "description": "TF-M TinyMaix Inference Service",
  "psa_framework_version": "1.1",
  "name": "TFM_SP_TINYMAIX_INFERENCE",
  "type": "APPLICATION-ROT",
  "priority": "NORMAL",
  "id": "0x00000107", // Unique Partition ID
  "entry_point": "tfm_tinymaix_inference_main",
  "stack_size": "0x2000",  // 8KB - Larger stack for ML operations and buffers
  "services": [
    {
      "name": "TFM_TINYMAIX_INFERENCE_SERVICE",
      "sid": "0x00000107", // Unique Service ID
      "connection_based": true,
      "non_secure_clients": true,
      "version": 1,
      "version_policy": "STRICT"
    }
  ],
  "mmio_regions": [], // No direct hardware access
  "irqs": [],         // No interrupts handled
  "linker_pattern": {
    "library_list": [
      "*tinymaix*"  // Ensures TinyMaix library code is linked into this partition
    ]
  },
  "dependencies": [
    "TFM_CRYPTO" // Depends on the PSA Crypto service for model decryption
  ]
}
```
Key differences from Echo Service manifest:
- Larger `stack_size` for ML data.
- `linker_pattern` to include TinyMaix library symbols.
- `dependencies` field to declare reliance on `TFM_CRYPTO` service.

#### 3.2.2. Service Implementation: `partitions/tinymaix_inference/tinymaix_inference.c`

This file implements the logic for loading, decrypting, and running TinyMaix models.

```c
#include "tfm_sp_log.h"
#include "psa/service.h"
#include "psa/crypto.h"
#include "psa_manifest/tfm_tinymaix_inference.h"
#include "tinymaix.h" // TinyMaix core library
#include "encrypted_mnist_model_psa.h" // Contains the encrypted model data
#include "tfm_huk_deriv_srv_api.h" // For HUK-derived key (conceptual)

// Define IPC message types for different operations
#define TINYMAIX_IPC_LOAD_MODEL     (PSA_IPC_CALL + 1) // Or use rhandle for custom types
#define TINYMAIX_IPC_SET_INPUT      (PSA_IPC_CALL + 2)
#define TINYMAIX_IPC_RUN_INFERENCE  (PSA_IPC_CALL + 3)
#define TINYMAIX_IPC_GET_OUTPUT     (PSA_IPC_CALL + 4)

#ifdef DEV_MODE
#define TINYMAIX_IPC_GET_MODEL_KEY  (PSA_IPC_CALL + 5) // For debugging
#endif

// Model and buffer sizes (should be defined appropriately)
#define MODEL_BUFFER_SIZE (100 * 1024) // 100KB example for model data
#define INPUT_BUFFER_SIZE (1 * 28 * 28) // MNIST example: 28x28 grayscale image
#define OUTPUT_BUFFER_SIZE (10)         // MNIST example: 10 class scores
#define DERIVED_KEY_LEN 16              // AES-128 key

// Global state for the service (consider thread-safety if not connection_based)
static bool g_model_loaded = false;
static tm_mdl_t g_model; // TinyMaix model structure
static uint8_t g_model_buffer[MODEL_BUFFER_SIZE]; // Buffer for TinyMaix internal use
static uint8_t g_input_buffer[INPUT_BUFFER_SIZE];
static uint8_t g_output_buffer[OUTPUT_BUFFER_SIZE];

// Forward declarations
static void handle_tinymaix_request(psa_msg_t *msg);
static psa_status_t handle_load_model(psa_msg_t *msg);
static psa_status_t handle_set_input(psa_msg_t *msg);
static psa_status_t handle_run_inference(psa_msg_t *msg);
static psa_status_t handle_get_output(psa_msg_t *msg);
#ifdef DEV_MODE
static psa_status_t handle_get_model_key(psa_msg_t *msg);
#endif
static psa_status_t derive_key_from_huk(const char* label, uint8_t* key_buffer, size_t key_buffer_size);
static psa_status_t decrypt_model_cbc(const uint8_t* encrypted_model, size_t encrypted_size,
                                    const uint8_t* key, uint8_t* decrypted_buffer, size_t* decrypted_len);

void tfm_tinymaix_inference_main(void)
{
    psa_msg_t msg;
    LOG_INFSIG("[TinyMaix SP] Service Started\r\n");

    // Initialize PSA Crypto client (needed if this SP calls PSA Crypto APIs)
    psa_status_t crypto_status = psa_crypto_init();
    if (crypto_status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Failed to initialize PSA Crypto: %d\r\n", crypto_status);
        // Service cannot function without crypto, might enter a permanent error state or abort.
        // For simplicity, we continue, but a real implementation should handle this robustly.
        // psa_panic(); // Or similar TF-M mechanism if unrecoverable
    }

    while (1) {
        psa_status_t status = psa_get(TFM_TINYMAIX_INFERENCE_SERVICE_SIGNAL, &msg);
        if (status != PSA_SUCCESS) {
            LOG_ERRSIG("[TinyMaix SP] psa_get failed: %d\r\n", status);
            continue;
        }

        switch (msg.type) {
            case PSA_IPC_CONNECT:
                LOG_INFSIG("[TinyMaix SP] Client connected (handle: %d)\r\n", msg.handle);
                psa_reply(msg.handle, PSA_SUCCESS);
                break;

            case PSA_IPC_CALL: // All custom operations are mapped to PSA_IPC_CALL
                               // The specific operation is identified by msg->rhandle or a parameter.
                handle_tinymaix_request(&msg);
                break;

            case PSA_IPC_DISCONNECT:
                LOG_INFSIG("[TinyMaix SP] Client disconnected (handle: %d)\r\n", msg.handle);
                // Clean up resources associated with this connection if any
                g_model_loaded = false; // Example: unload model on disconnect
                psa_reply(msg.handle, PSA_SUCCESS);
                break;

            default:
                LOG_ERRSIG("[TinyMaix SP] Unknown message type: %d\r\n", msg.type);
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

static void handle_tinymaix_request(psa_msg_t *msg)
{
    // Use msg->rhandle (passed as 'type' in psa_call from client) to distinguish operations
    // This is a common way to multiplex different service functions over a single SID.
    uint32_t operation_type = (uint32_t)(uintptr_t)msg->rhandle;
    psa_status_t status = PSA_SUCCESS;

    LOG_DBGSIG("[TinyMaix SP] Handling request type: 0x%x\r\n", operation_type);

    switch (operation_type) {
        case TINYMAIX_IPC_LOAD_MODEL:
            status = handle_load_model(msg);
            break;
        case TINYMAIX_IPC_SET_INPUT:
            status = handle_set_input(msg);
            break;
        case TINYMAIX_IPC_RUN_INFERENCE:
            status = handle_run_inference(msg);
            break;
        case TINYMAIX_IPC_GET_OUTPUT:
            status = handle_get_output(msg);
            break;
#ifdef DEV_MODE
        case TINYMAIX_IPC_GET_MODEL_KEY:
            status = handle_get_model_key(msg);
            break;
#endif
        default:
            LOG_ERRSIG("[TinyMaix SP] Unknown operation type: 0x%x\r\n", operation_type);
            status = PSA_ERROR_NOT_SUPPORTED;
            break;
    }
    psa_reply(msg.handle, status);
}

static psa_status_t handle_load_model(psa_msg_t *msg)
{
    psa_status_t status;
    uint8_t derived_key[DERIVED_KEY_LEN];
    uint8_t decrypted_model_data[MODEL_BUFFER_SIZE]; // Temporary buffer for decrypted model
    size_t decrypted_model_size;

    LOG_INFSIG("[TinyMaix SP] Attempting to load model...\r\n");

    // 1. Derive decryption key from HUK (Hardware Unique Key)
    // The label should be unique and specific to this model/version.
    const char *huk_label = "pico2w-tinymaix-mnist-aes128-v1.0";
    status = derive_key_from_huk(huk_label, derived_key, sizeof(derived_key));
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Failed to derive key from HUK: %d\r\n", status);
        return status;
    }
    LOG_DBGSIG("[TinyMaix SP] Model decryption key derived.\r\n");

    // 2. Decrypt the embedded model data (e.g., encrypted_mnist_model_psa_data)
    // This example assumes encrypted_mnist_model_psa_data and _size are available
    status = decrypt_model_cbc(encrypted_mnist_model_psa_data, encrypted_mnist_model_psa_size,
                               derived_key, decrypted_model_data, &decrypted_model_size);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Failed to decrypt model: %d\r\n", status);
        return status;
    }
    LOG_DBGSIG("[TinyMaix SP] Model decrypted successfully (%d bytes).\r\n", decrypted_model_size);

    // 3. Load the decrypted model into TinyMaix
    // tm_load requires a model data buffer and a temporary working buffer (g_model_buffer)
    tm_stat_t tm_status = tm_load(&g_model, decrypted_model_data, g_model_buffer, sizeof(g_model_buffer), 1);
    if (tm_status != TM_OK) {
        LOG_ERRSIG("[TinyMaix SP] TinyMaix model load failed: %d\r\n", tm_status);
        // Map TinyMaix error to PSA error if possible
        return PSA_ERROR_INVALID_ARGUMENT; // Or a more specific error
    }

    g_model_loaded = true;
    LOG_INFSIG("[TinyMaix SP] Model loaded successfully.\r\n");
    return PSA_SUCCESS;
}

static psa_status_t handle_set_input(psa_msg_t *msg)
{
    if (!g_model_loaded) {
        LOG_ERRSIG("[TinyMaix SP] Set Input: Model not loaded.\r\n");
        return PSA_ERROR_BAD_STATE;
    }
    if (msg->in_size[0] != INPUT_BUFFER_SIZE) {
        LOG_ERRSIG("[TinyMaix SP] Set Input: Invalid input size. Expected %d, got %d\r\n", INPUT_BUFFER_SIZE, msg->in_size[0]);
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    size_t bytes_read = psa_read(msg->handle, 0, g_input_buffer, INPUT_BUFFER_SIZE);
    if (bytes_read != INPUT_BUFFER_SIZE) {
        LOG_ERRSIG("[TinyMaix SP] Set Input: Failed to read input data (%d bytes read).\r\n", bytes_read);
        return PSA_ERROR_COMMUNICATION_FAILURE;
    }

    LOG_INFSIG("[TinyMaix SP] Input data set (%d bytes).\r\n", bytes_read);
    return PSA_SUCCESS;
}

static psa_status_t handle_run_inference(psa_msg_t *msg)
{
    if (!g_model_loaded) {
        LOG_ERRSIG("[TinyMaix SP] Run Inference: Model not loaded.\r\n");
        return PSA_ERROR_BAD_STATE;
    }

    LOG_INFSIG("[TinyMaix SP] Running inference...\r\n");

    // Prepare input for TinyMaix (tm_preprocess can be used if needed)
    // For this example, assume g_input_buffer is already in the correct format.
    tm_mat_t input_matrix = {TM_MAT_TYPE_INT8, INPUT_H, INPUT_W, INPUT_C, {(uint8_t*)g_input_buffer}};
    // Assuming INPUT_H, INPUT_W, INPUT_C are defined for the model (e.g., 28, 28, 1 for MNIST)

    tm_stat_t tm_status = tm_run(&g_model, &input_matrix);
    if (tm_status != TM_OK) {
        LOG_ERRSIG("[TinyMaix SP] TinyMaix inference failed: %d\r\n", tm_status);
        return PSA_ERROR_GENERIC_ERROR; // Or map to a more specific error
    }

    // Output is now in g_model.outp, copy to g_output_buffer if needed for client
    // For simplicity, assume tm_run places output where client can get it via handle_get_output
    // Or, if tm_run directly uses a client-provided buffer via psa_map_outvec, that's also an option.
    // Here, we assume g_model.outp points to the output layer's data.
    // And we need to copy it to our g_output_buffer.
    if (g_model.outp && g_model.outp->data) {
        memcpy(g_output_buffer, g_model.outp->data, MIN(OUTPUT_BUFFER_SIZE, g_model.outp->h * g_model.outp->w * g_model.outp->c * TM_MAT_TYPESIZE(g_model.outp->dtype)));
    } else {
        LOG_ERRSIG("[TinyMaix SP] Inference ran but output pointer is null.\r\n");
        return PSA_ERROR_GENERIC_ERROR;
    }

    LOG_INFSIG("[TinyMaix SP] Inference completed.\r\n");
    return PSA_SUCCESS;
}

static psa_status_t handle_get_output(psa_msg_t *msg)
{
    if (!g_model_loaded) {
        LOG_ERRSIG("[TinyMaix SP] Get Output: Model not loaded.\r\n");
        return PSA_ERROR_BAD_STATE;
    }
    if (msg->out_size[0] < OUTPUT_BUFFER_SIZE) {
        LOG_ERRSIG("[TinyMaix SP] Get Output: Output buffer too small. Expected %d, got %d\r\n", OUTPUT_BUFFER_SIZE, msg->out_size[0]);
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }

    psa_write(msg->handle, 0, g_output_buffer, OUTPUT_BUFFER_SIZE);
    LOG_INFSIG("[TinyMaix SP] Output data sent (%d bytes).\r\n", OUTPUT_BUFFER_SIZE);
    return PSA_SUCCESS;
}

// Helper function to derive key from HUK using PSA Crypto API
// This is a simplified example. A real implementation might use a dedicated HUK derivation service.
static psa_status_t derive_key_from_huk(const char* label, uint8_t* key_buffer, size_t key_buffer_size)
{
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t huk_id = 0; // Placeholder for actual HUK ID or mechanism to get it
                             // In TF-M, HUK is often wrapped by a platform service or derived via psa_generate_key
                             // For this example, we'll try to generate a persistent key based on a label if supported,
                             // or use a volatile key for demonstration.

    // This is a conceptual representation. Actual HUK access is platform-specific.
    // TF-M might provide a specific API or expect a pre-provisioned key ID for the HUK.
    // Let's assume we use PSA key derivation with a pre-existing root key (simulating HUK access).

    // For demonstration, let's try to import a dummy HUK or use a volatile key for derivation.
    // A real system would have a secure way to access the HUK.
    // This example uses psa_generate_key to get a key, then derives from it.
    // A more realistic scenario would be to use a pre-provisioned key ID for the HUK.

    psa_key_id_t base_key_id;
    uint8_t salt[] = "some_salt_for_hkdf"; // Salt should be unique if possible

    // Setup attributes for the base key (simulating HUK)
    psa_set_key_type(&attributes, PSA_KEY_TYPE_DERIVE);
    psa_set_key_bits(&attributes, 256); // Example HUK size
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    // In a real scenario, this key would be persistent and hardware-backed.
    // psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_PERSISTENT);
    // psa_set_key_id(&attributes, YOUR_PREPROVISIONED_HUK_ID);
    // status = psa_import_key(&attributes, huk_raw_material, huk_raw_material_size, &base_key_id);
    // For this example, let's generate a volatile key to act as the base for derivation.
    status = psa_generate_key(&attributes, &base_key_id);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Failed to generate/import base key for HUK derivation: %d\r\n", status);
        return status;
    }

    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    status = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Derivation setup failed: %d\r\n", status);
        psa_destroy_key(base_key_id);
        return status;
    }

    status = psa_key_derivation_input_key(&op, PSA_KEY_DERIVATION_INPUT_SECRET, base_key_id);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Derivation input key failed: %d\r\n", status);
        goto cleanup_derivation;
    }

    status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT, salt, sizeof(salt));
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Derivation input salt failed: %d\r\n", status);
        goto cleanup_derivation;
    }

    status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO, (const uint8_t*)label, strlen(label));
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Derivation input label failed: %d\r\n", status);
        goto cleanup_derivation;
    }

    status = psa_key_derivation_output_bytes(&op, key_buffer, key_buffer_size);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Derivation output bytes failed: %d\r\n", status);
    }

cleanup_derivation:
    psa_key_derivation_abort(&op);
    psa_destroy_key(base_key_id); // Clean up the base key
    return status;
}

// Helper function to decrypt model using AES-128-CBC
static psa_status_t decrypt_model_cbc(const uint8_t* encrypted_model, size_t encrypted_size,
                                    const uint8_t* key, uint8_t* decrypted_buffer, size_t* decrypted_len)
{
    psa_status_t status;
    psa_key_id_t key_id;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
    // First 16 bytes of encrypted_model are IV, rest is ciphertext
    if (encrypted_size <= 16) return PSA_ERROR_INVALID_ARGUMENT;
    const uint8_t* iv = encrypted_model;
    const uint8_t* ciphertext = encrypted_model + 16;
    size_t ciphertext_len = encrypted_size - 16;

    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, PSA_BYTES_TO_BITS(DERIVED_KEY_LEN));
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT));
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING); // Assuming no padding or handled elsewhere

    status = psa_import_key(&attributes, key, DERIVED_KEY_LEN, &key_id);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Failed to import decryption key: %d\r\n", status);
        return status;
    }

    status = psa_cipher_decrypt_setup(&operation, key_id, PSA_ALG_CBC_NO_PADDING);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Cipher decrypt setup failed: %d\r\n", status);
        goto cleanup_key;
    }

    status = psa_cipher_set_iv(&operation, iv, 16);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Cipher set IV failed: %d\r\n", status);
        goto cleanup_op;
    }

    status = psa_cipher_update(&operation, ciphertext, ciphertext_len,
                               decrypted_buffer, MODEL_BUFFER_SIZE, decrypted_len);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Cipher update (decrypt) failed: %d\r\n", status);
        goto cleanup_op;
    }

    size_t final_len;
    status = psa_cipher_finish(&operation, decrypted_buffer + *decrypted_len,
                               MODEL_BUFFER_SIZE - *decrypted_len, &final_len);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] Cipher finish (decrypt) failed: %d\r\n", status);
        goto cleanup_op;
    }
    *decrypted_len += final_len;

cleanup_op:
    psa_cipher_abort(&operation);
cleanup_key:
    psa_destroy_key(key_id);
    return status;
}

#ifdef DEV_MODE
// For debugging: allow NSPE to retrieve the derived key.
// WARNING: THIS IS INSECURE AND FOR DEBUGGING ONLY.
static psa_status_t handle_get_model_key(psa_msg_t *msg)
{
    uint8_t derived_key[DERIVED_KEY_LEN];
    psa_status_t status;
    const char *huk_label = "pico2w-tinymaix-mnist-aes128-v1.0";

    LOG_INFSIG("[TinyMaix SP] DEV_MODE: Getting HUK-derived model key...\r\n");

    status = derive_key_from_huk(huk_label, derived_key, sizeof(derived_key));
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix SP] DEV_MODE: Key derivation failed: %d\r\n", status);
        return status;
    }

    if (msg->out_size[0] < DERIVED_KEY_LEN) {
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }
    psa_write(msg->handle, 0, derived_key, DERIVED_KEY_LEN);

    LOG_DBGSIG("[TinyMaix SP] DEV_MODE: HUK-derived key sent: ");
    for (int i = 0; i < DERIVED_KEY_LEN; i++) {
        LOG_DBGSIG("%02x", derived_key[i]);
    }
    LOG_DBGSIG("\r\n");

    return PSA_SUCCESS;
}
#endif

```

#### 3.2.3. CMake Build Configuration: `partitions/tinymaix_inference/CMakeLists.txt`
Similar to Echo Service, but links TinyMaix and potentially crypto libraries.
```cmake
if (NOT TFM_PARTITION_TINYMAIX_INFERENCE)
    return()
endif()

add_library(tfm_app_rot_partition_tinymaix_inference STATIC)

target_sources(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        tinymaix_inference.c
        # Add path to encrypted_mnist_model_psa.c if it's compiled directly
        ${CMAKE_SOURCE_DIR}/models/encrypted_mnist_model_psa.c
)

target_include_directories(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/interface/include
        ${CMAKE_SOURCE_DIR}/lib/tinymaix/include # TinyMaix headers
        ${CMAKE_SOURCE_DIR}/models # For model header
        # TF-M Crypto service headers if calling directly
        # ${CMAKE_SOURCE_DIR}/pico2w-trusted-firmware-m/secure_fw/partitions/crypto/psa_crypto_core.h (example path)
)

target_link_libraries(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        tfm_secure_api
        tfm_sprt
        platform_s
        tfm_sp_log_s
        tinymaix_s # Secure build of TinyMaix library
        # tfm_crypto_service_s # If calling PSA Crypto directly
)

target_compile_definitions(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        TFM_PARTITION_TINYMAIX_INFERENCE
        $<$<BOOL:${DEV_MODE}>:DEV_MODE> // Pass DEV_MODE flag from main build
        // Define model parameters if not in header
        // INPUT_H=28 INPUT_W=28 INPUT_C=1 etc.
)
```

## 4. Creating a New Secure Partition (Step-by-Step)

This section guides you through creating a new custom Secure Partition.

### Step 1: Create Directory Structure

```bash
# Navigate to the partitions directory in your project
cd <project_root>/pico2w-trusted-firmware-m/partitions # Or your project's equivalent

# Create a directory for your new service
mkdir my_custom_service
cd my_custom_service

# Create the necessary files
touch my_custom_service.c
touch my_custom_service_manifest.yaml
touch CMakeLists.txt
```
It's common to place APP-ROT partitions within the application's source tree rather than TF-M's core `partitions` directory. For `pico2w-tfm-tflm`, this would be under `<project_root>/partitions/my_custom_service`.

### Step 2: Write the Manifest File (`my_custom_service_manifest.yaml`)

```yaml
# partitions/my_custom_service/my_custom_service_manifest.yaml
{
  "description": "My Custom Secure Service",
  "psa_framework_version": "1.1",
  "name": "TFM_SP_MY_CUSTOM_SERVICE",  # Unique partition name
  "type": "APPLICATION-ROT",
  "priority": "NORMAL", # Can be HIGH, NORMAL, LOW
  "id": "0x00000108",  # !!! CHOOSE A UNIQUE PARTITION ID (e.g., increment from existing PIDs) !!!
  "entry_point": "tfm_my_custom_service_main", # Main function of your service
  "stack_size": "0x1000", # 4KB stack, adjust as needed
  "services": [
    {
      "name": "TFM_MY_CUSTOM_SERVICE_ENDPOINT", # Unique service endpoint name
      "sid": "0x00000108",  # !!! CHOOSE A UNIQUE SERVICE ID (SID) !!! Often same as PID for single-service partitions.
      "connection_based": true, # true for psa_connect/psa_close, false for stateless psa_call only
      "non_secure_clients": true, # Allow calls from NSPE
      "version": 1,
      "version_policy": "STRICT" # Or RELAXED
    }
    // You can define multiple services (endpoints) within one partition if needed
  ],
  "mmio_regions": [ # If your service needs direct hardware peripheral access
    // {
    //   "name": "TFM_PERIPHERAL_UART0", // Pre-defined peripheral name in TF-M platform
    //   "permission": "READ-WRITE"
    // }
  ],
  "irqs": [ # If your service needs to handle interrupts
    // {
    //   "source": "TIMER0_IRQn", // Platform-specific IRQ number
    //   "signal": "MY_SERVICE_TIMER_SIGNAL", // Signal name to be generated in manifest header
    //   "tfm_irq_priority": 64 // TF-M internal priority for the IRQ
    // }
  ],
  "linker_pattern": {
    "library_list": [ // If your partition uses specific static libraries
      // "*my_custom_lib*"
    ]
  },
  "dependencies": [ // If your service calls other PSA services
    // "TFM_CRYPTO", "TFM_PROTECTED_STORAGE"
  ]
}

```
**Important:** `id` (Partition ID) and `sid` (Service ID) must be unique across the entire system.

### Step 3: Implement the Service Logic (`my_custom_service.c`)

```c
// partitions/my_custom_service/my_custom_service.c
#include "tfm_sp_log.h" // For LOG_INFSIG, LOG_ERRSIG, etc.
#include "psa/service.h"
#include "psa_manifest/tfm_my_custom_service.h" // Auto-generated from manifest, provides signal name

// Define custom IPC message types (using rhandle in psa_call)
#define MY_SERVICE_IPC_OPERATION_FOO (1U)
#define MY_SERVICE_IPC_OPERATION_BAR (2U)

// Buffer for data exchange (example)
#define MAX_DATA_SIZE 128
static uint8_t internal_buffer[MAX_DATA_SIZE];

// Forward declarations for operation handlers
static psa_status_t handle_operation_foo(psa_msg_t *msg);
static psa_status_t handle_operation_bar(psa_msg_t *msg);
static void initialize_custom_service(void);

/* Main service entry point */
void tfm_my_custom_service_main(void)
{
    psa_msg_t msg;

    initialize_custom_service();
    LOG_INFSIG("[MyCustomService] Started and initialized.\r\n");

    while (1) {
        // Wait for a signal associated with this service's endpoint
        // TFM_MY_CUSTOM_SERVICE_ENDPOINT_SIGNAL is auto-generated from manifest service name
        psa_wait(TFM_MY_CUSTOM_SERVICE_ENDPOINT_SIGNAL, PSA_BLOCK);

        // Get the message details
        psa_status_t status = psa_get(TFM_MY_CUSTOM_SERVICE_ENDPOINT_SIGNAL, &msg);
        if (status != PSA_SUCCESS) {
            LOG_ERRSIG("[MyCustomService] Failed to get message: %d\r\n", status);
            continue; // Or handle error more robustly
        }

        switch (msg.type) {
            case PSA_IPC_CONNECT:
                LOG_INFSIG("[MyCustomService] Client connected (handle: %d)\r\n", msg.handle);
                psa_reply(msg.handle, PSA_SUCCESS);
                break;

            case PSA_IPC_CALL: {
                uint32_t operation_type = (uint32_t)(uintptr_t)msg.rhandle;
                LOG_DBGSIG("[MyCustomService] IPC_CALL for operation: %d\r\n", operation_type);
                psa_status_t op_status = PSA_ERROR_NOT_SUPPORTED;
                switch (operation_type) {
                    case MY_SERVICE_IPC_OPERATION_FOO:
                        op_status = handle_operation_foo(&msg);
                        break;
                    case MY_SERVICE_IPC_OPERATION_BAR:
                        op_status = handle_operation_bar(&msg);
                        break;
                    default:
                        LOG_ERRSIG("[MyCustomService] Unknown operation type: %d\r\n", operation_type);
                        break;
                }
                psa_reply(msg.handle, op_status);
                break;
            }

            case PSA_IPC_DISCONNECT:
                LOG_INFSIG("[MyCustomService] Client disconnected (handle: %d)\r\n", msg.handle);
                // Perform any cleanup related to this connection
                psa_reply(msg.handle, PSA_SUCCESS);
                break;

            default:
                LOG_ERRSIG("[MyCustomService] Unknown message type: %d\r\n", msg.type);
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

static void initialize_custom_service(void)
{
    // Perform any one-time initialization for your service here
    memset(internal_buffer, 0, sizeof(internal_buffer));
    LOG_DBGSIG("[MyCustomService] Internal buffer initialized.\r\n");
}

static psa_status_t handle_operation_foo(psa_msg_t *msg)
{
    // Example: Read data, process, write data
    size_t bytes_read;
    if (msg->in_size[0] > MAX_DATA_SIZE) {
        LOG_ERRSIG("[MyCustomService FOO] Input data too large: %d\r\n", msg->in_size[0]);
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    bytes_read = psa_read(msg->handle, 0, internal_buffer, msg->in_size[0]);
    if (bytes_read != msg->in_size[0]) {
        LOG_ERRSIG("[MyCustomService FOO] Failed to read input: %d\r\n", bytes_read);
        return PSA_ERROR_COMMUNICATION_FAILURE;
    }

    LOG_INFSIG("[MyCustomService FOO] Received %d bytes: %.*s\r\n", bytes_read, (int)bytes_read, internal_buffer);

    // Process data in internal_buffer (e.g., reverse it)
    for(size_t i = 0; i < bytes_read / 2; ++i) {
        uint8_t temp = internal_buffer[i];
        internal_buffer[i] = internal_buffer[bytes_read - 1 - i];
        internal_buffer[bytes_read - 1 - i] = temp;
    }

    if (msg->out_size[0] > 0) {
        psa_write(msg->handle, 0, internal_buffer, MIN(bytes_read, msg->out_size[0]));
    }
    return PSA_SUCCESS;
}

static psa_status_t handle_operation_bar(psa_msg_t *msg)
{
    // Example: Simple operation, no data in/out, just returns a value
    // Client might pass a small integer via in_vec[0] if needed.
    int32_t input_param = 0;
    if (msg->in_size[0] == sizeof(int32_t)) {
        psa_read(msg->handle, 0, &input_param, sizeof(int32_t));
        LOG_INFSIG("[MyCustomService BAR] Received parameter: %d\r\n", input_param);
    }

    // Perform some action based on input_param
    int32_t result_val = input_param * 2;

    if (msg->out_size[0] >= sizeof(int32_t)) {
        psa_write(msg->handle, 0, &result_val, sizeof(int32_t));
    }
    return PSA_SUCCESS;
}

```

### Step 4: Create CMake Configuration (`CMakeLists.txt`)

```cmake
# partitions/my_custom_service/CMakeLists.txt

# Guard to allow enabling/disabling this partition from the main build config
if (NOT TFM_PARTITION_MY_CUSTOM_SERVICE)
    return()
endif()

# Minimum CMake version (align with TF-M's requirements)
cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0079 NEW) # Handle IN_LIST operator correctly

# Create a static library for this partition
add_library(tfm_app_rot_partition_my_custom_service STATIC)

# Add source files for the partition
target_sources(tfm_app_rot_partition_my_custom_service
    PRIVATE
        my_custom_service.c
)

# Add generated sources (manifest processing output)
# Paths might vary slightly based on TF-M version and project structure
target_sources(tfm_app_rot_partition_my_custom_service
    PRIVATE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/my_custom_service/auto_generated/intermedia_tfm_my_custom_service.c
)

# Add to the global list of partition load info objects
# This tells TF-M's build system about your partition's manifest for code generation
target_sources(tfm_partitions # This is a global INTERFACE library in TF-M
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/my_custom_service/auto_generated/load_info_tfm_my_custom_service.c
)

# Include directories required by the partition
target_include_directories(tfm_app_rot_partition_my_custom_service
    PRIVATE
        .  # Current directory for local headers (if any)
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/my_custom_service/auto_generated # For generated manifest header
        ${CMAKE_SOURCE_DIR}/../../interface/include # For PSA client API definitions (if calling other services)
        ${CMAKE_SOURCE_DIR}/../../pico2w-trusted-firmware-m/secure_fw/core/include # TF-M core includes
        ${CMAKE_SOURCE_DIR}/../../pico2w-trusted-firmware-m/secure_fw/spm/include # SPM includes
        # Add other TF-M or platform include paths as needed
)

# Include directories for the global tfm_partitions target
target_include_directories(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/my_custom_service/auto_generated
)

# Link against necessary libraries
target_link_libraries(tfm_app_rot_partition_my_custom_service
    PRIVATE
        platform_s      # Secure platform library
        tfm_sprt        # TF-M Secure Partition Runtime
        tfm_sp_log_s    # TF-M Secure Logging library
        # Add tfm_secure_api if calling other PSA services from this partition
        # Add other libraries like tfm_crypto_service_s if directly using their APIs
)

# Add this partition's library to the global tfm_partitions target
target_link_libraries(tfm_partitions
    INTERFACE
        tfm_app_rot_partition_my_custom_service
)

# Compile definitions for this partition
target_compile_definitions(tfm_app_rot_partition_my_custom_service
    PRIVATE
        TFM_PARTITION_MY_CUSTOM_SERVICE # For conditional compilation in C code
        $<$<BOOL:${DEV_MODE}>:DEV_MODE> # Propagate DEV_MODE if used
)
```

### Step 5: Register Partition in Manifest List

Edit `partitions/manifest_list.yaml` (this file is usually at `<project_root>/pico2w-trusted-firmware-m/partitions/manifest_list.yaml` or a similar central location for TF-M partitions). Add an entry for your new service:

```yaml
# ... (existing entries for echo_service, tinymaix_inference, etc.)
    {
      "description": "My Custom Service",
      "manifest": "partitions/my_custom_service/my_custom_service_manifest.yaml", # Path relative to manifest_list.yaml location
      "output_path": "partitions/my_custom_service", # Output directory for generated files
      "conditional": "TFM_PARTITION_MY_CUSTOM_SERVICE", # CMake flag to enable/disable
      "version_major": 0,
      "version_minor": 1,
      "pid": 446 # !!! CHOOSE A UNIQUE PID (Process ID) !!! Increment from existing.
    }
# ... (rest of the list)
```
**Note:** The `manifest` path here is relative to the location of `manifest_list.yaml` itself. If you placed your partition outside the TF-M tree (e.g., in your application's `partitions` folder), you'll need to adjust the path accordingly, possibly using an absolute path variable from CMake or a relative path like `../../../<app_folder>/partitions/my_custom_service/my_custom_service_manifest.yaml`.

### Step 6: Enable Partition in Build Configuration

Add a CMake option to enable your partition in a project-level CMake configuration file. For `pico2w-tfm-tflm`, this is typically in `tflm_spe/config/config_tflm.cmake` or a similar SPE configuration file:

```cmake
# In tflm_spe/config/config_tflm.cmake (or equivalent)
# ... (other partition flags)
set(TFM_PARTITION_MY_CUSTOM_SERVICE ON CACHE BOOL "Enable My Custom Secure Service")
```

### Step 7: Add to Root CMakeLists.txt for Partitions

If your partition directory is within a subdirectory that's processed by CMake (e.g., `<project_root>/partitions/CMakeLists.txt`), ensure your new partition's subdirectory is added:

```cmake
# In <project_root>/partitions/CMakeLists.txt (or where other APP-ROT partitions are added)
# ...
if(TFM_PARTITION_MY_CUSTOM_SERVICE)
    add_subdirectory(my_custom_service)
endif()
# ...
```

## 5. Non-Secure Client API Development

To call your Secure Partition from the NSPE, you need to create a client-side API.

### Step 1: Define API Interface Header

Create `interface/include/tfm_my_custom_service_api.h` (or a similar path accessible to NSPE code):

```c
// interface/include/tfm_my_custom_service_api.h
#ifndef TFM_MY_CUSTOM_SERVICE_API_H
#define TFM_MY_CUSTOM_SERVICE_API_H

#include <stdint.h>
#include <stddef.h>
#include "psa/client.h" // For psa_status_t

#ifdef __cplusplus
extern "C" {
#endif

/* The SID of your service, must match the one in my_custom_service_manifest.yaml */
#define TFM_MY_CUSTOM_SERVICE_SID       (0x00000108U)
#define TFM_MY_CUSTOM_SERVICE_VERSION   (1U)

/* IPC message types (rhandles for psa_call), must match definitions in my_custom_service.c */
#define MY_SERVICE_IPC_OPERATION_FOO    (1U)
#define MY_SERVICE_IPC_OPERATION_BAR    (2U)

/* Custom status codes for the service (optional, can also just use psa_status_t) */
typedef enum {
    TFM_MY_CUSTOM_SERVICE_SUCCESS = 0,
    TFM_MY_CUSTOM_SERVICE_ERROR_GENERIC = -201,
    TFM_MY_CUSTOM_SERVICE_ERROR_INVALID_PARAM = -202,
    // Add more specific errors as needed
} tfm_my_custom_service_status_t;

/**
 * @brief Calls the FOO operation in My Custom Service.
 *
 * @param[in]  input_data    Pointer to the input data buffer.
 * @param[in]  input_size    Size of the input data buffer.
 * @param[out] output_data   Pointer to the output data buffer.
 * @param[in,out] output_size As input, size of output_data buffer. As output, actual bytes written.
 * @return tfm_my_custom_service_status_t Operation status.
 */
tfm_my_custom_service_status_t tfm_my_custom_service_foo(
    const uint8_t* input_data, size_t input_size,
    uint8_t* output_data, size_t* output_size);

/**
 * @brief Calls the BAR operation in My Custom Service.
 *
 * @param[in]  param         An integer parameter for the operation.
 * @param[out] result        Pointer to store the integer result from the service.
 * @return tfm_my_custom_service_status_t Operation status.
 */
tfm_my_custom_service_status_t tfm_my_custom_service_bar(
    int32_t param, int32_t* result);

#ifdef __cplusplus
}
#endif

#endif /* TFM_MY_CUSTOM_SERVICE_API_H */
```

### Step 2: Implement Client API Functions

Create `interface/src/tfm_my_custom_service_api.c`:

```c
// interface/src/tfm_my_custom_service_api.c
#include "tfm_my_custom_service_api.h" // Your new header
#include "psa/client.h" // For psa_connect, psa_call, psa_close

tfm_my_custom_service_status_t tfm_my_custom_service_foo(
    const uint8_t* input_data, size_t input_size,
    uint8_t* output_data, size_t* output_size)
{
    psa_status_t status;
    psa_handle_t handle;

    if (!input_data || !output_data || !output_size) {
        return TFM_MY_CUSTOM_SERVICE_ERROR_INVALID_PARAM;
    }

    handle = psa_connect(TFM_MY_CUSTOM_SERVICE_SID, TFM_MY_CUSTOM_SERVICE_VERSION);
    if (handle <= 0) { // psa_connect returns handle > 0 on success
        // Map PSA error (from handle) to your custom error or return as is
        return TFM_MY_CUSTOM_SERVICE_ERROR_GENERIC; // Or map specific PSA errors
    }

    psa_invec_t in_vecs[1] = { {input_data, input_size} };
    // Pass the address of output_size because its value might be updated by psa_call
    // if the actual written size is different from buffer size.
    psa_outvec_t out_vecs[1] = { {output_data, *output_size} };

    status = psa_call(handle, MY_SERVICE_IPC_OPERATION_FOO, // Type/rhandle for the call
                      in_vecs, 1, out_vecs, 1);

    psa_close(handle);

    if (status == PSA_SUCCESS) {
        *output_size = out_vecs[0].len; // Update output_size with actual bytes written
        return TFM_MY_CUSTOM_SERVICE_SUCCESS;
    } else {
        // Map PSA error to your custom error or return as is
        return TFM_MY_CUSTOM_SERVICE_ERROR_GENERIC;
    }
}

tfm_my_custom_service_status_t tfm_my_custom_service_bar(
    int32_t param, int32_t* result)
{
    psa_status_t status;
    psa_handle_t handle;

    if (!result) {
        return TFM_MY_CUSTOM_SERVICE_ERROR_INVALID_PARAM;
    }

    handle = psa_connect(TFM_MY_CUSTOM_SERVICE_SID, TFM_MY_CUSTOM_SERVICE_VERSION);
    if (handle <= 0) {
        return TFM_MY_CUSTOM_SERVICE_ERROR_GENERIC;
    }

    psa_invec_t in_vecs[1] = { {&param, sizeof(param)} };
    psa_outvec_t out_vecs[1] = { {result, sizeof(*result)} };

    status = psa_call(handle, MY_SERVICE_IPC_OPERATION_BAR,
                      in_vecs, 1, out_vecs, 1);

    psa_close(handle);

    if (status == PSA_SUCCESS) {
        return TFM_MY_CUSTOM_SERVICE_SUCCESS;
    } else {
        return TFM_MY_CUSTOM_SERVICE_ERROR_GENERIC;
    }
}
```

## 6. Testing Your Secure Partition

### Step 1: Create a Non-Secure Test Application

In your NSPE application code (e.g., `tflm_ns/main_ns.c` or a dedicated test file like `tflm_ns/my_custom_service_test.c`):

```c
// In tflm_ns/my_custom_service_test.c (example)
#include <stdio.h>
#include "tfm_my_custom_service_api.h" // Your client API header

// Declare a test function if creating a separate test file
void test_my_custom_service(void);

void test_my_custom_service(void)
{
    printf("--- Testing My Custom Service ---\n");
    tfm_my_custom_service_status_t status;

    // Test FOO operation
    const char* foo_input = "HelloSecurePartition";
    uint8_t foo_output[128];
    size_t foo_output_size = sizeof(foo_output);

    printf("Calling MyCustomService_Foo with input: '%s'\n", foo_input);
    status = tfm_my_custom_service_foo((const uint8_t*)foo_input, strlen(foo_input),
                                       foo_output, &foo_output_size);

    if (status == TFM_MY_CUSTOM_SERVICE_SUCCESS) {
        printf("MyCustomService_Foo SUCCESS. Output (%d bytes): %.*s\n",
               (int)foo_output_size, (int)foo_output_size, foo_output);
    } else {
        printf("MyCustomService_Foo FAILED with status: %d\n", status);
    }

    // Test BAR operation
    int32_t bar_param = 21;
    int32_t bar_result;
    printf("Calling MyCustomService_Bar with param: %d\n", bar_param);
    status = tfm_my_custom_service_bar(bar_param, &bar_result);

    if (status == TFM_MY_CUSTOM_SERVICE_SUCCESS) {
        printf("MyCustomService_Bar SUCCESS. Result: %d\n", bar_result);
    } else {
        printf("MyCustomService_Bar FAILED with status: %d\n", status);
    }
    printf("--- My Custom Service Test Complete ---\n
");
}
```

### Step 2: Add Test Code to NSPE Build

If you created `my_custom_service_test.c` and `tfm_my_custom_service_api.c`, add them to your NSPE application's CMakeLists.txt (e.g., `app_broker/CMakeLists.txt` or `tflm_ns/CMakeLists.txt`):

```cmake
# In app_broker/CMakeLists.txt (or similar for NSPE app)
target_sources(tfm_tflm_broker # Or your NSPE application target name
    PRIVATE
        # ... existing NSPE sources ...
        ${CMAKE_SOURCE_DIR}/tflm_ns/my_custom_service_test.c # Path to your test C file
        ${CMAKE_SOURCE_DIR}/interface/src/tfm_my_custom_service_api.c # Path to your client API C file
)

# Ensure include directories are set for the NSPE application to find your API header
target_include_directories(tfm_tflm_broker
    PRIVATE
        ${CMAKE_SOURCE_DIR}/interface/include # Where tfm_my_custom_service_api.h is
        # ... other NSPE include directories ...
)
```

### Step 3: Call Test Function from NSPE Main

In your main NSPE application file (e.g., `app_broker/main_ns.c`), include the test header and call the test function:

```c
// In app_broker/main_ns.c
// ... other includes ...
#include "tfm_my_custom_service_api.h" // If directly calling API, or...
#include "my_custom_service_test.h" // If you created a test header

// If you created a separate test file with a test function:
extern void test_my_custom_service(void);

int main(void)
{
    // ... board init, etc. ...

    printf("Non-Secure Application Started\n");

    // Call your test function
    test_my_custom_service();

    // ... rest of NSPE application ...
    while(1) { /* loop */ }
    return 0;
}
```

## 7. Advanced Secure Partition Features

### 7.1. Interrupt Handling

Secure Partitions can handle hardware interrupts.

1.  **Manifest Configuration** (`my_custom_service_manifest.yaml`):

    ```yaml
    "irqs": [
      {
        "source": "TFM_TIMER0_IRQ",       # Platform-defined IRQ source name
        "signal": "MY_TIMER_SIGNAL",     # Signal name auto-generated in manifest header
        "tfm_irq_priority": 64         # TF-M's priority for this IRQ (platform specific range)
      }
    ]
    ```

2.  **Service Code** (`my_custom_service.c`):

    ```c
    // In tfm_my_custom_service_main's while(1) loop or a dedicated thread/handler
    // psa_wait can wait on multiple signals using bitwise OR
    psa_signal_t signals = psa_wait(TFM_MY_CUSTOM_SERVICE_ENDPOINT_SIGNAL | MY_TIMER_SIGNAL, PSA_BLOCK);

    if (signals & MY_TIMER_SIGNAL) {
        // This signal was asserted due to the interrupt
        LOG_INFSIG("[MyCustomService] Timer interrupt occurred!\r\n");
        // Process the interrupt event
        // ... your interrupt handling logic ...

        // Clear the interrupt pending state in the peripheral if necessary
        // ... platform_clear_timer_irq(); ...

        // Acknowledge handling of the interrupt signal to TF-M
        psa_eoi(MY_TIMER_SIGNAL); // End Of Interrupt
    }
    if (signals & TFM_MY_CUSTOM_SERVICE_ENDPOINT_SIGNAL) {
        // Process regular IPC message as before
        // ... psa_get(), switch(msg.type) ...
    }
    ```

### 7.2. MMIO (Memory Mapped I/O) Access

Partitions can be granted direct access to peripheral registers.

1.  **Manifest Configuration** (`my_custom_service_manifest.yaml`):

    ```yaml
    "mmio_regions": [
      {
        "name": "TFM_PERIPHERAL_GPIO",  # Platform-defined peripheral name (maps to base address & size)
        "permission": "READ-WRITE"     # Or "READ-ONLY"
      }
      // Add more peripherals if needed
    ]
    ```
    The platform's configuration (e.g., `platform_def.h` or device tree) maps `TFM_PERIPHERAL_GPIO` to actual memory addresses.

2.  **Service Code** (`my_custom_service.c`):
    TF-M will configure the MPU to allow access. Your code can then use pointers to access these registers.

    ```c
    // platform_drivers.h or similar would define this based on TFM_PERIPHERAL_GPIO
    #define GPIO_BASE_S (0x50000000UL) // Example secure base address of GPIO
    #define GPIO_DATA_OUT_REG_OFFSET (0x04)

    volatile uint32_t* gpio_data_out_s = (uint32_t*)(GPIO_BASE_S + GPIO_DATA_OUT_REG_OFFSET);
    *gpio_data_out_s = 0x00000001; // Write to GPIO register
    ```
    **Caution:** Direct MMIO access requires careful handling to maintain security and avoid conflicts.

### 7.3. Secure Communication Patterns

When exchanging sensitive data, consider adding layers of protection within your IPC messages if TF-M's basic transport security is insufficient for your threat model.

```c
// Example structure for a more secured message payload
typedef struct {
    uint32_t sequence_number; // To prevent replay attacks
    uint16_t data_length;     // Actual length of payload_data
    uint16_t checksum;        // Simple checksum of payload_data
    // Consider MAC (Message Authentication Code) for stronger integrity/authenticity
    // uint8_t mac[16];
    uint8_t payload_data[];  // Flexible array member for actual data
} secure_payload_t;

/* In your service handler:
1. Read the secure_payload_t header.
2. Verify sequence_number (if applicable).
3. Verify checksum/MAC over payload_data.
4. Process payload_data.
5. Construct a similar secure response.
*/
```
PSA Crypto API can be used within the partition to generate MACs or perform encryption/decryption if needed.

## 8. Performance Optimization Strategies

### 8.1. Memory Usage Optimization

-   **Static Allocation:** Prefer static or stack allocation over heap (`malloc`) within SPs to avoid fragmentation and non-deterministic behavior. TF-M's default heap for SPs can be limited.
-   **Buffer Pooling:** For frequently allocated/deallocated buffers of fixed sizes, implement a static buffer pool.

    ```c
    #define NUM_BUFFERS 4
    #define BUFFER_SZ 256
    static uint8_t g_buffer_pool[NUM_BUFFERS][BUFFER_SZ];
    static bool g_buffer_in_use[NUM_BUFFERS] = {false};

    uint8_t* alloc_from_pool() {
        for(int i=0; i<NUM_BUFFERS; ++i) {
            if(!g_buffer_in_use[i]) {
                g_buffer_in_use[i] = true;
                return g_buffer_pool[i];
            }
        }
        return NULL; // Pool exhausted
    }

    void free_to_pool(uint8_t* buf) {
        for(int i=0; i<NUM_BUFFERS; ++i) {
            if(g_buffer_pool[i] == buf) {
                g_buffer_in_use[i] = false;
                return;
            }
        }
    }
    ```
-   **Stack Size:** Carefully estimate and configure `stack_size` in the manifest. Too small leads to stack overflows; too large wastes RAM.

### 8.2. Batch Processing

If clients frequently make multiple small calls, consider adding a batch operation to reduce IPC overhead.

1.  **Define Batch Structures** (in client API header and service C file):

    ```c
    typedef struct {
        uint32_t operation_id; // Identifies the sub-operation
        uint32_t param1;       // Example parameters
        // ... other params ...
    } single_op_t;

    #define MAX_OPS_IN_BATCH 5
    typedef struct {
        uint32_t num_operations;
        single_op_t operations[MAX_OPS_IN_BATCH];
        // Results can be written back into this structure or a separate output vec
    } batch_request_t;
    ```

2.  **Implement Batch Handler in Service**:
    The client sends one `psa_call` with `batch_request_t`. The service iterates through `operations` and processes them.

## 9. Debugging and Monitoring Secure Partitions

### 9.1. Logging

Use TF-M's logging macros within your service code:
-   `LOG_INFSIG(...)`: Informational messages.
-   `LOG_ERRSIG(...)`: Error messages.
-   `LOG_DBGSIG(...)`: Debug messages (often compiled out in release builds).
-   `LOG_WRNSIG(...)`: Warning messages.

These logs are typically routed to a UART console on the NSPE side.

### 9.2. Partition Statistics (Example)

Maintain internal counters for monitoring.

```c
// In my_custom_service.c
typedef struct {
    uint32_t total_requests;
    uint32_t successful_requests;
    uint32_t failed_requests;
    uint32_t active_connections; // If connection_based
} service_stats_t;

static service_stats_t g_stats = {0};

// Increment in appropriate places:
// g_stats.total_requests++; in PSA_IPC_CALL handler
// g_stats.active_connections++; in PSA_IPC_CONNECT
// g_stats.active_connections--; in PSA_IPC_DISCONNECT

// Optionally, add an IPC call to retrieve these stats for debugging.
```

### 9.3. Robust Error Handling and Recovery

-   **Validate Inputs:** Thoroughly validate all data from NSPE (sizes, pointers, content).
-   **Resource Cleanup:** Ensure resources (memory, PSA Crypto handles, etc.) are freed, especially on error paths.
-   **State Restoration:** If an operation fails mid-way, try to restore the partition to a consistent state.

    ```c
    static psa_status_t robust_operation_handler(psa_msg_t *msg) {
        psa_status_t status = PSA_SUCCESS;
        void* temp_resource = NULL;

        // temp_resource = allocate_some_resource();
        // if (!temp_resource) return PSA_ERROR_INSUFFICIENT_MEMORY;

        // status = perform_step1(msg, temp_resource);
        // if (status != PSA_SUCCESS) goto cleanup_and_exit;

        // status = perform_step2(msg, temp_resource);
        // if (status != PSA_SUCCESS) {
        //     revert_step1_effects(); // Attempt to roll back
        //     goto cleanup_and_exit;
        // }

    // cleanup_and_exit:
        // if (temp_resource) free_some_resource(temp_resource);
        // return status;
    }
    ```

### 9.4. Common Issues & Tips

1.  **Manifest Errors**: YAML is strict. Typos or incorrect structure can cause build failures during manifest processing.
2.  **SID/PID Clashes**: Ensure `id` (Partition ID) and `sid` (Service ID) in manifests are unique across all partitions.
3.  **Stack Overflow**: If the partition crashes or behaves erratically, suspect stack overflow. Increase `stack_size` in the manifest.
4.  **Alignment Issues**: Data in `psa_invec_t` / `psa_outvec_t` might have specific alignment requirements depending on the platform and data types.
5.  **Build System Integration**: Ensure `CMakeLists.txt` for the partition is correctly written and integrated into the main TF-M build.
6.  **Generated Headers**: Files like `psa_manifest/tfm_my_custom_service.h` are auto-generated. If they are missing or outdated, it usually indicates a build configuration or manifest processing issue.

## 10. Next Steps

With your Secure Partition developed and tested, refer to related documentation:

-   **[PSA API Guide](./psa-api.md)**: For deeper understanding of PSA client/server interactions.
-   **[Model Encryption Guide](./model-encryption.md)**: If your partition handles encrypted ML models.
-   **[TinyMaix Integration Guide](./tinymaix-integration.md)**: For specifics on the ML inference service.
-   **[Testing Framework Guide](./testing-framework.md)**: For writing more comprehensive tests.
-   **[Troubleshooting Guide](./troubleshooting.md)**: For common issues and solutions.