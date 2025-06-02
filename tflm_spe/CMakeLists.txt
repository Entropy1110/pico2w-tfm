#-------------------------------------------------------------------------------
# Copyright (c) 2023-2025, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.21)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/utils)
include(utils)
include(tflm_flag_parse)

collect_build_cmd_args(TFM_CMDLINE_CONFIGS)

project("TF-M SPE for tflm" NONE)

if (NOT DEFINED CONFIG_TFM_SOURCE_PATH OR NOT EXISTS ${CONFIG_TFM_SOURCE_PATH})
    message(FATAL_ERROR "CONFIG_TFM_SOURCE_PATH = ${CONFIG_TFM_SOURCE_PATH} is not defined or incorrect. Please provide full path to TF-M sources.")
endif()

# Parse tflm flags (ensures all test flags are disabled)
parse_tflm_flag(TFM_CMDLINE_CONFIGS)

# tflm application does not need regression tests - force all test flags to OFF
set(TFM_NS_REG_TEST OFF CACHE BOOL "Non-secure regression tests are disabled for tflm" FORCE)
set(TFM_S_REG_TEST OFF CACHE BOOL "Secure regression tests are disabled for tflm" FORCE)
set(TEST_BL2 OFF CACHE BOOL "BL2 tests are disabled for tflm" FORCE)
set(TEST_BL1_1 OFF CACHE BOOL "BL1_1 tests are disabled for tflm" FORCE)
set(TEST_BL1_2 OFF CACHE BOOL "BL1_2 tests are disabled for tflm" FORCE)

# tfm_s_tflm is IMPORTED to inform CMake that it has no source files.
add_executable(tfm_s_tflm IMPORTED)

# Use tflm-specific configuration file
set(CONFIG_TFLM_CONFIG_FILE ${CMAKE_CURRENT_LIST_DIR}/config/config_tflm.cmake)

# Add TFLM partition to the build
set(TFM_EXTRA_PARTITION_PATHS ${CMAKE_CURRENT_LIST_DIR}/partitions)
set(TFM_EXTRA_MANIFEST_LIST_FILES ${CMAKE_CURRENT_LIST_DIR}/partitions/tflm_manifest_list.yaml)

include(ExternalProject)

ExternalProject_Add(TF-M
  SOURCE_DIR        ${CONFIG_TFM_SOURCE_PATH}
  BINARY_DIR        build-spe
  INSTALL_DIR       api_ns
  CMAKE_ARGS        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
  CMAKE_ARGS        -DPROJECT_CONFIG_HEADER_FILE=${CMAKE_CURRENT_LIST_DIR}/config/config_tflm.h
  CMAKE_ARGS        -DTFM_EXTRA_PARTITION_PATHS=${TFM_EXTRA_PARTITION_PATHS}
  CMAKE_ARGS        -DTFM_EXTRA_MANIFEST_LIST_FILES=${TFM_EXTRA_MANIFEST_LIST_FILES}
  CMAKE_CACHE_DEFAULT_ARGS ${TFM_CMDLINE_CONFIGS}
  PREFIX             "temp"
  BUILD_ALWAYS       ON
)

add_dependencies(tfm_s_tflm TF-M)

# Skip "up-to-date" prints to avoid flooding the build output. Just print "installing"
set(CMAKE_INSTALL_MESSAGE LAZY)

install(DIRECTORY ${CMAKE_BINARY_DIR}/build-spe/bin DESTINATION ${CMAKE_BINARY_DIR})

install(DIRECTORY   ${CONFIG_TFM_SOURCE_PATH}/secure_fw/partitions/initial_attestation
        DESTINATION ${CMAKE_BINARY_DIR}/api_ns
        FILES_MATCHING PATTERN "*.h")

install(FILES       ${CONFIG_TFM_SOURCE_PATH}/secure_fw/spm/include/boot/tfm_boot_status.h
        DESTINATION ${CMAKE_BINARY_DIR}/api_ns/initial_attestation)
