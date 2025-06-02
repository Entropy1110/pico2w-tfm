#-------------------------------------------------------------------------------
# Copyright (c) 2024, Your Organization
# SPDX-License-Identifier: Apache-2.0
#-------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.21)

add_library(tflm_secure_service_sp STATIC)

target_sources(tflm_secure_service_sp
    PRIVATE
        tflm_secure_service_sp.c
        tflm_crypto_ops.c
        tflm_inference_engine.c
)

target_include_directories(tflm_secure_service_sp
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
    PRIVATE
        ${CMAKE_BINARY_DIR}/generated/interface/include
        ${CMAKE_SOURCE_DIR}/secure_fw/spm/include
        ${CMAKE_SOURCE_DIR}/interface/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tflm_interface/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tflm_interface/spe
        ${CMAKE_BINARY_DIR}/lib/ext/mbedcrypto-src/include
        ${CMAKE_BINARY_DIR}/lib/ext/mbedcrypto-build/include
        ${CMAKE_BINARY_DIR}/lib/ext/mbedcrypto-build
)

target_link_libraries(tflm_secure_service_sp
    PRIVATE
        tfm_sprt
        platform_s
)

target_compile_definitions(tflm_secure_service_sp
    PRIVATE
        TFM_PARTITION_TFLM_SECURE_SERVICE_SP
        TFM_PARTITION_CRYPTO=1
)

if(TARGET tfm_psa_rot_partition_crypto)
    target_link_libraries(tflm_secure_service_sp
        PRIVATE
            tfm_psa_rot_partition_crypto
    )
endif()

if(TARGET mbedcrypto)
    add_dependencies(tflm_secure_service_sp mbedcrypto)
elseif(TARGET crypto_service_mbedcrypto)
    add_dependencies(tflm_secure_service_sp crypto_service_mbedcrypto)
endif()

############################ Partition Defs ####################################

target_link_libraries(tfm_partitions
    INTERFACE
        tflm_secure_service_sp
)

target_compile_definitions(tfm_config
    INTERFACE
        TFM_PARTITION_TFLM_SECURE_SERVICE_SP
)
