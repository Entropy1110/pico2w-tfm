#-------------------------------------------------------------------------------
# Copyright (c) 2025, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#-------------------------------------------------------------------------------

if (NOT TFM_PARTITION_ECHO_SERVICE)
    return()
endif()

cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0079 NEW)

add_library(tfm_app_rot_partition_echo_service STATIC)

target_sources(tfm_app_rot_partition_echo_service
    PRIVATE
        echo_service.c
)

# The generated sources
target_sources(tfm_app_rot_partition_echo_service
    PRIVATE
        ${CMAKE_BINARY_DIR}/generated/partitions/echo_service/auto_generated/intermedia_echo_service_manifest.c
)

target_sources(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/partitions/echo_service/auto_generated/load_info_echo_service_manifest.c
)

# Set include directories
target_include_directories(tfm_app_rot_partition_echo_service
    PRIVATE
        .
        ${CMAKE_BINARY_DIR}/generated/partitions/echo_service
        ${CMAKE_SOURCE_DIR}/../interface/include
)

target_include_directories(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/generated/partitions/echo_service
)

target_link_libraries(tfm_app_rot_partition_echo_service
    PRIVATE
        platform_s
        tfm_sprt
)

target_link_libraries(tfm_partitions
    INTERFACE
        tfm_app_rot_partition_echo_service
)

target_compile_definitions(tfm_app_rot_partition_echo_service
    PRIVATE
        TFM_PARTITION_ECHO_SERVICE
)
