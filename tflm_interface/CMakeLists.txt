#-------------------------------------------------------------------------------
# Copyright (c) 2024, TZTZEN Organization
# SPDX-License-Identifier: Apache-2.0
#-------------------------------------------------------------------------------

# TFLM Interface Library - provides common API definitions
add_library(tflm_interface INTERFACE)

target_include_directories(tflm_interface 
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# TFLM NS Client Library - for non-secure side
add_library(tflm_ns_client INTERFACE)

target_include_directories(tflm_ns_client
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/ns
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tflm_ns_client
    INTERFACE
        tflm_interface
)

# TFLM SPE Service Library - for secure side
add_library(tflm_spe_service INTERFACE)

target_include_directories(tflm_spe_service
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/spe
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tflm_spe_service
    INTERFACE
        tflm_interface
)
