#-------------------------------------------------------------------------------
# Copyright (c) 2025, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#-------------------------------------------------------------------------------

if (NOT TFM_PARTITION_TFLM_INFERENCE)
    return()
endif()

cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0079 NEW)

add_library(tfm_app_rot_partition_tflm_inference STATIC)

target_sources(tfm_app_rot_partition_tflm_inference
    PRIVATE
        tflm_inference.cpp
)

# The generated sources
target_sources(tfm_app_rot_partition_tflm_inference
    PRIVATE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/tflm_inference/auto_generated/intermedia_tflm_inference_manifest.c
)

target_sources(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/tflm_inference/auto_generated/load_info_tflm_inference_manifest.c
)

# Set include directories
target_include_directories(tfm_app_rot_partition_tflm_inference
    PRIVATE
        .
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/tflm_inference
        ${CMAKE_CURRENT_SOURCE_DIR}/../../interface/include
)

target_include_directories(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/tflm_inference
)

# Add TFLM library as subdirectory to ensure proper integration
set(TFLM_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/tflm_lib")

# Check if TFLM library was built
if(EXISTS "${TFLM_LIB_PATH}/build/libtflm_lib.a")
    message(STATUS "Found TFLM library: ${TFLM_LIB_PATH}/build/libtflm_lib.a")
    
    # Create imported library target
    add_library(tflm_lib_imported STATIC IMPORTED)
    set_target_properties(tflm_lib_imported PROPERTIES
        IMPORTED_LOCATION "${TFLM_LIB_PATH}/build/libtflm_lib.a"
        INTERFACE_INCLUDE_DIRECTORIES "${TFLM_LIB_PATH}/include;${TFLM_LIB_PATH};${TFLM_LIB_PATH}/tensorflow;${TFLM_LIB_PATH}/third_party;${TFLM_LIB_PATH}/third_party/flatbuffers/include;${TFLM_LIB_PATH}/third_party/gemmlowp;${TFLM_LIB_PATH}/signal"
        INTERFACE_COMPILE_DEFINITIONS "TF_LITE_STATIC_MEMORY;TF_LITE_MCU;TF_LITE_USE_GLOBAL_CMATH_FUNCTIONS;TF_LITE_DISABLE_X86_NEON;TFLITE_EMULATE_FLOAT"
    )
    
    target_link_libraries(tfm_app_rot_partition_tflm_inference
        PRIVATE
            tflm_lib_imported
    )
else()
    message(WARNING "TFLM library not found. Please build it first with:")
    message(WARNING "  cd ${TFLM_LIB_PATH}")
    message(WARNING "  mkdir -p build && cd build")
    message(WARNING "  cmake -DARM_TOOLCHAIN=ON .. && make")
endif()

target_link_libraries(tfm_app_rot_partition_tflm_inference
    PRIVATE
        platform_s
        tfm_sprt
)

target_link_libraries(tfm_partitions
    INTERFACE
        tfm_app_rot_partition_tflm_inference
)

target_compile_definitions(tfm_app_rot_partition_tflm_inference
    PRIVATE
        TFM_PARTITION_TFLM_INFERENCE
        TF_LITE_STATIC_MEMORY
        TF_LITE_MCU
        TFM_LOG_LEVEL=TFM_LOG_LEVEL_INFO
)

# Enable C++ compilation
enable_language(CXX)

# C++ standard for partition (match TFLM library)
set_target_properties(tfm_app_rot_partition_tflm_inference PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Set compile options for C++
target_compile_options(tfm_app_rot_partition_tflm_inference
    PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers>
)