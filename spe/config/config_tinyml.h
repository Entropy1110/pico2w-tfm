/*
 * SPDX-FileCopyrightText: Copyright The TrustedFirmware-M Contributors
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __CONFIG_PROVISION_H__
#define __CONFIG_PROVISION_H__

/* PSA Crypto Configuration - simplified like provision project */
#define MBEDTLS_PSA_CRYPTO_CONFIG

/* Enable basic hash support (SHA-256) for HKDF */
#define PSA_WANT_ALG_SHA_256                    1

/* Enable key derivation algorithms for HUK */
#define PSA_WANT_ALG_HKDF                       1
#define PSA_WANT_ALG_HMAC                       1

/* Enable AES encryption modes for model decryption */
#define PSA_WANT_ALG_CCM                        1
#define PSA_WANT_ALG_GCM                        1

/* Enable basic key types */
#define PSA_WANT_KEY_TYPE_RAW_DATA              1
#define PSA_WANT_KEY_TYPE_DERIVE                1
#define PSA_WANT_KEY_TYPE_HMAC                  1
#define PSA_WANT_KEY_TYPE_AES                   1

/* Platform Partition Configs */

/* Size of input buffer in platform service */
#ifndef PLATFORM_SERVICE_INPUT_BUFFER_SIZE
#define PLATFORM_SERVICE_INPUT_BUFFER_SIZE     64
#endif

/* Size of output buffer in platform service */
#ifndef PLATFORM_SERVICE_OUTPUT_BUFFER_SIZE
#define PLATFORM_SERVICE_OUTPUT_BUFFER_SIZE    64
#endif

/* The stack size of the Platform Secure Partition */
#ifndef PLATFORM_SP_STACK_SIZE
#define PLATFORM_SP_STACK_SIZE                 0x500
#endif

/* Disable Non-volatile counter module */
#ifndef PLATFORM_NV_COUNTER_MODULE_DISABLED
#define PLATFORM_NV_COUNTER_MODULE_DISABLED    0
#endif

/* Crypto Partition Configs */

/*
 * Heap size for the crypto backend
 * Set to match PSA API tests configuration
 */
#ifndef CRYPTO_ENGINE_BUF_SIZE
#define CRYPTO_ENGINE_BUF_SIZE                 0x8000
#endif

/* The max number of concurrent operations that can be active (allocated) at any time in Crypto */
#ifndef CRYPTO_CONC_OPER_NUM
#define CRYPTO_CONC_OPER_NUM                   8
#endif

/* Enable PSA Crypto random number generator module */
#ifndef CRYPTO_RNG_MODULE_ENABLED
#define CRYPTO_RNG_MODULE_ENABLED              1
#endif

/* Enable PSA Crypto Key module */
#ifndef CRYPTO_KEY_MODULE_ENABLED
#define CRYPTO_KEY_MODULE_ENABLED              1
#endif

/* Enable PSA Crypto AEAD module for AES-CCM/GCM encryption/decryption */
#ifndef CRYPTO_AEAD_MODULE_ENABLED
#define CRYPTO_AEAD_MODULE_ENABLED             1
#endif

/* Enable PSA Crypto MAC module (needed for HKDF/HMAC) */
#ifndef CRYPTO_MAC_MODULE_ENABLED
#define CRYPTO_MAC_MODULE_ENABLED              1
#endif

/* Enable PSA Crypto Hash module */
#ifndef CRYPTO_HASH_MODULE_ENABLED
#define CRYPTO_HASH_MODULE_ENABLED             1
#endif

/* Enable PSA Crypto Cipher module for AES-GCM */
#ifndef CRYPTO_CIPHER_MODULE_ENABLED
#define CRYPTO_CIPHER_MODULE_ENABLED           1
#endif

/* Disable PSA Crypto asymmetric key signature module */
#ifndef CRYPTO_ASYM_SIGN_MODULE_ENABLED
#define CRYPTO_ASYM_SIGN_MODULE_ENABLED        0
#endif

/* Disable PSA Crypto asymmetric key encryption module */
#ifndef CRYPTO_ASYM_ENCRYPT_MODULE_ENABLED
#define CRYPTO_ASYM_ENCRYPT_MODULE_ENABLED     0
#endif

/* Enable PSA Crypto key derivation module (needed for HUK derivation) */
#ifndef CRYPTO_KEY_DERIVATION_MODULE_ENABLED
#define CRYPTO_KEY_DERIVATION_MODULE_ENABLED   1
#endif

/* Default size of the internal scratch buffer used for PSA FF IOVec allocations */
#ifndef CRYPTO_IOVEC_BUFFER_SIZE
#define CRYPTO_IOVEC_BUFFER_SIZE               8192
#endif

/* Use stored NV seed to provide entropy */
#ifndef CRYPTO_NV_SEED
#define CRYPTO_NV_SEED                         1
#endif

/*
 * Only enable multi-part operations in Hash, MAC, AEAD and symmetric ciphers,
 * to optimize memory footprint in resource-constrained devices.
 */
#ifndef CRYPTO_SINGLE_PART_FUNCS_DISABLED
#define CRYPTO_SINGLE_PART_FUNCS_DISABLED      0
#endif

/* The stack size of the Crypto Secure Partition */
#ifndef CRYPTO_STACK_SIZE
#define CRYPTO_STACK_SIZE                      0x3000
#endif

/* ITS Partition Configs */

/* Create flash FS if it doesn't exist for Internal Trusted Storage partition */
#ifndef ITS_CREATE_FLASH_LAYOUT
#define ITS_CREATE_FLASH_LAYOUT                1
#endif

/* Enable emulated RAM FS for platforms that don't have flash for Internal Trusted Storage partition */
#ifndef ITS_RAM_FS
#define ITS_RAM_FS                             0
#endif

/* Validate filesystem metadata every time it is read from flash */
#ifndef ITS_VALIDATE_METADATA_FROM_FLASH
#define ITS_VALIDATE_METADATA_FROM_FLASH       1
#endif

/* The maximum asset size to be stored in the Internal Trusted Storage */
#ifndef ITS_MAX_ASSET_SIZE
#define ITS_MAX_ASSET_SIZE                     512
#endif

/*
 * Size of the ITS internal data transfer buffer
 * (Default to the max asset size so that all requests can be handled in one iteration.)
 */
#ifndef ITS_BUF_SIZE
#define ITS_BUF_SIZE                           ITS_MAX_ASSET_SIZE
#endif

/* The maximum number of assets to be stored in the Internal Trusted Storage */
#ifndef ITS_NUM_ASSETS
#define ITS_NUM_ASSETS                         10
#endif

/* The stack size of the Internal Trusted Storage Secure Partition */
#ifndef ITS_STACK_SIZE
#define ITS_STACK_SIZE                         0x720
#endif

/* SPM Partition Configs */

#ifdef CONFIG_TFM_CONNECTION_POOL_ENABLE
/* The maximal number of secure services that are connected or requested at the same time */
#ifndef CONFIG_TFM_CONN_HANDLE_MAX_NUM
#define CONFIG_TFM_CONN_HANDLE_MAX_NUM         8
#endif
#endif

/* Set the doorbell APIs */
#ifndef CONFIG_TFM_DOORBELL_API
#define CONFIG_TFM_DOORBELL_API                0
#endif

#endif /* __CONFIG_PROVISION_H__ */
