#-------------------------------------------------------------------------------
# Copyright (c) 2024, TZTZEN Organization
# SPDX-License-Identifier: Apache-2.0
#-------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.21)

if (NOT DEFINED CONFIG_SPE_PATH OR NOT EXISTS ${CONFIG_SPE_PATH})
    message(FATAL_ERROR "CONFIG_SPE_PATH = ${CONFIG_SPE_PATH} is not defined or incorrect. Please provide full path to TF-M build artifacts using -DCONFIG_SPE_PATH=")
endif()

# TF-M build system integration
list(APPEND CMAKE_MODULE_PATH ${CONFIG_SPE_PATH}/cmake)

# Platform specific MCPU and architecture flags for NS side
include(${CONFIG_SPE_PATH}/platform/cpuarch.cmake)

# Include common configs exported from TF-M
include(${CONFIG_SPE_PATH}/cmake/spe_config.cmake)

# Select toolchain file
if (NOT DEFINED TFM_TOOLCHAIN_FILE)
    if (NOT DEFINED TFM_TOOLCHAIN)
        set(TFM_TOOLCHAIN "GNUARM")
        message(WARNING "TFM_TOOLCHAIN is set to ${TFM_TOOLCHAIN}")
    endif()
    set(TFM_TOOLCHAIN_FILE ${CONFIG_SPE_PATH}/cmake/toolchain_ns_${TFM_TOOLCHAIN}.cmake)
endif()

include(${TFM_TOOLCHAIN_FILE})

# Project declaration - C language only
project("TFLM-NS-Application" LANGUAGES CXX C ASM)

# Create main executable
add_executable(tfm_ns)

# Add TF-M exported interfaces
add_subdirectory(${CONFIG_SPE_PATH} ${CMAKE_BINARY_DIR}/spe)

# Source files
target_sources(tfm_ns
    PRIVATE
        main_ns.c
        ${CONFIG_SPE_PATH}/interface/src/os_wrapper/tfm_ns_interface_bare_metal.c
        $<$<BOOL:${CONFIG_GNU_SYSCALL_STUB_ENABLED}>:syscalls_stub.c>
)

# Include directories
target_include_directories(tfm_ns
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../../interface
)

# Link TF-M libraries
target_link_libraries(tfm_ns
    PRIVATE
        tfm_api_ns
)

if (CONFIG_TFM_USE_TRUSTZONE)
    target_link_libraries(tfm_ns
        PRIVATE
            tfm_api_ns_tz
    )
endif()

# Set target properties
set_target_properties(tfm_ns PROPERTIES
    SUFFIX ".axf"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LINKER_LANGUAGE C
)

# Add scatter file for linking
target_add_scatter_file(tfm_ns ${CONFIG_SPE_PATH}/platform/linker_scripts)

# Link options
target_link_options(tfm_ns
    PRIVATE
        $<$<C_COMPILER_ID:GNU>:-Wl,-Map=${CMAKE_BINARY_DIR}/bin/tfm_ns.map>
        $<$<C_COMPILER_ID:ARMClang>:--map>
        $<$<C_COMPILER_ID:IAR>:--map\;${CMAKE_BINARY_DIR}/bin/tfm_ns.map>
)

# Create binary output
add_convert_to_bin_target(tfm_ns)

# Debug information
message(STATUS "=== TFLM NS Application Configuration ===")
message(STATUS "CONFIG_SPE_PATH: ${CONFIG_SPE_PATH}")
message(STATUS "TFM_TOOLCHAIN_FILE: ${TFM_TOOLCHAIN_FILE}")
message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")
message(STATUS "==========================================")
