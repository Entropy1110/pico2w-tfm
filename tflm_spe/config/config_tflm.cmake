#-------------------------------------------------------------------------------
# Copyright (c) 2023-2024, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

# Provision application specific configuration
# Based on PSA Crypto test configuration from tf-m-tests

# Enable partitions required for Crypto operations (similar to PSA CRYPTO test)
set(TFM_PARTITION_CRYPTO                   ON       CACHE BOOL      "Enable Crypto partition")
set(TFM_PARTITION_INTERNAL_TRUSTED_STORAGE ON       CACHE BOOL      "Enable Internal Trusted Storage partition")
set(TFM_PARTITION_PLATFORM                 ON       CACHE BOOL      "Enable Platform partition")

# Enable TFLM partition
set(TFM_PARTITION_TFLM_SECURE_SERVICE_SP   ON       CACHE BOOL      "Enable TFLM Secure Service partition")

# Disable storage partitions that aren't needed for basic crypto
set(TFM_PARTITION_PROTECTED_STORAGE        OFF      CACHE BOOL      "Disable Protected Storage partition")

# Disable other optional partitions
set(TFM_PARTITION_INITIAL_ATTESTATION      OFF      CACHE BOOL      "Disable Initial Attestation partition")
set(TFM_PARTITION_FIRMWARE_UPDATE          OFF      CACHE BOOL      "Disable Firmware Update partition")

# Use SFN backend for simpler configuration
set(CONFIG_TFM_SPM_BACKEND                 "SFN"    CACHE STRING    "The SPM backend [IPC, SFN]")

# Force logging configuration
set(TFM_SPM_LOG_LEVEL                      "LOG_LEVEL_INFO"    CACHE STRING    "Set SPM log level" FORCE)
set(TFM_PARTITION_LOG_LEVEL                "LOG_LEVEL_INFO"    CACHE STRING    "Set partition log level" FORCE)

# Disable crypto modules that we don't need for basic random generation
set(CRYPTO_CIPHER_MODULE_ENABLED            OFF      CACHE BOOL      "Disable cipher module")
set(CRYPTO_AEAD_MODULE_ENABLED               OFF      CACHE BOOL      "Disable AEAD module")
set(CRYPTO_MAC_MODULE_ENABLED                OFF      CACHE BOOL      "Disable MAC module")
set(CRYPTO_ASYM_SIGN_MODULE_ENABLED          OFF      CACHE BOOL      "Disable asymmetric signature module")
set(CRYPTO_ASYM_ENCRYPT_MODULE_ENABLED       OFF      CACHE BOOL      "Disable asymmetric encryption module")
set(CRYPTO_KEY_DERIVATION_MODULE_ENABLED     OFF      CACHE BOOL      "Disable key derivation module")

# Enable only what we need for random generation
set(CRYPTO_RNG_MODULE_ENABLED                ON       CACHE BOOL      "Enable RNG module")
set(CRYPTO_HASH_MODULE_ENABLED               ON       CACHE BOOL      "Enable hash module")

# Set the project configuration header file
set(PROJECT_CONFIG_HEADER_FILE
    ${CMAKE_CURRENT_LIST_DIR}/config_tflm.h
    CACHE FILEPATH "The tflm config header file")
