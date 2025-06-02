# TensorFlow Lite Micro CMake configuration for standalone static library
cmake_minimum_required(VERSION 3.15)

# Configure for ARM Cortex-M33 cross-compilation if specified
# NOTE: This must be done BEFORE project() command
if(ARM_TOOLCHAIN)
    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_SYSTEM_PROCESSOR arm)
    set(ARM_TOOLCHAIN_PREFIX "arm-none-eabi-")
    set(CMAKE_C_COMPILER ${ARM_TOOLCHAIN_PREFIX}gcc)
    set(CMAKE_CXX_COMPILER ${ARM_TOOLCHAIN_PREFIX}g++)
    set(CMAKE_ASM_COMPILER ${ARM_TOOLCHAIN_PREFIX}gcc)
    set(CMAKE_LINKER ${ARM_TOOLCHAIN_PREFIX}ld)
    set(CMAKE_AR ${ARM_TOOLCHAIN_PREFIX}ar)
    set(CMAKE_RANLIB ${ARM_TOOLCHAIN_PREFIX}ranlib)
    set(CMAKE_STRIP ${ARM_TOOLCHAIN_PREFIX}strip)
    set(CMAKE_OBJCOPY ${ARM_TOOLCHAIN_PREFIX}objcopy)
    set(CMAKE_SIZE ${ARM_TOOLCHAIN_PREFIX}size)
    # Cortex-M33 specific flags
    set(ARM_CPU_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARM_CPU_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARM_CPU_FLAGS}")
    # Skip compiler tests for cross-compilation (we're building static library only)
    set(CMAKE_C_COMPILER_WORKS 1)
    set(CMAKE_CXX_COMPILER_WORKS 1)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
endif()

project(tflm_minimal CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Create TFLM library
add_library(tflm_minimal STATIC)

# TFLM core source files
file(GLOB_RECURSE TFLM_CORE_SOURCES
    "tensorflow/lite/c/*.cpp"
    "tensorflow/lite/c/*.c"
    "tensorflow/lite/core/*.cpp"
    "tensorflow/lite/core/*.c"
    "tensorflow/lite/micro/*.cpp"
    "tensorflow/lite/micro/*.c"
    "tensorflow/lite/kernels/*.cpp"
    "tensorflow/lite/kernels/*.c"
    "tensorflow/lite/schema/*.cpp"
    "tensorflow/lite/schema/*.c"
)

# TFLM kernel sources
file(GLOB_RECURSE TFLM_KERNEL_SOURCES
    "tensorflow/lite/micro/kernels/*.cpp"
    "tensorflow/lite/micro/kernels/*.c"
)

# Third party sources
file(GLOB_RECURSE THIRD_PARTY_SOURCES
    "third_party/*.cpp"
    "third_party/*.c"
)

# Signal processing sources
file(GLOB_RECURSE SIGNAL_SOURCES
    "signal/*.cpp"
    "signal/*.c"
)

# C interface wrapper
set(TFLM_C_INTERFACE_SOURCES
    "src/tflm_c_interface.cpp"
)

# Add all sources to the library
target_sources(tflm_minimal PRIVATE
    ${TFLM_CORE_SOURCES}
    ${TFLM_KERNEL_SOURCES}
    ${THIRD_PARTY_SOURCES}
    ${SIGNAL_SOURCES}
    ${TFLM_C_INTERFACE_SOURCES}
)

# Include directories
target_include_directories(tflm_minimal PUBLIC
    .
    include
    tensorflow
    third_party
    third_party/gemmlowp
    third_party/flatbuffers/include
    third_party/kissfft
    third_party/ruy
    signal
)

# Compiler definitions for TFLM
target_compile_definitions(tflm_minimal PRIVATE
    TF_LITE_DISABLE_X86_NEON
    TF_LITE_STATIC_MEMORY
    TF_LITE_MCU
    TF_LITE_USE_GLOBAL_CMATH_FUNCTIONS
    TF_LITE_USE_GLOBAL_MAX
    TF_LITE_USE_GLOBAL_MIN
    TFLITE_EMULATE_FLOAT
    # ARM Cortex-M definitions
    ARM_MATH_CM4
    __CORTEX_M
    PROJECT_GENERATION
)

# Compiler definitions for interface users
target_compile_definitions(tflm_minimal INTERFACE
    TF_LITE_STATIC_MEMORY
    TF_LITE_MCU
)

# Compiler options for different architectures
target_compile_options(tflm_minimal PRIVATE
    -fno-rtti
    -fno-exceptions
    -fno-threadsafe-statics
    -fno-unwind-tables
    -ffunction-sections
    -fdata-sections
    -fmessage-length=0
)

# Additional flags for ARM Cortex-M when cross-compiling
if(DEFINED ARM_TOOLCHAIN)
    target_compile_options(tflm_minimal PRIVATE
        -Os  # Optimize for size
        -fno-common
        -fstack-usage
        -Wall
        -Wextra
        -Wno-unused-parameter
    )
endif()

# Set target properties
set_target_properties(tflm_minimal PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE OFF
)

# Export include directories for parent projects
target_include_directories(tflm_minimal INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tensorflow>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/signal>
)

# Optional: create a simple test executable when building standalone
option(BUILD_TFLM_TEST "Build TFLM test executable" OFF)
if(BUILD_TFLM_TEST)
    add_executable(tflm_test test/test_main.c)
    target_link_libraries(tflm_test tflm_minimal)
endif()
