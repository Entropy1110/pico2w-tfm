/*
 * Copyright (c) 2024, Your Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tflm_crypto_ops.h"
#include "encrypted_audio_model.h"
#include "psa/crypto.h"
#include "tfm_log_unpriv.h"
#include <string.h>
#include <stdlib.h>

/* Model decryption key handle */
static psa_key_handle_t model_key_handle = 0;

/* Encrypted model package structure */
typedef struct {
    uint32_t magic;            // 4B: Magic header "TFLM"
    uint32_t version;          // 4B: Version
    uint32_t original_size;    // 4B: Original model size
    uint32_t encrypted_size;   // 4B: Encrypted data size
    uint8_t model_hash[32];    // 32B: SHA-256 hash
    uint8_t nonce[12];         // 12B: GCM nonce
    uint8_t auth_tag[16];      // 16B: GCM auth tag
    uint8_t model_name[32];    // 32B: Model name (null-padded)
    // Variable length encrypted data follows
} __attribute__((packed)) encrypted_model_header_t;

psa_status_t tflm_crypto_init(void)
{
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

    INFO_UNPRIV_RAW("[TFLM Crypto] Initializing crypto operations");

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Failed to initialize PSA Crypto: %d", status);
        return status;
    }

    /* Set up key attributes for AES-256-GCM */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_GCM);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 256);

    /* Import the encrypted model key */
    status = psa_import_key(&attributes, 
                            encrypted_audio_preprocessor_int8_key, 
                            encrypted_audio_preprocessor_int8_key_size,
                            &model_key_handle);
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Failed to import key: %d", status);
        return status;
    }

    INFO_UNPRIV_RAW("[TFLM Crypto] Crypto operations initialized successfully");
    return PSA_SUCCESS;
}

psa_status_t tflm_decrypt_model(const uint8_t *encrypted_data,
                                size_t encrypted_size,
                                uint8_t **decrypted_data,
                                size_t *decrypted_size)
{
    psa_status_t status;
    const encrypted_model_header_t *header;
    const uint8_t *encrypted_payload;
    uint8_t *output = NULL;
    size_t output_len = 0;

    INFO_UNPRIV_RAW("[TFLM Crypto] Starting model decryption");

    /* Validate input parameters */
    if (!encrypted_data || !decrypted_data || !decrypted_size) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Invalid parameters");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* Check minimum size for header */
    if (encrypted_size < sizeof(encrypted_model_header_t)) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Encrypted data too small");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* Parse encrypted model header */
    header = (const encrypted_model_header_t *)encrypted_data;
    
    /* Verify magic header */
    if (header->magic != 0x4D4C4654) { // "TFLM" in little endian
        INFO_UNPRIV_RAW("[TFLM Crypto] Invalid magic header: 0x%08x", header->magic);
        return PSA_ERROR_INVALID_SIGNATURE;
    }

    /* Verify version */
    if (header->version != 1) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Unsupported version: %d", header->version);
        return PSA_ERROR_NOT_SUPPORTED;
    }

    /* Get encrypted payload */
    encrypted_payload = encrypted_data + sizeof(encrypted_model_header_t);
    
    /* Verify encrypted data size */
    if (encrypted_size != sizeof(encrypted_model_header_t) + header->encrypted_size) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Size mismatch");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* Allocate output buffer */
    output = malloc(header->original_size);
    if (!output) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Memory allocation failed");
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }

    /* Decrypt using AES-GCM */
    status = psa_aead_decrypt(model_key_handle,
                              PSA_ALG_GCM,
                              header->nonce, 12,           // nonce
                              NULL, 0,                     // additional data
                              encrypted_payload, header->encrypted_size,
                              output, header->original_size,
                              &output_len);

    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Decryption failed: %d", status);
        free(output);
        return status;
    }

    /* Verify decrypted size matches expected */
    if (output_len != header->original_size) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Decrypted size mismatch");
        free(output);
        return PSA_ERROR_INVALID_SIGNATURE;
    }

    *decrypted_data = output;
    *decrypted_size = output_len;

    INFO_UNPRIV_RAW("[TFLM Crypto] Model decrypted successfully: %d bytes", output_len);
    return PSA_SUCCESS;
}

psa_status_t tflm_encrypt_data(const uint8_t *plain_data,
                               size_t plain_size,
                               uint8_t **encrypted_data,
                               size_t *encrypted_size)
{
    /* This function is not needed since encryption is done at build time */
    INFO_UNPRIV_RAW("[TFLM Crypto] Encryption not supported in secure partition");
    return PSA_ERROR_NOT_SUPPORTED;
}

void tflm_crypto_cleanup(void)
{
    INFO_UNPRIV_RAW("[TFLM Crypto] Cleaning up crypto operations");
    
    if (model_key_handle != 0) {
        psa_destroy_key(model_key_handle);
        model_key_handle = 0;
    }
}
