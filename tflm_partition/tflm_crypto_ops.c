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
    0xC6, 0x9C, 0xE1, 0xD0,
    0x9F, 0xE9, 0xCD, 0x85,
    0xD9, 0x52, 0x80, 0x14,
    0xCC, 0x7D, 0x38, 0x26,
    0xB4, 0x0F, 0x01, 0xBF,
    0xE9, 0x99, 0x1F, 0x4D,
    0xC8, 0xDF, 0x2B, 0xBB,
    0x8C, 0xFB, 0xBC, 0x47
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
    psa_cipher_operation_t op = PSA_CIPHER_OPERATION_INIT;
    size_t iv_size = psa_cipher_iv_length(PSA_ALG_CBC_NO_PADDING);
    size_t cipher_size;
    uint8_t *output = NULL;
    size_t output_len = 0, finish_len = 0;

    if (encrypted_size <= iv_size) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Encrypted data too small");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* 1) IV / Cipher 분리 */
    const uint8_t *iv     = encrypted_data;
    const uint8_t *cipher = encrypted_data + iv_size;
    cipher_size = encrypted_size - iv_size;

    /* 2) 출력 버퍼 할당 (같은 크기) */
    output = (uint8_t *)malloc(cipher_size);
    if (!output) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Memory alloc failed");
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }

    /* 3) 복호화 연산 설정 */
    status = psa_cipher_decrypt_setup(&op, model_key_handle, PSA_ALG_CBC_NO_PADDING);
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Decrypt setup failed: %d", status);
        goto cleanup;
    }

    /* 4) IV 설정 */
    status = psa_cipher_set_iv(&op, iv, iv_size);
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Set IV failed: %d", status);
        goto cleanup;
    }

    /* 5) ciphertext → plaintext */
    status = psa_cipher_update(&op, cipher, cipher_size,
                               output, cipher_size, &output_len);
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Decrypt update failed: %d", status);
        goto cleanup;
    }

    /* 6) 마무리 (block alignment 확인) */
    status = psa_cipher_finish(&op, output + output_len,
                               cipher_size - output_len, &finish_len);
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Decrypt finish failed: %d", status);
        goto cleanup;
    }
    output_len += finish_len;

    /* 7) PKCS#7 패딩 제거 */
    if (output_len == 0 || output_len % iv_size != 0) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Invalid plaintext length");
        status = PSA_ERROR_INVALID_SIGNATURE;
        goto cleanup;
    }
    uint8_t pad = output[output_len - 1];
    if (pad == 0 || pad > iv_size) {
        INFO_UNPRIV_RAW("[TFLM Crypto] Bad padding value");
        status = PSA_ERROR_INVALID_SIGNATURE;
        goto cleanup;
    }
    /* 패딩 바이트 검사 */
    for (size_t i = 0; i < pad; i++) {
        if (output[output_len - 1 - i] != pad) {
            INFO_UNPRIV_RAW("[TFLM Crypto] Padding check failed");
            status = PSA_ERROR_INVALID_SIGNATURE;
            goto cleanup;
        }
    }
    *decrypted_size = output_len - pad;
    *decrypted_data = output;
    status = PSA_SUCCESS;

cleanup:
    psa_cipher_abort(&op);
    if (status != PSA_SUCCESS) {
        free(output);
    }
    return status;
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
