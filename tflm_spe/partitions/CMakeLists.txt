#-------------------------------------------------------------------------------
# Copyright (c) 2024, Your Organization
# SPDX-License-Identifier: Apache-2.0
#-------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.21)

# Import the TFLM minimal static library
set(TFLM_MINIMAL_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../tflm_minimal)

# Check if TFLM library exists, build if needed
set(TFLM_LIBRARY_PATH ${TFLM_MINIMAL_ROOT}/build_arm/libtflm_minimal.a)
set(TFLM_AVAILABLE FALSE)

if(EXISTS ${TFLM_LIBRARY_PATH})
    set(TFLM_AVAILABLE TRUE)
    message(STATUS "TFLM library found at ${TFLM_LIBRARY_PATH}")
else()
    message(WARNING "TFLM library not found. Building without TFLM support for echo test.")
    message(STATUS "To enable full TFLM functionality, build the library manually:")
    message(STATUS "  cd ${TFLM_MINIMAL_ROOT}")
    message(STATUS "  cmake -S . -B build_arm -DARM_TOOLCHAIN=ON")
    message(STATUS "  cmake --build build_arm")
endif()

# Import the pre-built TFLM library if available
if(TFLM_AVAILABLE)
    add_library(tflm_minimal STATIC IMPORTED)
    set_target_properties(tflm_minimal PROPERTIES
        IMPORTED_LOCATION ${TFLM_LIBRARY_PATH}
        INTERFACE_INCLUDE_DIRECTORIES "${TFLM_MINIMAL_ROOT}/include;${TFLM_MINIMAL_ROOT}"
    )
endif()

add_library(tflm_secure_service_sp STATIC)

target_sources(tflm_secure_service_sp
    PRIVATE
        tflm_secure_service_sp.c
        tflm_crypto_ops.c
        tflm_inference_engine.cpp
        encrypted_audio_model.c
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
        # TFLM minimal library includes
        ${TFLM_MINIMAL_ROOT}/include
        ${TFLM_MINIMAL_ROOT}
)

# Conditionally link TFLM library if available
if(TFLM_AVAILABLE)
    target_link_libraries(tflm_secure_service_sp
        PRIVATE
            tfm_sprt
            platform_s
            tflm_minimal  # Link our TFLM static library
    )
    target_compile_definitions(tflm_secure_service_sp
        PRIVATE
            TFLM_LIBRARY_AVAILABLE=1
    )
    message(STATUS "Linking with TFLM library support")
else()
    target_link_libraries(tflm_secure_service_sp
        PRIVATE
            tfm_sprt
            platform_s
    )
    target_compile_definitions(tflm_secure_service_sp
        PRIVATE
            TFLM_LIBRARY_AVAILABLE=0
    )
    message(STATUS "Building without TFLM library - echo test only")
endif()

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
