#-------------------------------------------------------------------------------
# Copyright (c) 2025, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#-------------------------------------------------------------------------------

if (NOT TFM_PARTITION_CMSIS_NN_INFERENCE)
    return()
endif()

cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0079 NEW)

add_library(tfm_app_rot_partition_cmsis_nn_inference STATIC)

target_sources(tfm_app_rot_partition_cmsis_nn_inference
    PRIVATE
        cmsis_nn_inference.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/cmsis_nn_lib/src/cmsis_nn_wrapper.c
)

# The generated sources
target_sources(tfm_app_rot_partition_cmsis_nn_inference
    PRIVATE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/cmsis_nn_inference/auto_generated/intermedia_cmsis_nn_inference_manifest.c
)

target_sources(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/cmsis_nn_inference/auto_generated/load_info_cmsis_nn_inference_manifest.c
)

# Set include directories
target_include_directories(tfm_app_rot_partition_cmsis_nn_inference
    PRIVATE
        .
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/cmsis_nn_inference
        ${CMAKE_CURRENT_SOURCE_DIR}/../../interface/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/cmsis_nn_lib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/cmsis_nn/Include
)

target_include_directories(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/secure_fw/partitions/cmsis_nn_inference
)


target_link_libraries(tfm_app_rot_partition_cmsis_nn_inference
    PRIVATE
        platform_s
        tfm_sprt
)

target_link_libraries(tfm_partitions
    INTERFACE
        tfm_app_rot_partition_cmsis_nn_inference
)

target_compile_definitions(tfm_app_rot_partition_cmsis_nn_inference
    PRIVATE
        TFM_PARTITION_CMSIS_NN_INFERENCE
        TFM_LOG_LEVEL=TFM_LOG_LEVEL_INFO
        # ARM Cortex-M definitions
        ARM_MATH_CM33
        ARM_MATH_DSP
        ARM_MATH_LOOPUNROLL
        __CORTEX_M
        PROJECT_GENERATION
)

# Pure C implementation - no C++ needed
# Using standard C99 for maximum compatibility