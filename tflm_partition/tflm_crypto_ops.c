/*
 * Copyright (c) 2024, Your Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tflm_crypto_ops.h"
#include "psa/crypto.h"
#include "tfm_log_unpriv.h"
#include <string.h>
#include <stdlib.h>

/* Model decryption key handle */
static psa_key_handle_t model_key_handle = 0;

/* Dummy key for testing - in production, this should be securely provisioned */
static const uint8_t model_key_data[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

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

    /* Set up key attributes for AES-256 */
    psa_set_key_usage_flags(&attributes, 
                            PSA_KEY_USAGE_DECRYPT | PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 256);

    /* Import the key */
    status = psa_import_key(&attributes, model_key_data, sizeof(model_key_data),
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
    size_t output_length;

    INFO_UNPRIV_RAW("[TFLM Crypto] Decrypting model, size: %d", encrypted_size);

    /* For now, just copy the data (dummy implementation) */
    /* In production, implement actual AES decryption */
    *decrypted_data = (uint8_t *)malloc(encrypted_size);
    if (*decrypted_data == NULL) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Failed to allocate memory for decrypted data");
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }

    /* Dummy decryption - just copy the data */
    memcpy(*decrypted_data, encrypted_data, encrypted_size);
    *decrypted_size = encrypted_size;

    INFO_UNPRIV_RAW("[TFLM Crypto] Model decrypted successfully");
    return PSA_SUCCESS;
}

psa_status_t tflm_encrypt_output(const uint8_t *plain_data,
                                 size_t plain_size,
                                 uint8_t *encrypted_data,
                                 size_t *encrypted_size)
{
    INFO_UNPRIV_RAW("[TFLM Crypto] Encrypting output, size: %d", plain_size);

    /* For now, just copy the data (dummy implementation) */
    memcpy(encrypted_data, plain_data, plain_size);
    *encrypted_size = plain_size;

    INFO_UNPRIV_RAW("[TFLM Crypto] Output encrypted successfully");
    return PSA_SUCCESS;
}
