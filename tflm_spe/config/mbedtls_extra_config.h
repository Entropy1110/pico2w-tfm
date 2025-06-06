/*
 * Copyright (c) 2022-2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef MBEDTLS_EXTRA_CONFIG_H
#define MBEDTLS_EXTRA_CONFIG_H

/*
 * Extra configuration options for MbedTLS to optimize for TFLM application with PSA encryption
 */

/* Disable unnecessary features for TFLM apps */
#undef MBEDTLS_SSL_TLS_C
#undef MBEDTLS_SSL_CLI_C  
#undef MBEDTLS_SSL_SRV_C
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C

/* Keep essential crypto features */
#define MBEDTLS_PSA_CRYPTO_C
#define MBEDTLS_USE_PSA_CRYPTO

/* Enable required MbedTLS support for AEAD and hash */
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CCM_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA1_C

/* PSA crypto algorithm support for AEAD (authenticated encryption) */
#define PSA_WANT_ALG_GCM               1
#define PSA_WANT_ALG_CCM               1

/* PSA crypto key types for symmetric encryption */
#define PSA_WANT_KEY_TYPE_RAW_DATA     1
#define PSA_WANT_KEY_TYPE_AES          1
#define PSA_WANT_KEY_TYPE_DERIVE       1
#define PSA_WANT_KEY_TYPE_HMAC         1

/* PSA crypto hash algorithms */
#define PSA_WANT_ALG_SHA_256           1
#define PSA_WANT_ALG_SHA_224           1
#define PSA_WANT_ALG_SHA_1             1

/* PSA crypto key derivation */
#define PSA_WANT_ALG_HKDF              1

#endif /* MBEDTLS_EXTRA_CONFIG_H */
