#-------------------------------------------------------------------------------
# Copyright (c) 2024, Your Organization
# SPDX-License-Identifier: Apache-2.0
#-------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.21)

project(TFLM_NS_APPS 
    LANGUAGES C CXX ASM
    VERSION 1.0.0
)

# Add client application
add_subdirectory(client_ns_app)
