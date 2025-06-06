/*
 * Copyright (c) 2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include "psa/crypto.h"
#include "tfm_ns_interface.h"

#define TEST_DATA_SIZE 64
#define AES_KEY_SIZE 16  /* 128-bit AES key */

/**
 * \brief Test PSA symmetric encryption/decryption
 */
void test_psa_encryption(void)
{
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = 0;
    
    /* Test data */
    uint8_t plaintext[TEST_DATA_SIZE] = "Hello PSA Crypto! This is a test message for encryption.";
    uint8_t ciphertext[TEST_DATA_SIZE + 16]; /* Extra space for padding */
    uint8_t decrypted[TEST_DATA_SIZE];
    size_t ciphertext_length = 0;
    size_t decrypted_length = 0;
    
    /* AES-128 key */
    uint8_t key_data[AES_KEY_SIZE] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    };

    printf("\n=== PSA Crypto Encryption Test ===\n");

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("❌ PSA crypto initialization failed: %d\n", (int)status);
        return;
    }
    printf("✅ PSA crypto initialized\n");

    /* Set up key attributes */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_PKCS7);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);

    /* Import the key */
    status = psa_import_key(&attributes, key_data, sizeof(key_data), &key_id);
    if (status != PSA_SUCCESS) {
        printf("❌ Key import failed: %d\n", (int)status);
        return;
    }
    printf("✅ AES-128 key imported (ID: %u)\n", (unsigned int)key_id);

    /* Test 1: Encryption */
    printf("\nTest 1: AES-CBC-PKCS7 Encryption\n");
    printf("Plaintext:  '%s'\n", (char*)plaintext);
    
    status = psa_cipher_encrypt(key_id, PSA_ALG_CBC_PKCS7,
                               plaintext, strlen((char*)plaintext),
                               ciphertext, sizeof(ciphertext),
                               &ciphertext_length);
    
    if (status != PSA_SUCCESS) {
        printf("❌ Encryption failed: %d\n", (int)status);
        goto cleanup;
    }
    
    printf("✅ Encryption successful, ciphertext length: %zu bytes\n", ciphertext_length);
    printf("Ciphertext: ");
    for (size_t i = 0; i < ciphertext_length; i++) {
        printf("%02x", ciphertext[i]);
    }
    printf("\n");

    /* Test 2: Decryption */
    printf("\nTest 2: AES-CBC-PKCS7 Decryption\n");
    
    status = psa_cipher_decrypt(key_id, PSA_ALG_CBC_PKCS7,
                               ciphertext, ciphertext_length,
                               decrypted, sizeof(decrypted),
                               &decrypted_length);
    
    if (status != PSA_SUCCESS) {
        printf("❌ Decryption failed: %d\n", (int)status);
        goto cleanup;
    }
    
    decrypted[decrypted_length] = '\0'; /* Null terminate */
    printf("✅ Decryption successful, decrypted length: %zu bytes\n", decrypted_length);
    printf("Decrypted:  '%s'\n", (char*)decrypted);

    /* Verify the results */
    if (strlen((char*)plaintext) == decrypted_length && 
        memcmp(plaintext, decrypted, decrypted_length) == 0) {
        printf("✅ Encryption/Decryption test PASSED\n");
    } else {
        printf("❌ Encryption/Decryption test FAILED - data mismatch\n");
    }

cleanup:
    /* Clean up the key */
    psa_destroy_key(key_id);
    printf("🔑 Key destroyed\n");
    
    printf("=== PSA Crypto Test Complete ===\n\n");
}

/**
 * \brief Test PSA hash operations
 */
void test_psa_hash(void)
{
    psa_status_t status;
    psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
    
    uint8_t input[] = "Hello PSA Hash!";
    uint8_t hash[PSA_HASH_LENGTH(PSA_ALG_SHA_256)];
    size_t hash_length = 0;

    printf("\n=== PSA Hash Test ===\n");
    printf("Input: '%s'\n", (char*)input);

    /* Compute SHA-256 hash */
    status = psa_hash_compute(PSA_ALG_SHA_256,
                             input, strlen((char*)input),
                             hash, sizeof(hash),
                             &hash_length);

    if (status != PSA_SUCCESS) {
        printf("❌ Hash computation failed: %d\n", (int)status);
        return;
    }

    printf("✅ SHA-256 hash computed, length: %zu bytes\n", hash_length);
    printf("Hash: ");
    for (size_t i = 0; i < hash_length; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");

    printf("=== PSA Hash Test Complete ===\n\n");
}
