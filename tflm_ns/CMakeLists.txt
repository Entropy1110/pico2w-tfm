#-------------------------------------------------------------------------------
# Copyright (c) 2023-2024, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.21)

if (NOT DEFINED CONFIG_SPE_PATH OR NOT EXISTS ${CONFIG_SPE_PATH})
    message(FATAL_ERROR "CONFIG_SPE_PATH = ${CONFIG_SPE_PATH} is not defined or incorrect. Please provide full path to TF-M build artifacts using -DCONFIG_SPE_PATH=")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../cmake)
list(APPEND CMAKE_MODULE_PATH ${CONFIG_SPE_PATH}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/utils)
include(remote_library)
include(toolchain_selection)
include(utils)

# A platform specific MCPU and architecture flags for NS side
include(${CONFIG_SPE_PATH}/platform/cpuarch.cmake)
# Include common configs exported from TF-M
include(${CONFIG_SPE_PATH}/cmake/spe_config.cmake)
# Platform abilities for example IRQ test support status
include(${CONFIG_SPE_PATH}/platform/config.cmake OPTIONAL)

include(${TFM_TOOLCHAIN_FILE})
project(tfm_ns LANGUAGES C ASM)

# Provision functionality - no test configuration needed
# Log functionality is preserved for provision applications
# Include coprocessor check to make sure NS toolchain is compatible
include(${CONFIG_SPE_PATH}/config/cp_check.cmake)

add_executable(tfm_ns)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/ext ${CMAKE_BINARY_DIR}/lib/ext)
add_subdirectory(../app_broker ${CMAKE_BINARY_DIR}/app_broker)
add_subdirectory(../tflm_interface ${CMAKE_BINARY_DIR}/tflm_interface)

############################# TFM NS provision app ##################################

target_sources(tfm_ns
    PRIVATE
        app/tflm_main.c
        $<$<BOOL:${CONFIG_GNU_SYSCALL_STUB_ENABLED}>:../app_broker/syscalls_stub.c>
)

target_link_libraries(tfm_ns
    PRIVATE
        os_wrapper
        tfm_tflm_broker
        tflm_ns_client
)

target_include_directories(tfm_ns
    PRIVATE
        # Remove this line as we now use tflm_ns_client library
)

target_compile_definitions(tfm_ns
    PUBLIC
        # Force TFM_NS_LOG to be always enabled for provision apps
        TFM_NS_LOG
)

set_target_properties(tfm_ns PROPERTIES
    SUFFIX ".axf"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_add_scatter_file(tfm_ns ${CONFIG_SPE_PATH}/platform/linker_scripts)

target_link_options(tfm_ns
    PRIVATE
        $<$<C_COMPILER_ID:GNU>:-Wl,-Map=${CMAKE_BINARY_DIR}/bin/tfm_ns.map>
        $<$<C_COMPILER_ID:ARMClang>:--map>
        $<$<C_COMPILER_ID:IAR>:--map\;${CMAKE_BINARY_DIR}/bin/tfm_ns.map>
)

add_convert_to_bin_target(tfm_ns)
