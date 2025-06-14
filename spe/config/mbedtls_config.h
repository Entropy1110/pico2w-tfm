/*
 * MbedTLS configuration for TF-M with AES-CBC support
 */

#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* Use PSA crypto configuration */
#define MBEDTLS_PSA_CRYPTO_CONFIG

/* Basic platform configuration */
#define MBEDTLS_PLATFORM_C

/* AES and CBC support */
#define MBEDTLS_AES_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CIPHER_MODE_CBC

/* Enable cipher padding */
#define MBEDTLS_CIPHER_PADDING_PKCS7

/* Memory management */
#define MBEDTLS_PLATFORM_MEMORY

/* PSA crypto support */
#define MBEDTLS_PSA_CRYPTO_C

/* Error handling */
#define MBEDTLS_ERROR_C

#endif /* MBEDTLS_CONFIG_H */