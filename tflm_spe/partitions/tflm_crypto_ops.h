/*
 * Copyright (c) 2024, Your Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TFLM_CRYPTO_OPS_H__
#define __TFLM_CRYPTO_OPS_H__

#include <stdint.h>
#include <stddef.h>
#include "psa/crypto.h"
#include "psa_tflm_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize crypto operations */
psa_status_t tflm_crypto_init(void);

/* Decrypt model data */
psa_status_t tflm_decrypt_model(const uint8_t *encrypted_data,
                                 size_t encrypted_size,
                                 uint8_t **decrypted_data,
                                 size_t *decrypted_size);

/* Encrypt inference results if needed */
psa_status_t tflm_encrypt_output(const uint8_t *plain_data,
                                 size_t plain_size,
                                 uint8_t *encrypted_data,
                                 size_t *encrypted_size);

#ifdef __cplusplus
}
#endif

#endif /* __TFLM_CRYPTO_OPS_H__ */
