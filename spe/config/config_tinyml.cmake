#-------------------------------------------------------------------------------
# Copyright (c) 2023-2025, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

set(TFM_PROFILE                         ""          CACHE STRING    "Profile to use")
set(TFM_PLATFORM                        "rpi/rp2350" CACHE STRING   "Platform to build TF-M for.")

############################ Platform ##########################################

set(PLATFORM_DEFAULT_ATTEST_HAL        ON          CACHE BOOL      "Use default attest hal implementation.")
set(PLATFORM_DEFAULT_CRYPTO_KEYS       ON          CACHE BOOL      "Use default crypto keys implementation.")
set(PLATFORM_DEFAULT_NV_COUNTERS       ON          CACHE BOOL      "Use default nv counter implementation.")
set(PLATFORM_DEFAULT_PROVISIONING      ON          CACHE BOOL      "Use default provisioning implementation.")
set(PLATFORM_DEFAULT_ROTPK             ON          CACHE BOOL      "Use default root of trust public key.")
set(PLATFORM_DEFAULT_IAK               ON          CACHE BOOL      "Use default initial attestation_key.")
set(PLATFORM_DEFAULT_UART_STDOUT       ON          CACHE BOOL      "Use default uart stdout implementation.")
set(PLATFORM_DEFAULT_NV_SEED           ON          CACHE BOOL      "Use default NV seed implementation.")
set(PLATFORM_DEFAULT_OTP               ON          CACHE BOOL      "Use default OTP implementation.")
set(PLATFORM_DEFAULT_SYSTEM_RESET_HALT ON          CACHE BOOL      "Use default system reset/halt implementation.")

############################ Partitions ########################################

set(TFM_PARTITION_CRYPTO                ON          CACHE BOOL      "Enable Crypto partition for PSA encryption")
set(TFM_PARTITION_INITIAL_ATTESTATION   OFF         CACHE BOOL      "Disable Initial Attestation partition")
set(TFM_PARTITION_PROTECTED_STORAGE     OFF         CACHE BOOL      "Disable Protected Storage partition")
set(TFM_PARTITION_INTERNAL_TRUSTED_STORAGE ON       CACHE BOOL      "Enable Internal Trusted Storage partition")
set(TFM_PARTITION_PLATFORM              ON          CACHE BOOL      "Enable Platform partition")
set(TFM_PARTITION_ECHO_SERVICE           ON          CACHE BOOL      "Enable Echo Service partition")
set(TFM_PARTITION_TINYMAIX_INFERENCE    ON          CACHE BOOL      "Enable TinyMaix Inference partition")

# Development mode option for debug features
set(DEV_MODE                            OFF         CACHE BOOL      "Enable development mode with debug features")

# Crypto modules will be automatically enabled based on TFM_CRYPTO dependency in manifest
# No need to manually configure them

# Echo service is now out-of-tree
set(TFM_EXTRA_MANIFEST_LIST_FILES       "${CMAKE_SOURCE_DIR}/../partitions/manifest_list.yaml" CACHE STRING "Out-of-tree partition manifest list")
set(TFM_EXTRA_PARTITION_PATHS           "${CMAKE_SOURCE_DIR}/../partitions" CACHE STRING "Out-of-tree partition paths")

################################## Tests #######################################

# All test-related configurations are disabled for TINYML
set(TFM_PARTITION_AUDIT_LOG             OFF         CACHE BOOL      "Enable Audit Log partition")
set(TFM_PARTITION_FIRMWARE_UPDATE       OFF         CACHE BOOL      "Enable firmware update partition")
set(TEST_S                              OFF         CACHE BOOL      "Whether to build S regression tests")
set(TEST_NS                             OFF         CACHE BOOL      "Whether to build NS regression tests")
set(TEST_PSA_API                        OFF         CACHE BOOL      "Whether to build PSA API tests")
set(TFM_NS_REG_TEST                     OFF         CACHE BOOL      "Enable NS regression test")
set(TFM_S_REG_TEST                      OFF         CACHE BOOL      "Enable S regression test")

################################## Dependencies ############################

# Use TF-M's default client config and our service config
set(TFM_MBEDCRYPTO_CONFIG_PATH ${CMAKE_CURRENT_LIST_DIR}/mbedtls_config.h CACHE PATH "Service side mbedtls config")
# set(TFM_MBEDCRYPTO_CONFIG_CLIENT_PATH ${CONFIG_TFM_SOURCE_PATH}/lib/ext/mbedcrypto/mbedcrypto_config/tfm_mbedcrypto_config_client.h CACHE PATH "Client side mbedtls config") 
# set(TFM_MBEDCRYPTO_PLATFORM_EXTRA_CONFIG_PATH ${CMAKE_CURRENT_LIST_DIR}/mbedtls_extra_config.h CACHE PATH "Extra config for MbedTLS")
set(TFM_MBEDCRYPTO_PSA_CRYPTO_CONFIG_PATH ${CMAKE_CURRENT_LIST_DIR}/config_tinyml.h CACHE PATH "PSA crypto configuration file")

################################## Compiler options #########################

# BL2 (bootloader) is not necessary for TINYML apps
set(BL2                                 OFF         CACHE BOOL      "Whether to build BL2")
set(MCUBOOT_IMAGE_NUMBER                1           CACHE STRING    "Number of images supported by MCUBoot")
set(TFM_ISOLATION_LEVEL                 2           CACHE STRING    "Isolation level")
set(CONFIG_TFM_SPM_BACKEND_IPC          ON          CACHE BOOL      "The IPC model for the SPM backend")

# Enable logging functionality
set(TFM_LOG_LEVEL                       TFM_LOG_LEVEL_INFO CACHE STRING "Set default log level as INFO level")
set(TFM_SPM_LOG_LEVEL                   TFM_SPM_LOG_LEVEL_INFO CACHE STRING "Set SPM log level as INFO level")
set(TFM_EXCEPTION_INFO_DUMP             ON          CACHE BOOL      "On fatal errors in the secure firmware, capture info about the exception")

# Set system tick and clock
set(TFM_NS_CLIENT_IDENTIFICATION        ON          CACHE BOOL      "Enable NS client identification")
set(CONFIG_TFM_HALT_ON_CORE_PANIC       ON          CACHE BOOL      "On fatal errors in the secure firmware, halt execution")
