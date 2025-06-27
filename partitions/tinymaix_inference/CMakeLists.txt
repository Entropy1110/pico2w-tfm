#-------------------------------------------------------------------------------
# Copyright (c) 2025, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#-------------------------------------------------------------------------------

if (NOT TFM_PARTITION_TINYMAIX_INFERENCE)
    return()
endif()

cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0079 NEW)

add_library(tfm_app_rot_partition_tinymaix_inference STATIC)

target_sources(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        tinymaix_inference.c
        # TinyMaix core source files (internal copy)
        tinymaix/src/tm_model.c
        tinymaix/src/tm_layers.c
        # PSA-friendly encrypted model data (CBC mode)
        ../../models/encrypted_mnist_model_psa.c
)

# The generated sources
target_sources(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/tinymaix_inference/auto_generated/intermedia_tinymaix_inference_manifest.c
)

target_sources(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/tinymaix_inference/auto_generated/load_info_tinymaix_inference_manifest.c
)

# Set include directories
target_include_directories(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        .
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/tinymaix_inference
        ${CMAKE_SOURCE_DIR}/../../interface/include
        tinymaix/include
        ../../models
)

target_include_directories(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/tinymaix_inference
)

target_link_libraries(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        platform_s
        tfm_sprt
        tfm_log_unpriv
        psa_crypto_config
)

target_link_libraries(tfm_partitions
    INTERFACE
        tfm_app_rot_partition_tinymaix_inference
)

target_compile_definitions(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        TFM_PARTITION_TINYMAIX_INFERENCE
        $<$<BOOL:${DEV_MODE}>:DEV_MODE>
)