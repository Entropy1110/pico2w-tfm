# PSA API Guide for pico2w-tfm-tflm

## 1. Introduction

The Platform Security Architecture (PSA) provides a framework for designing and building secure embedded systems. Central to this is the PSA Functional API, which offers a standardized interface for accessing security services. In the `pico2w-tfm-tflm` project, these APIs are crucial for implementing secure functionalities, particularly those involving cryptographic operations, secure storage, and attestation, all managed within the Trusted Firmware-M (TF-M) environment.

This document outlines the key PSA APIs utilized in this project and provides guidance on their usage.

## 2. Core PSA APIs Utilized

The `pico2w-tfm-tflm` project leverages several PSA Functional APIs provided by TF-M. These APIs enable secure operations by abstracting the underlying hardware and software security mechanisms.

### 2.1. PSA Cryptography API

The PSA Cryptography API provides a portable interface for cryptographic operations. It supports a wide range of cryptographic primitives, including hashing, MAC (Message Authentication Code) generation and verification, symmetric and asymmetric encryption/decryption, and key agreement.

**Key Functions and Concepts:**

*   **Key Management:**
    *   `psa_import_key()`: Imports a key in binary format into the PSA crypto service.
    *   `psa_destroy_key()`: Destroys a key from the PSA crypto service.
    *   `psa_generate_key()`: Generates a new key.
    *   `psa_export_key()`: Exports a key in binary format.
    *   `psa_export_public_key()`: Exports a public key.
*   **Hashing:**
    *   `psa_hash_setup()`: Sets up a hash operation.
    *   `psa_hash_update()`: Adds data to an ongoing hash operation.
    *   `psa_hash_finish()`: Finalizes a hash operation and retrieves the hash.
    *   `psa_hash_compare()`: Compares a calculated hash with an expected value.
*   **MAC Operations:**
    *   `psa_mac_sign_setup()`: Sets up a MAC generation operation.
    *   `psa_mac_verify_setup()`: Sets up a MAC verification operation.
    *   `psa_mac_update()`: Adds data to an ongoing MAC operation.
    *   `psa_mac_sign_finish()`: Finalizes MAC generation.
    *   `psa_mac_verify_finish()`: Finalizes MAC verification.
*   **Symmetric Ciphers (AES):**
    *   `psa_cipher_encrypt_setup()`: Sets up a symmetric encryption operation.
    *   `psa_cipher_decrypt_setup()`: Sets up a symmetric decryption operation.
    *   `psa_cipher_generate_iv()`: Generates an Initialization Vector (IV).
    *   `psa_cipher_set_iv()`: Sets the IV for a cipher operation.
    *   `psa_cipher_update()`: Processes data through the cipher.
    *   `psa_cipher_finish()`: Finalizes the cipher operation.
*   **Asymmetric Cryptography (RSA, ECC):**
    *   `psa_sign_hash()`: Signs a pre-calculated hash.
    *   `psa_verify_hash()`: Verifies a signature against a pre-calculated hash.
    *   `psa_asymmetric_encrypt()`: Encrypts data using an asymmetric public key.
    *   `psa_asymmetric_decrypt()`: Decrypts data using an asymmetric private key.
*   **Key Derivation:**
    *   `psa_key_derivation_setup()`: Sets up a key derivation operation.
    *   `psa_key_derivation_input_key()`: Inputs a key into the derivation process.
    *   `psa_key_derivation_input_bytes()`: Inputs other data (e.g., salt, info) into the derivation.
    *   `psa_key_derivation_output_key()`: Derives a new key.
    *   `psa_key_derivation_output_bytes()`: Derives raw bytes.

**Usage Example (AES-CBC Encryption):**
```c
#include "psa/crypto.h"
#include <string.h>

// Example: Encrypt data using AES-128-CBC
psa_status_t encrypt_data_aes_cbc(
    const uint8_t *key_data, size_t key_data_size,
    const uint8_t *iv, size_t iv_size,
    const uint8_t *plaintext, size_t plaintext_size,
    uint8_t *ciphertext, size_t ciphertext_buffer_size, size_t *ciphertext_length)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;
    psa_status_t status;
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;

    // Set key attributes
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, PSA_BYTES_TO_BITS(key_data_size));
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING); // Or PSA_ALG_CBC_PKCS7

    // Import the key
    status = psa_import_key(&attributes, key_data, key_data_size, &key_id);
    if (status != PSA_SUCCESS) {
        return status;
    }

    // Setup encryption operation
    status = psa_cipher_encrypt_setup(&operation, key_id, PSA_ALG_CBC_NO_PADDING);
    if (status != PSA_SUCCESS) {
        goto cleanup_key;
    }

    // Set IV
    if (iv_size > 0) {
        status = psa_cipher_set_iv(&operation, iv, iv_size);
        if (status != PSA_SUCCESS) {
            goto cleanup_op;
        }
    }

    // Encrypt
    status = psa_cipher_update(&operation, plaintext, plaintext_size,
                               ciphertext, ciphertext_buffer_size, ciphertext_length);
    if (status != PSA_SUCCESS) {
        goto cleanup_op;
    }

    // Finalize encryption (handles padding if PKCS7 is used)
    size_t final_output_len = 0;
    status = psa_cipher_finish(&operation, ciphertext + *ciphertext_length,
                               ciphertext_buffer_size - *ciphertext_length, &final_output_len);
    *ciphertext_length += final_output_len;

cleanup_op:
    psa_cipher_abort(&operation);
cleanup_key:
    psa_destroy_key(key_id);
    return status;
}
```

### 2.2. PSA Protected Storage API

The PSA Protected Storage API provides a means to store small amounts of data (assets) with confidentiality, integrity, and authenticity. It is typically used for storing sensitive data like cryptographic keys, configuration settings, or device identity information.

**Key Functions and Concepts:**

*   **UID (Unique Identifier):** Each asset is identified by a unique 64-bit integer.
*   **Create Flags:** Control the access and properties of the asset (e.g., write-once).
    *   `PSA_STORAGE_FLAG_NONE`
    *   `PSA_STORAGE_FLAG_WRITE_ONCE`
*   **Operations:**
    *   `psa_ps_set()`: Creates a new asset or modifies an existing one.
    *   `psa_ps_get()`: Retrieves an asset.
    *   `psa_ps_get_info()`: Retrieves metadata about an asset (size, flags).
    *   `psa_ps_remove()`: Deletes an asset.

**Usage Example:**
```c
#include "psa/protected_storage.h"
#include <string.h>

// Example: Store and retrieve a secret
psa_status_t store_and_retrieve_secret(psa_storage_uid_t uid,
                                       const uint8_t *secret_data, size_t secret_size)
{
    psa_status_t status;

    // Store the secret
    status = psa_ps_set(uid, secret_size, secret_data, PSA_STORAGE_FLAG_NONE);
    if (status != PSA_SUCCESS) {
        // Handle error (e.g., PSA_ERROR_INSUFFICIENT_SPACE, PSA_ERROR_ALREADY_EXISTS)
        return status;
    }

    // Retrieve the secret
    uint8_t retrieved_buffer[64]; // Ensure buffer is large enough
    size_t retrieved_length = 0;
    status = psa_ps_get(uid, 0, sizeof(retrieved_buffer), retrieved_buffer, &retrieved_length);
    if (status != PSA_SUCCESS) {
        // Handle error
        return status;
    }

    if (retrieved_length != secret_size || memcmp(secret_data, retrieved_buffer, secret_size) != 0) {
        // Data mismatch
        return PSA_ERROR_CORRUPTION_DETECTED;
    }

    // Optionally, remove the secret
    // status = psa_ps_remove(uid);
    // if (status != PSA_SUCCESS) { /* Handle error */ }

    return PSA_SUCCESS;
}
```

### 2.3. PSA Internal Trusted Storage (ITS) API

The PSA Internal Trusted Storage API is similar to Protected Storage but is designed for data that does not require the same level of protection against sophisticated physical attacks. It's suitable for storing data that needs integrity and rollback protection but where confidentiality might be less critical or handled by other means (e.g., application-level encryption).

**Key Functions and Concepts:**

*   The API functions are analogous to Protected Storage:
    *   `psa_its_set()`
    *   `psa_its_get()`
    *   `psa_its_get_info()`
    *   `psa_its_remove()`
*   ITS is generally faster than PS due to potentially simpler protection mechanisms.

**Note:** The choice between PS and ITS depends on the security requirements of the data being stored. For highly sensitive data like private keys, PS is preferred.

### 2.4. PSA Initial Attestation API

The PSA Initial Attestation API allows the device to prove its identity and current software state to a relying party. It generates a token containing claims about the device, signed by a unique Attestation Key.

**Key Functions and Concepts:**

*   **Attestation Token:** A data structure (e.g., CBOR-encoded) containing claims.
*   **Claims:** Information about the device, such as:
    *   Instance ID (unique device identifier)
    *   Boot status / Lifecycle state
    *   Software component measurements (hashes of firmware images)
    *   Hardware version
*   **Operations:**
    *   `psa_initial_attest_get_token()`: Retrieves the attestation token.
    *   `psa_initial_attest_get_token_size()`: Gets the required buffer size for the token.

**Usage Example:**
```c
#include "psa/initial_attestation.h"

// Example: Get an attestation token
psa_status_t get_attestation_token(const uint8_t *challenge, size_t challenge_size,
                                   uint8_t *token_buffer, size_t token_buffer_size,
                                   size_t *token_size)
{
    psa_status_t status;

    // Get the size of the token
    status = psa_initial_attest_get_token_size(challenge_size, token_size);
    if (status != PSA_SUCCESS) {
        return status;
    }

    if (*token_size > token_buffer_size) {
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }

    // Get the token
    status = psa_initial_attest_get_token(challenge, challenge_size,
                                          token_buffer, token_buffer_size, token_size);
    if (status != PSA_SUCCESS) {
        // Handle error
        return status;
    }

    return PSA_SUCCESS;
}
```

## 3. PSA API Usage in `pico2w-tfm-tflm`

In the `pico2w-tfm-tflm` project, PSA APIs are primarily used for:

*   **Model Encryption/Decryption:** The `model-encryption.md` guide details how HUK (Hardware Unique Key) is used via PSA key derivation (`psa_key_derivation_setup`, `psa_key_derivation_input_key`, `psa_key_derivation_output_bytes`) to generate model-specific encryption keys. The actual decryption of TinyMaix models in the secure partition uses `psa_cipher_decrypt()` with AES-CBC.
*   **Secure Storage of Keys/Credentials:** While not explicitly detailed in all examples, PSA Protected Storage could be used to store derived keys or other sensitive credentials if they need to persist across reboots.
*   **Secure Boot and Firmware Update:** TF-M itself relies heavily on PSA Crypto APIs for signature verification during secure boot and firmware updates, ensuring the integrity and authenticity of the firmware.
*   **Inter-partition Communication:** Secure services (Partitions) within TF-M expose functionalities that often wrap PSA API calls. For instance, a custom secure service might use PSA Crypto to perform a specific cryptographic task requested by the Non-Secure Processing Environment (NSPE).

## 4. Error Handling

PSA API functions return a `psa_status_t` type. `PSA_SUCCESS` (which is 0) indicates success. Any other value indicates an error. Common error codes include:

*   `PSA_ERROR_INVALID_ARGUMENT`: An invalid argument was passed to the function.
*   `PSA_ERROR_NOT_SUPPORTED`: The requested algorithm or operation is not supported.
*   `PSA_ERROR_INSUFFICIENT_MEMORY`: Not enough memory to complete the operation.
*   `PSA_ERROR_INSUFFICIENT_STORAGE`: Not enough storage space (for PS/ITS).
*   `PSA_ERROR_COMMUNICATION_FAILURE`: Error in communication with the secure service.
*   `PSA_ERROR_STORAGE_FAILURE`: Underlying storage mechanism failed.
*   `PSA_ERROR_CORRUPTION_DETECTED`: Data corruption detected.
*   `PSA_ERROR_BAD_STATE`: The operation is not permitted in the current state.
*   `PSA_ERROR_BUFFER_TOO_SMALL`: The output buffer provided is too small.
*   `PSA_ERROR_INVALID_SIGNATURE`: Signature verification failed.
*   `PSA_ERROR_INVALID_PADDING`: Padding validation failed during decryption.

It is crucial to check the return status of every PSA API call and handle errors appropriately.

## 5. Best Practices

*   **Minimize Key Exposure:** Handle key material carefully. Use key IDs (`psa_key_id_t`) instead of raw key bytes whenever possible in the application.
*   **Clear Sensitive Data:** Securely clear buffers containing keys or sensitive plaintext after use (e.g., using `memset_s` or equivalent).
*   **Use Appropriate Key Attributes:** Define key attributes (`psa_key_attributes_t`) precisely, specifying usage flags, algorithm, key type, and size to enforce security policies.
*   **Check Return Values:** Always check the return status of PSA API calls.
*   **Resource Management:** Properly initialize and release PSA operations (e.g., `psa_cipher_operation_t`, `psa_hash_operation_t`) using `_INIT` macros and `_abort()` functions in case of errors or completion. Destroy keys using `psa_destroy_key()` when no longer needed.
*   **Understand Security Implications:** Be aware of the security properties of different algorithms and modes (e.g., using authenticated encryption like AES-GCM or AES-CCM over unauthenticated modes if both confidentiality and integrity are required).

## 6. Further Reading

*   **PSA Functional API Specifications:** The official Arm PSA specifications provide the most detailed information.
    *   [PSA Cryptography API](https://developer.arm.com/documentation/ihi0086/latest/)
    *   [PSA Protected Storage API](https://developer.arm.com/documentation/ihi0087/latest/)
    *   [PSA Initial Attestation API](https://developer.arm.com/documentation/ihi0085/latest/)
*   **TF-M Documentation:** The Trusted Firmware-M documentation explains how these APIs are implemented and integrated.

This guide provides a project-specific overview. For comprehensive details, always refer to the official PSA and TF-M documentation.