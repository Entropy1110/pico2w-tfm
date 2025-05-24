#-------------------------------------------------------------------------------
# Copyright (c) 2024, Your Organization
# SPDX-License-Identifier: Apache-2.0
#-------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.21)

project(PICO2W_TFM_TFLM_NS 
    LANGUAGES C CXX ASM
    VERSION 1.0.0
)

# This is a container project for NS components
# Actual builds are managed by the build script

# Interface library for shared headers
add_library(tflm_interface INTERFACE)

target_include_directories(tflm_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/interface
)

message(STATUS "TFLM NS Application Project configured")
