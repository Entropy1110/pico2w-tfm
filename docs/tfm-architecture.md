# TF-M Architecture Guide

## Overview

Trusted Firmware-M (TF-M) implements the ARM Platform Security Architecture (PSA) on Cortex-M processors, providing a dual-world security model using ARM TrustZone technology. This project utilizes TF-M on the Raspberry Pi Pico 2W (RP2350) to implement secure machine learning inference.

## ARM TrustZone Architecture

### Dual-World Model
```
┌─────────────────────────┬─────────────────────────┐
│     Secure World (SPE)      │    Non-Secure World (NSPE)    │
│   Secure Processing     │  Non-Secure Processing  │
│     Environment         │      Environment        │
├─────────────────────────┼─────────────────────────┤
│ • TF-M Core             │ • Application Code       │
│ • Secure Partitions     │ • RTOS (e.g., CMSIS-RTOS2/RTX)│
│ • PSA Services          │ • Test Frameworks       │
│ • TinyMaix Inference    │ • PSA Client APIs       │
│ • Cryptographic Services│                         │
└─────────────────────────┴─────────────────────────┘
```

### Memory Isolation

**Flash Memory Layout (RP2350 Example):**
```
┌──────────────────────────────────────┐ 0x10200000 (Example End)
│               Flash End              │
├──────────────────────────────────────┤
│         Non-Secure Code              │
│         (NSPE Applications, Tests)   │
├──────────────────────────────────────┤ (e.g., 0x10080000)
│         Secure Code                  │
│         (TF-M Core + Partitions)     │
├──────────────────────────────────────┤ (e.g., 0x10011000)
│         Bootloader (MCUBoot)         │
├──────────────────────────────────────┤ 0x10000000 (Flash Start)
│         Flash Start                  │
└──────────────────────────────────────┘
```
*Note: Specific addresses depend on the build configuration.*

**RAM Memory Layout (Example):**
```
┌──────────────────────────────────────┐ (e.g., 0x20042000 RAM End)
│               RAM End                │
├──────────────────────────────────────┤
│         Non-Secure RAM               │
│         (Application Data, RTOS Heap)│
├──────────────────────────────────────┤ (e.g., 0x20020000)
│         Secure RAM                   │
│         (TF-M Stack/Heap, Partition Data)│
├──────────────────────────────────────┤ 0x20000000 (RAM Start)
│         RAM Start                    │
└──────────────────────────────────────┘
```
*Note: Specific addresses depend on the build configuration and MPU settings.*


### Build Artifacts Location
- **SPE Image**: `build/spe/bin/tfm_s.bin` (or similar, depends on platform)
- **NSPE Image**: `build/nspe/bin/tfm_ns.bin` (or similar)
- **Combined Signed Image**: `build/spe/bin/tfm_s_ns_signed.uf2` (or `.bin`/`.hex`)
- **Bootloader Image**: `build/spe/bin/bl2.uf2` (or `.bin`/`.hex`)

## TF-M Component Structure

### Core Components
```
TF-M Core Architecture:
┌─────────────────────────────────────────────────────┐
│                   SPE (Secure World)                │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │    PSA      │  │ Secure      │  │ Inter-Process│  │
│  │   Framework │  │ Partition   │  │ Communication│  │
│  │   APIs      │  │ Manager(SPM)│  │   (IPC)     │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │    Echo     │  │  TinyMaix   │  │    Crypto   │  │
│  │   Service   │  │  Inference  │  │   Service   │  │
│  │  (PID 444)  │  │  (PID 445)  │  │  (Built-in) │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │   Internal  │  │  Protected  │  │ Attestation │  │
│  │   Trusted   │  │   Storage   │  │   Service   │  │
│  │   Storage   │  │   Service   │  │  (Built-in) │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────┤
│          Hardware Abstraction Layer (HAL)           │
└─────────────────────────────────────────────────────┘
                            │ (PSA Functional API / IPC)
┌─────────────────────────────────────────────────────┐
│                  NSPE (Non-Secure World)            │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │   Test      │  │  Application│  │    PSA      │  │
│  │   Runner    │  │   (e.g. App │  │  Client API │  │
│  │             │  │   Broker)   │  │             │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────┤
│                   RTOS (e.g., CMSIS-RTOS2/RTX)      │
└─────────────────────────────────────────────────────┘
```

## PSA (Platform Security Architecture)

### PSA Service Model
The PSA framework defines how Non-Secure applications can request services from Secure Partitions.

```c
// PSA Client-Server Communication Model

// 1. Connect to the service (for connection-based services)
//    SERVICE_SID is the Secure Service ID.
//    Version is the requested version of the service.
psa_handle_t handle = psa_connect(SERVICE_SID, SERVICE_VERSION);
if (handle <= 0) {
    // Error handling: connection failed
}

// 2. Call the service
//    'operation_type' specifies the requested action within the service.
//    'in_vec' contains input data buffers.
//    'out_vec' contains output data buffers.
psa_status_t status = psa_call(handle, PSA_IPC_CALL, // or specific operation_type
                              in_vec, num_invec,
                              out_vec, num_outvec);
if (status != PSA_SUCCESS) {
    // Error handling: service call failed
}

// 3. Close the connection (for connection-based services)
psa_close(handle);
```
Stateless services might be invoked directly with `psa_call` using a specific `SERVICE_SID` instead of a connection handle, or through dedicated PSA Functional APIs (e.g., `psa_crypto_xxx()`).

### Service and Partition Identifiers
Secure services and partitions are identified by unique IDs.
```yaml
# Project Service ID Mapping (Example)
Service Name        | SID (Service ID) | PID (Partition ID) | Type
────────────────────|──────────────────|────────────────----|───────────────────
Echo Service        | 0x00000105       | 444                | Connection-based
TinyMaix Inference  | 0x00000107       | 445                | Connection-based
PSA Crypto          | (Varies)         | (TF-M Internal)    | API-based
PSA Storage (ITS/PS)| (Varies)         | (TF-M Internal)    | API-based
Initial Attestation | (Varies)         | (TF-M Internal)    | API-based
```
- **SID (Service ID)**: Unique identifier for a specific service/interface offered by a partition.
- **PID (Partition ID)**: Unique identifier for a Secure Partition. A partition can host multiple services.

## Boot Sequence

### System Boot Process
1.  **MCUBoot (Bootloader - BL2)**:
    *   Performs basic hardware initialization.
    *   Verifies the integrity and authenticity of the Secure Processing Environment (SPE) image (TF-M firmware).
    *   If valid, loads and jumps to the TF-M SPE entry point.

2.  **TF-M Secure Boot (SPE Initialization)**:
    *   Initializes TrustZone security controllers (SAU, MPU).
    *   Sets up Secure Memory regions.
    *   Initializes the Secure Partition Manager (SPM).
    *   Initializes all configured Secure Partitions (including PSA RoT services and Application RoT services).
    *   Registers PSA services.
    *   Jumps to the Non-Secure Processing Environment (NSPE) entry point.

3.  **Non-Secure Boot (NSPE Initialization)**:
    *   Initializes the Non-Secure RTOS (e.g., CMSIS-RTOS2/RTX).
    *   Sets up PSA client API infrastructure.
    *   Starts the main Non-Secure application (e.g., `app_broker`, test runner).

### Initialization Flow Diagram
```
Power On Reset
      │
      ▼
┌───────────┐
│  MCUBoot  │ (BL2)
└───────────┘
      │   1. HW Init
      │   2. Verify SPE Image
      │   3. Load & Jump to SPE
      ▼
┌───────────┐
│ TF-M Core │ (SPE)
└───────────┘
      │   1. TrustZone Init (SAU, MPU)
      │   2. SPM Init
      │   3. Secure Partitions Init (RoT, App RoT)
      │   4. PSA Services Register
      │   5. Jump to NSPE
      ▼
┌───────────┐
│   NSPE    │
└───────────┘
      │   1. RTOS Init
      │   2. PSA Client Init
      │   3. Application Start / Test Execution
      ▼
  System Running
```

## Secure Partitions Architecture

Secure Partitions are the fundamental building blocks for security services in TF-M. They are isolated software components running in the SPE.

### Partition Types and Roles
*   **PSA Root of Trust (PSA RoT) Partitions**: Provide fundamental security services defined by PSA (e.g., Crypto, Secure Storage, Attestation). These are typically part of the TF-M core.
*   **Application Root of Trust (App RoT) Partitions**: User-defined Secure Partitions providing application-specific security services (e.g., Echo Service, TinyMaix Inference Service in this project).

```c
// Conceptual metadata for a Secure Partition (defined in its manifest)
// This information is used by the SPM to manage the partition.
typedef struct partition_metadata {
    uint32_t partition_id;           // Unique Partition ID (PID)
    uint32_t service_count;          // Number of services it provides
    const char* name;                // Partition name string
    uint32_t stack_size;             // Allocated stack size
    bool connection_based;           // If it hosts connection-based services
    // ... other manifest properties like memory regions, IRQs, etc.
} partition_metadata_t;
```
Each partition operates in its own isolated memory space, enforced by the MPU (Memory Protection Unit) configured by the SPM. Communication between partitions or between NSPE and SPE happens via PSA IPC mechanisms.

### Echo Service Example Structure (Conceptual)
The Echo service is a simple App RoT partition demonstrating basic PSA IPC.
```c
// In echo_service_partition.c (Simplified)
#include "psa/service.h"
#include "psa_manifest/sid.h" // For ECHO_SERVICE_SID

#define ECHO_SERVICE_PID // Defined in manifest, e.g., 444

void echo_service_main(void *param) {
    (void)param;
    psa_msg_t msg;
    psa_status_t status;

    while (1) {
        // Wait for any incoming PSA message/signal
        psa_wait(PSA_WAIT_ANY, PSA_BLOCK); // Or use psa_get with specific signals

        status = psa_get(ECHO_SERVICE_SIGNAL, &msg); // ECHO_SERVICE_SIGNAL is from manifest
        if (status != PSA_SUCCESS) {
            // Error handling
            continue;
        }

        switch (msg.type) {
            case PSA_IPC_CONNECT:
                // Client is trying to connect
                if (/* some condition for allowing connection */) {
                    psa_reply(msg.handle, PSA_SUCCESS);
                } else {
                    psa_reply(msg.handle, PSA_ERROR_CONNECTION_REFUSED);
                }
                break;

            case PSA_IPC_CALL:
                // Client sent a request
                // Process input from msg.in_size, msg.rhandle
                // Write output to msg.out_size
                // Example: echo back the input
                if (msg.in_size[0] > 0 && msg.out_size[0] >= msg.in_size[0]) {
                    uint8_t buffer[msg.in_size[0]];
                    psa_read(msg.handle, 0, buffer, msg.in_size[0]);
                    psa_write(msg.handle, 0, buffer, msg.in_size[0]);
                    psa_reply(msg.handle, PSA_SUCCESS);
                } else {
                    psa_reply(msg.handle, PSA_ERROR_INVALID_ARGUMENT);
                }
                break;

            case PSA_IPC_DISCONNECT:
                // Client is disconnecting
                psa_reply(msg.handle, PSA_SUCCESS);
                break;

            default:
                // Unknown message type
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}
```

### TinyMaix Inference Service Structure (Conceptual)
The TinyMaix service is an App RoT partition for secure ML model inference.
```c
// In tinymaix_service_partition.c (Simplified)
#include "psa/service.h"
#include "psa_manifest/sid.h" // For TINYMAIX_SERVICE_SID
#include "tinymaix.h"         // TinyMaix library

#define TINYMAIX_SERVICE_PID // Defined in manifest, e.g., 445

// Custom IPC message types for TinyMaix service
#define TINYMAIX_IPC_LOAD_MODEL     (PSA_IPC_CALL + 1) // Or use msg.rhandle for type
#define TINYMAIX_IPC_SET_INPUT      (PSA_IPC_CALL + 2)
#define TINYMAIX_IPC_RUN_INFERENCE  (PSA_IPC_CALL + 3)
#ifdef DEV_MODE
#define TINYMAIX_IPC_GET_MODEL_KEY  (PSA_IPC_CALL + 4) // Debug only
#endif

// Model state (global within the partition)
static bool g_model_loaded = false;
static tm_mdl_t g_model;
#define MODEL_BUFFER_SIZE (/* appropriate size */)
static uint8_t g_model_buffer[MODEL_BUFFER_SIZE] __attribute__((aligned(4))); // Ensure alignment

void tinymaix_service_main(void *param) {
    // ... similar structure to echo_service_main ...
    // Handle PSA_IPC_CONNECT, PSA_IPC_DISCONNECT

    // For PSA_IPC_CALL, or specific types like TINYMAIX_IPC_LOAD_MODEL:
    // switch (msg.type) {
    //    case TINYMAIX_IPC_LOAD_MODEL:
    //        // Read model data from NSPE (via psa_read)
    //        // Decrypt model if necessary
    //        // Initialize g_model with tm_load()
    //        // psa_reply()
    //        break;
    //    case TINYMAIX_IPC_SET_INPUT:
    //        // Read input tensor data (via psa_read)
    //        // psa_reply()
    //        break;
    //    case TINYMAIX_IPC_RUN_INFERENCE:
    //        // Run tm_run(&g_model, ...)
    //        // Write output tensor data (via psa_write)
    //        // psa_reply()
    //        break;
    //    #ifdef DEV_MODE
    //    case TINYMAIX_IPC_GET_MODEL_KEY:
    //        // Handle debug request
    //        break;
    //    #endif
    // }
}
```

## Cryptographic Services

TF-M provides PSA Crypto services, often leveraging hardware acceleration if available. Key management is a critical aspect.

### HUK (Hardware Unique Key) Based Key Derivation
A Hardware Unique Key (HUK) is a device-specific secret key, ideally provisioned in hardware. It serves as a root of trust for deriving other keys.
```c
// Example: Deriving a model encryption key from HUK
// This function would typically reside within a Secure Partition.
#include "psa/crypto.h"
#include "tfm_plat_crypto_keys.h" // For TFM_BUILTIN_KEY_ID_HUK (platform specific)
#include <string.h>

psa_status_t derive_key_from_huk(const uint8_t* label, size_t label_len,
                                 uint8_t* derived_key, size_t derived_key_len) {
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t status;
    psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t huk_key_id; // This might be a volatile key slot for HUK

    // For TF-M, HUK is often wrapped and accessed via a specific key ID.
    // This example assumes TFM_BUILTIN_KEY_ID_HUK is available.
    // If not, HUK needs to be imported into a volatile slot first if it's raw.
    // This step might be platform-dependent or handled by TF-M internally.
    // For simplicity, let's assume HUK is accessible via a pre-defined ID.
    // If TFM_BUILTIN_KEY_ID_HUK is a direct ID:
    huk_key_id = TFM_BUILTIN_KEY_ID_HUK; // Or a more generic PSA_KEY_ID_...

    // Setup key derivation (e.g., HKDF-SHA256)
    status = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (status != PSA_SUCCESS) return status;

    // Input the HUK as the secret for derivation
    status = psa_key_derivation_input_key(&op, PSA_KEY_DERIVATION_INPUT_SECRET, huk_key_id);
    if (status != PSA_SUCCESS) goto cleanup;

    // Input the label (context/salt)
    status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO, label, label_len);
    if (status != PSA_SUCCESS) goto cleanup;

    // Output the derived key
    status = psa_key_derivation_output_bytes(&op, derived_key, derived_key_len);

cleanup:
    psa_key_derivation_abort(&op);
    // If HUK was imported into a volatile slot, destroy it here.
    return status;
}
```

### AES-CBC Model Decryption Example
Secure Partitions can use PSA Crypto APIs to decrypt data, such as an encrypted ML model.
```c
// Example: Decrypting a model using AES-CBC (within a Secure Partition)
#include "psa/crypto.h"
#include <string.h>

psa_status_t decrypt_data_cbc(const uint8_t* key_material, size_t key_bits, // e.g., 128 for AES-128
                              const uint8_t* iv, size_t iv_len,             // IV for CBC, e.g., 16 bytes
                              const uint8_t* encrypted_data, size_t encrypted_data_len,
                              uint8_t* decrypted_data_buffer, size_t buffer_size,
                              size_t* decrypted_data_len) {
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = PSA_KEY_ID_NULL;
    psa_status_t status;
    psa_cipher_operation_t op = PSA_CIPHER_OPERATION_INIT;

    // Set key attributes
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, key_bits);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING); // Or PKCS7 if used

    // Import the raw key material into a volatile key slot
    status = psa_import_key(&attributes, key_material, PSA_BITS_TO_BYTES(key_bits), &key_id);
    if (status != PSA_SUCCESS) return status;

    // Setup decryption operation
    status = psa_cipher_decrypt_setup(&op, key_id, PSA_ALG_CBC_NO_PADDING); // Or PKCS7
    if (status != PSA_SUCCESS) goto cleanup_key;

    // Set the IV
    if (iv_len > 0) { // IV is required for CBC
        status = psa_cipher_set_iv(&op, iv, iv_len);
        if (status != PSA_SUCCESS) goto cleanup_op;
    }

    // Perform decryption
    status = psa_cipher_update(&op, encrypted_data, encrypted_data_len,
                               decrypted_data_buffer, buffer_size, decrypted_data_len);
    if (status != PSA_SUCCESS) goto cleanup_op;

    // Finalize decryption (handles padding if PKCS7 was used)
    size_t final_len;
    status = psa_cipher_finish(&op, decrypted_data_buffer + *decrypted_data_len,
                               buffer_size - *decrypted_data_len, &final_len);
    if (status == PSA_SUCCESS) {
        *decrypted_data_len += final_len;
    }

cleanup_op:
    psa_cipher_abort(&op);
cleanup_key:
    psa_destroy_key(key_id); // Destroy the imported key
    return status;
}
```

## DEV_MODE vs. Production Mode

TF-M builds can often be configured for `DEV_MODE` (or a similar debug build flag).
This mode typically enables:
*   Easier debugging (e.g., less aggressive optimizations, more logging).
*   Potentially insecure features for development convenience, such as exposing HUK-derived keys or allowing simpler attestation.
**`DEV_MODE` should NEVER be used for production devices.**

### Compile-Time Distinctions
```c
// Example of DEV_MODE specific code in a Secure Partition
#ifdef DEV_MODE
    // Enable a debug-only IPC call to get a derived key
    case TINYMAIX_IPC_GET_MODEL_KEY: // Hypothetical debug IPC type
        // ... logic to handle the request and send back the key ...
        // This is highly insecure and only for development.
        DEBUG_LOG("DEV_MODE: Servicing GET_MODEL_KEY request.\n");
        status = handle_get_model_key_debug(&msg); // Example handler
        break;

    // More verbose logging
    #define DEBUG_LOG(fmt, ...) tfm_log_printf("[DEBUG] " fmt, ##__VA_ARGS__)
#else
    // In production, GET_MODEL_KEY would be an invalid/unsupported request.
    #define DEBUG_LOG(fmt, ...) // No-op
#endif
```

### Test Execution Differences
Tests might run differently based on the build mode.
```c
// Example in a Non-Secure test application
void run_all_tests(void* arg) {
    (void)arg;
#ifdef DEV_MODE
    printf("DEV_MODE: Running HUK key derivation test and limited functionality tests.\r\n");
    // test_tinymaix_get_model_key(); // Test the debug feature
    // test_basic_inference();
#else
    printf("Production Mode: Running full suite of security and functionality tests.\r\n");
    // test_echo_service_full();
    // test_psa_crypto_operations();
    // test_secure_storage();
    // test_tinymaix_inference_secure();
    // ... and other production-relevant tests
#endif
}
```

## Performance Considerations

### TrustZone Context Switching Overhead
Calls between NSPE and SPE (PSA IPC calls) involve a context switch, which has a performance cost.
*   Minimize frequent, small data transfers.
*   Batch operations where possible. For example, send multiple inference inputs or process a larger data chunk in a single PSA call if the service design allows.
```c
// Conceptual: Batching requests to a service
typedef struct batch_inference_request {
    uint32_t num_inputs;
    // Pointers or offsets to input data arrays
    // Pointers or offsets to output data buffers
} batch_inference_request_t;

// The service would then loop through num_inputs to process them.
// psa_call(handle, ..., &batch_request, ...);
```

### Memory Optimization
*   Secure Partitions have limited stack and heap sizes, defined in their manifests. Optimize memory usage carefully.
*   Use static allocation where feasible in SPE.
*   Consider shared memory regions (NSPE-SPE) for large data transfers if the security model permits and TF-M configuration supports it (e.g., using `psa_map_shared_memory` / `psa_unmap_shared_memory`).

```yaml
# Example partition manifest memory allocation (in partition.psa.json or similar)
# Echo Service (simple)
#   "stack_size": "0x400"     # 1KB
#
# TinyMaix Inference Service (more complex)
#   "stack_size": "0x1000"    # 4KB
#   "heap_size": "0x8000"     # 32KB for model buffer, etc. (if dynamic allocation is used)
#   "mmio_regions": [...]     # If accessing peripherals directly
```

## Debugging and Monitoring

### Secure Partition Logging
TF-M provides logging mechanisms for SPE.
```c
// Using TF-M logging macros within a Secure Partition
#include "tfm_log.h" // Or specific logging header like "tfm_hal_log.h"

void my_secure_function() {
    LOG_MSG("This is a generic log message from SPE.\r\n"); // May map to tfm_hal_output_log
    // Depending on TF-M version and HAL, specific macros might be:
    // TFM_LOG_PRINTF("Secure function called with arg: %d\r\n", arg);
    // Or platform-specific logging via HAL.
    // For unprivileged components (like App RoT partitions):
    // #include "tfm_spm_log.h"
    // SPM_LOG_INFO_S("Info from secure partition\r\n");
    // SPM_LOG_ERR_S("Error in secure partition\r\n");

    // Check TF-M documentation for the correct logging API for your version/config.
    // Often, a simple printf-like function is retargeted for SPE.
}
```
Log verbosity is typically controlled by build flags (e.g., `TFM_LOG_LEVEL`).

### Non-Secure Logging
Standard `printf` or RTOS-specific logging can be used in NSPE, usually outputting via UART.

### Performance Profiling (Conceptual)
Measure execution time of critical sections, especially PSA calls and secure service operations.
```c
// Example: Measuring inference time in NSPE (client-side)
// Requires an RTOS tick or cycle counter.
uint32_t start_tick = osKernelGetTickCount(); // CMSIS-RTOS example

// psa_call to TinyMaix service for inference...

uint32_t end_tick = osKernelGetTickCount();
uint32_t duration_ms = end_tick - start_tick; // Assuming 1ms tick

printf("Inference (including IPC) took: %lu ms\r\n", duration_ms);

// Within SPE, similar timing can be done if a secure timer is available.
```

## Configuration Parameters

### Key Build Options (CMake Example)
```cmake
# Platform and Board (example for Pico2W with RP2040, adjust for RP2350)
# For RP2350, TFM_PLATFORM might be something like "arm/corstone/corstone300" or a custom one.
# This project uses a custom platform definition.
# -DTFM_PLATFORM=trustedfirmware/pico2w (as per project structure)

# Security Profile (influences which services and features are enabled)
# -DTFM_PROFILE=profile_small
# -DTFM_PROFILE=profile_medium (Commonly used)
# -DTFM_PROFILE=profile_large

# Enable/Disable specific Application RoT Partitions
# -DTFM_PARTITION_ECHO_SERVICE=ON
# -DTFM_PARTITION_TINYMAIX_INFERENCE=ON
# (These are typically controlled in partition_list.cmake or similar)

# Development/Debug Mode
# -DDEV_MODE=ON  # Custom flag for this project, enables debug features
                 # TF-M might use CMAKE_BUILD_TYPE=Debug for similar effects
# -DCMAKE_BUILD_TYPE=Debug # Standard CMake flag

# Isolation Level
# -DTFM_ISOLATION_LEVEL=1 # Level 1: SPM and Partitions share privileges
# -DTFM_ISOLATION_LEVEL=2 # Level 2: Partitions are unprivileged
# -DTFM_ISOLATION_LEVEL=3 # Level 3: Level 2 + peripheral partitioning (if supported)
```

### Security Profiles
*   **Profile Small**: Minimal set of PSA RoT services, smallest footprint.
*   **Profile Medium**: Balanced set of services, suitable for many IoT use cases.
*   **Profile Large**: Most PSA RoT services enabled, largest footprint.
The choice of profile affects available PSA APIs and overall security capabilities.

## Next Steps

With an understanding of the TF-M architecture in this project:
- Explore **[Secure Partitions Development](./secure-partitions.md)** for details on creating and managing custom secure services like Echo and TinyMaix.
- Refer to **[PSA API Usage](./psa-api.md)** for patterns in client-service communication.
- Understand the **[Build System](./build-system.md)** to compile and deploy the project.
- Consult the **[Model Encryption Guide](./model-encryption.md)** for how ML models are secured.
- Review the **[Testing Framework](./testing-framework.md)** to see how these components are verified.
- Check the **[Troubleshooting Guide](./troubleshooting.md)** for common issues.