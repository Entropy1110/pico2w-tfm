cmake_minimum_required(VERSION 3.15)

project(cmsis_nn_lib)

# Set compilation flags for CMSIS-NN
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)

# Create a simple wrapper library for CMSIS-NN API compatibility
# This provides the same API as TFLM but implements a basic neural network
add_library(cmsis_nn_lib STATIC
    src/cmsis_nn_wrapper.c
    include/cmsis_nn_wrapper.h
)

# Include directories
target_include_directories(cmsis_nn_lib PUBLIC
    include
)

# Compiler specific options for optimization
target_compile_options(cmsis_nn_lib PRIVATE
    -O3
)

# Set optimization definitions
target_compile_definitions(cmsis_nn_lib PRIVATE
    ARM_MATH_CM33
)