# TensorFlow Lite Micro CMake configuration for standalone static library
cmake_minimum_required(VERSION 3.15)

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
# Cortex-M33 specific flags - compatible with TF-M (software floating point)
set(ARM_CPU_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=soft")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARM_CPU_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARM_CPU_FLAGS}")
# Skip compiler tests for cross-compilation (we're building static library only)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(tflm_lib CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Create TFLM library
add_library(tflm_lib STATIC)

# TFLM core sources - using actual existing files
set(TFLM_CORE_SOURCES
    # Core API and utilities
    "tensorflow/compiler/mlir/lite/core/api/error_reporter.cc"
    "tensorflow/lite/core/api/flatbuffer_conversions.cc"
    "tensorflow/lite/core/api/tensor_utils.cc"
    "tensorflow/lite/core/c/common.cc"
    "tensorflow/lite/kernels/internal/common.cc"
    "tensorflow/lite/kernels/internal/portable_tensor_utils.cc"
    "tensorflow/lite/kernels/internal/quantization_util.cc"
    "tensorflow/lite/kernels/internal/reference/comparisons.cc"
    "tensorflow/lite/kernels/internal/reference/portable_tensor_utils.cc"
    "tensorflow/lite/kernels/internal/runtime_shape.cc"
    "tensorflow/lite/kernels/internal/tensor_ctypes.cc"
    "tensorflow/lite/kernels/internal/tensor_utils.cc"
    "tensorflow/lite/kernels/kernel_util.cc"
    "tensorflow/compiler/mlir/lite/schema/schema_utils.cc"
    
    # Micro framework
    "tensorflow/lite/micro/arena_allocator/non_persistent_arena_buffer_allocator.cc"
    "tensorflow/lite/micro/arena_allocator/persistent_arena_buffer_allocator.cc"
    "tensorflow/lite/micro/arena_allocator/recording_single_arena_buffer_allocator.cc"
    "tensorflow/lite/micro/arena_allocator/single_arena_buffer_allocator.cc"
    "tensorflow/lite/micro/cortex_m_generic/debug_log.cc"
    "tensorflow/lite/micro/cortex_m_generic/micro_time.cc"
    "tensorflow/lite/micro/fake_micro_context.cc"
    "tensorflow/lite/micro/flatbuffer_utils.cc"
    "tensorflow/lite/micro/hexdump.cc"
    "tensorflow/lite/micro/memory_helpers.cc"
    "tensorflow/lite/micro/memory_planner/greedy_memory_planner.cc"
    "tensorflow/lite/micro/memory_planner/linear_memory_planner.cc"
    "tensorflow/lite/micro/memory_planner/non_persistent_buffer_planner_shim.cc"
    "tensorflow/lite/micro/micro_allocation_info.cc"
    "tensorflow/lite/micro/micro_allocator.cc"
    "tensorflow/lite/micro/micro_context.cc"
    "tensorflow/lite/micro/micro_interpreter.cc"
    "tensorflow/lite/micro/micro_interpreter_context.cc"
    "tensorflow/lite/micro/micro_interpreter_graph.cc"
    "tensorflow/lite/micro/micro_log.cc"
    "tensorflow/lite/micro/micro_op_resolver.cc"
    "tensorflow/lite/micro/micro_profiler.cc"
    "tensorflow/lite/micro/micro_resource_variable.cc"
    "tensorflow/lite/micro/micro_utils.cc"
    "tensorflow/lite/micro/mock_micro_graph.cc"
    "tensorflow/lite/micro/recording_micro_allocator.cc"
    "tensorflow/lite/micro/system_setup.cc"
    "tensorflow/lite/micro/test_helper_custom_ops.cc"
    "tensorflow/lite/micro/test_helpers.cc"
    "tensorflow/lite/micro/tflite_bridge/flatbuffer_conversions_bridge.cc"
    "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.cc"
)

# Essential TFLM kernel sources - include only commonly used kernels to reduce size
set(TFLM_KERNEL_SOURCES
    "tensorflow/lite/micro/kernels/activations.cc"
    "tensorflow/lite/micro/kernels/activations_common.cc"
    "tensorflow/lite/micro/kernels/add.cc"
    "tensorflow/lite/micro/kernels/add_common.cc"
    "tensorflow/lite/micro/kernels/conv.cc"
    "tensorflow/lite/micro/kernels/conv_common.cc"
    "tensorflow/lite/micro/kernels/depthwise_conv.cc"
    "tensorflow/lite/micro/kernels/depthwise_conv_common.cc"
    "tensorflow/lite/micro/kernels/dequantize.cc"
    "tensorflow/lite/micro/kernels/dequantize_common.cc"
    "tensorflow/lite/micro/kernels/fully_connected.cc"
    "tensorflow/lite/micro/kernels/fully_connected_common.cc"
    "tensorflow/lite/micro/kernels/hard_swish.cc"
    "tensorflow/lite/micro/kernels/hard_swish_common.cc"
    "tensorflow/lite/micro/kernels/kernel_runner.cc"
    "tensorflow/lite/micro/kernels/kernel_util.cc"
    "tensorflow/lite/micro/kernels/logistic.cc"
    "tensorflow/lite/micro/kernels/logistic_common.cc"
    "tensorflow/lite/micro/kernels/micro_tensor_utils.cc"
    "tensorflow/lite/micro/kernels/mul.cc"
    "tensorflow/lite/micro/kernels/mul_common.cc"
    "tensorflow/lite/micro/kernels/pooling.cc"
    "tensorflow/lite/micro/kernels/pooling_common.cc"
    "tensorflow/lite/micro/kernels/quantize.cc"
    "tensorflow/lite/micro/kernels/quantize_common.cc"
    "tensorflow/lite/micro/kernels/reshape.cc"
    "tensorflow/lite/micro/kernels/reshape_common.cc"
    "tensorflow/lite/micro/kernels/softmax.cc"
    "tensorflow/lite/micro/kernels/softmax_common.cc"
    "tensorflow/lite/micro/kernels/tanh.cc"
)

# Third party sources
set(THIRD_PARTY_SOURCES
    "third_party/kissfft/kiss_fft.c"
    "third_party/kissfft/tools/kiss_fftr.c"
)

# Signal processing sources
file(GLOB_RECURSE SIGNAL_SOURCES
    "signal/src/*.cc"
    "signal/micro/kernels/*.cc"
)

# C API wrapper and helper sources
set(TFLM_C_API_SOURCES
    "src/tflm_c_api.cpp"
    "src/micro_printf.cpp"
)

# Add all sources to the library
target_sources(tflm_lib PRIVATE
    ${TFLM_CORE_SOURCES}
    ${TFLM_KERNEL_SOURCES}
    ${THIRD_PARTY_SOURCES}
    ${SIGNAL_SOURCES}
    ${TFLM_C_API_SOURCES}
)

# Include directories
target_include_directories(tflm_lib PUBLIC
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
target_compile_definitions(tflm_lib PRIVATE
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
target_compile_definitions(tflm_lib INTERFACE
    TF_LITE_STATIC_MEMORY
    TF_LITE_MCU
)

# Compiler options for different architectures
target_compile_options(tflm_lib PRIVATE
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
    target_compile_options(tflm_lib PRIVATE
        -Os  # Optimize for size
        -fno-common
        -fstack-usage
        -Wall
        -Wextra
        -Wno-unused-parameter
    )
endif()

# Set target properties
set_target_properties(tflm_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE OFF
)

# Export include directories for parent projects
target_include_directories(tflm_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tensorflow>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/signal>
)