# Build System Guide

## Quick Start

```bash
# Standard build
./build.sh

# Clean build  
./build.sh clean

# Development mode (DEV_MODE)
./build.sh DEV_MODE

# Clean + DEV_MODE
./build.sh clean DEV_MODE
```

## Build Architecture

### Two-Stage Build Process
1. **SPE Build**: Secure Processing Environment
2. **NSPE Build**: Non-Secure Processing Environment (depends on SPE)

### Build Flow
```
Model Encryption → SPE Build → NSPE Build → UF2 Generation → Deployment
```

## Build Script Analysis

### Main Script: `build.sh`

#### DEV_MODE Detection
```bash
# Check for DEV_MODE argument
DEV_MODE_OPT=""
if [[ "$*" == *"DEV_MODE"* ]]; then
    DEV_MODE_OPT="-DDEV_MODE=ON"
    echo "DEV_MODE enabled - HUK key derivation debug features available"
fi
```

#### Model Encryption Step
```bash
echo "TinyMaix Model encryption using Test Key..."
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

#### SPE Build
```bash
cmake -S ./spe -B build/spe \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DTFM_PROFILE=profile_medium \
  -DPLATFORM_DEFAULT_PROVISIONING=OFF \
  -DTFM_TOOLCHAIN_FILE="${PROJECT_ROOT}/pico2w-trusted-firmware-m/toolchain_GNUARM.cmake" \
  -DCONFIG_TFM_SOURCE_PATH="${PROJECT_ROOT}/pico2w-trusted-firmware-m" \
  -DTFM_NS_REG_TEST=OFF \
  -DTFM_S_REG_TEST=OFF \
  -DTFM_PARTITION_ECHO_SERVICE=ON \
  -DTFM_PARTITION_TINYMAIX_INFERENCE=ON \
  ${DEV_MODE_OPT}

cmake --build build/spe -- -j8 install
```

#### NSPE Build
```bash
cmake -S ./nspe -B build/nspe \
    -DTFM_PLATFORM=rpi/rp2350 \
    -DPICO_BOARD=pico2_w \
    -DCONFIG_SPE_PATH="build/spe/api_ns" \
    -DTFM_TOOLCHAIN_FILE="build/spe/api_ns/cmake/toolchain_ns_GNUARM.cmake" \
    ${DEV_MODE_OPT}

cmake --build build/nspe -- -j8
```

#### Deployment
```bash
./pico_uf2.sh . ./build
picotool erase && picotool load build/spe/bin/bl2.uf2 && picotool load build/spe/bin/tfm_s_ns_signed.uf2 && picotool reboot
```

## CMake Configuration

### SPE Configuration: `spe/config/config_tinyml.cmake`

#### Platform Settings
```cmake
set(TFM_PLATFORM                        "rpi/rp2350" CACHE STRING   "Platform to build TF-M for.")
set(TFM_PROFILE                         ""          CACHE STRING    "Profile to use")
```

#### Partition Control
```cmake
set(TFM_PARTITION_CRYPTO                ON          CACHE BOOL      "Enable Crypto partition for PSA encryption")
set(TFM_PARTITION_ECHO_SERVICE           ON          CACHE BOOL      "Enable Echo Service partition")
set(TFM_PARTITION_TINYMAIX_INFERENCE    ON          CACHE BOOL      "Enable TinyMaix Inference partition")

# DEV_MODE option
set(DEV_MODE                            OFF         CACHE BOOL      "Enable development mode with debug features")
```

#### Test Configuration
```cmake
# All test-related configurations are disabled for tinyml
set(TFM_PARTITION_AUDIT_LOG             OFF         CACHE BOOL      "Enable Audit Log partition")
set(TEST_S                              OFF         CACHE BOOL      "Whether to build S regression tests")
set(TEST_NS                             OFF         CACHE BOOL      "Whether to build NS regression tests")
```

### NSPE Configuration: `nspe/CMakeLists.txt`

#### Dependencies
```cmake
if (NOT DEFINED CONFIG_SPE_PATH OR NOT EXISTS ${CONFIG_SPE_PATH})
    message(FATAL_ERROR "CONFIG_SPE_PATH = ${CONFIG_SPE_PATH} is not defined or incorrect.")
endif()
```

#### Compile Definitions
```cmake
target_compile_definitions(tfm_ns
    PUBLIC
        TFM_NS_LOG
        $<$<BOOL:${DEV_MODE}>:DEV_MODE>
)
```

### Partition Build: `partitions/tinymaix_inference/CMakeLists.txt`

#### Conditional Build
```cmake
if (NOT TFM_PARTITION_TINYMAIX_INFERENCE)
    return()
endif()
```

#### Source Files
```cmake
target_sources(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        tinymaix_inference.c
        tinymaix/src/tm_model.c
        tinymaix/src/tm_layers.c
        ../../models/encrypted_mnist_model_psa.c
)
```

#### DEV_MODE Compilation
```cmake
target_compile_definitions(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        TFM_PARTITION_TINYMAIX_INFERENCE
        $<$<BOOL:${DEV_MODE}>:DEV_MODE>
)
```

## Build Targets and Outputs

### SPE Build Artifacts
```
build/spe/bin/
├── bl2.uf2                    # Bootloader (MCUBoot)
├── tfm_s.bin                  # Secure firmware binary
├── tfm_s_ns_signed.uf2        # Combined signed firmware
└── ...
```

### NSPE Build Artifacts
```
build/nspe/bin/
├── tfm_ns.axf                 # Non-secure application (ELF)
├── tfm_ns.bin                 # Non-secure binary
└── tfm_ns.map                 # Memory map
```

### Generated API Files
```
build/spe/api_ns/
├── interface/
│   ├── include/               # PSA API headers
│   └── src/                   # PSA API implementations
├── cmake/                     # CMake configurations
└── platform/                 # Platform-specific files
```

## Development Builds vs Production

### DEV_MODE Effects

#### CMake Variables
- `DEV_MODE=ON`: Enables debug features
- Propagated to all build targets (SPE, NSPE, partitions)

#### Code Compilation
```c
#ifdef DEV_MODE
// Debug code only compiled in DEV_MODE
tfm_tinymaix_status_t tfm_tinymaix_get_model_key(uint8_t* key_buffer, size_t key_buffer_size);
#endif
```

#### Test Execution
- **Production**: All standard tests (Echo, PSA Crypto, TinyMaix inference)
- **DEV_MODE**: Only HUK key debugging test

## Advanced Build Options

### Manual CMake Configuration

#### SPE Manual Build
```bash
mkdir -p build/spe
cd build/spe

cmake ../../spe \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DTFM_PROFILE=profile_medium \
  -DTFM_PARTITION_ECHO_SERVICE=ON \
  -DTFM_PARTITION_TINYMAIX_INFERENCE=ON \
  -DDEV_MODE=ON

make -j8 install
```

#### NSPE Manual Build
```bash
mkdir -p build/nspe
cd build/nspe

cmake ../../nspe \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DCONFIG_SPE_PATH=../spe/api_ns \
  -DDEV_MODE=ON

make -j8
```

### Custom Partition Build
```bash
# Enable custom partition
cmake -DTFM_PARTITION_MY_SERVICE=ON ...

# With specific configuration
cmake -DMY_SERVICE_STACK_SIZE=0x4000 ...
```

## Toolchain Configuration

### GNU ARM Toolchain
```cmake
# toolchain_GNUARM.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# Cortex-M33 with TrustZone
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")
```

### Pico SDK Integration
```cmake
# Include Pico SDK
find_package(pico-sdk REQUIRED)

# Pico 2W specific settings
set(PICO_BOARD pico2_w)
set(PICO_PLATFORM rp2350)
```

## Build Optimization

### Parallel Builds
```bash
# Use multiple cores
cmake --build build/spe -- -j8
cmake --build build/nspe -- -j8

# Or with make
make -j8
```

### Incremental Builds
```bash
# Only rebuild changed components
cmake --build build/spe
cmake --build build/nspe
```

### Clean Builds
```bash
# Clean specific build
rm -rf build/spe build/nspe

# Or use build script
./build.sh clean
```

## Memory Configuration

### Memory Layout
```
# linker.ld configuration
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x10000000, LENGTH = 2048K
  RAM (rwx)       : ORIGIN = 0x20000000, LENGTH = 264K
  SCRATCH_X (rwx) : ORIGIN = 0x20040000, LENGTH = 4K
  SCRATCH_Y (rwx) : ORIGIN = 0x20041000, LENGTH = 4K
}
```

### Partition Memory
```yaml
# In manifest YAML
stack_size: "0x2000"  # 8KB stack for TinyMaix partition
```

## Troubleshooting

### Common Build Issues

#### SPE Build Failure
```
Error: TF-M platform not found
```
**Solution**: Check `TFM_PLATFORM=rpi/rp2350` setting

#### NSPE Build Failure
```
Error: CONFIG_SPE_PATH not found
```
**Solution**: Ensure SPE builds successfully first

#### Partition Build Failure
```
Error: Manifest validation failed
```
**Solution**: Check YAML syntax in manifest files

#### Memory Issues
```
Error: Section '.text' will not fit in region 'FLASH'
```
**Solution**: Optimize code size or adjust memory layout

### Debug Techniques

#### Verbose Build
```bash
cmake --build build/spe -- VERBOSE=1
```

#### Build Logs
```bash
# Redirect build output
./build.sh 2>&1 | tee build.log
```

#### Dependency Analysis
```bash
# Show build dependencies
cmake --build build/spe --target help
```

## Continuous Integration

### Automated Builds
```bash
#!/bin/bash
# ci-build.sh

set -e

echo "Starting CI build..."

# Clean build
./build.sh clean

# Test production build
echo "Testing production build..."
./build.sh

# Test DEV_MODE build
echo "Testing DEV_MODE build..."
./build.sh clean DEV_MODE

echo "CI build completed successfully"
```

### Build Validation
```bash
# Validate build outputs
test -f build/spe/bin/bl2.uf2 || exit 1
test -f build/spe/bin/tfm_s_ns_signed.uf2 || exit 1
test -f build/nspe/bin/tfm_ns.bin || exit 1

echo "Build validation passed"
```

## Advanced Build Features

### Conditional Compilation
```cmake
# Feature-specific conditional build
if(ENABLE_ADVANCED_CRYPTO)
    target_compile_definitions(my_target PRIVATE ADVANCED_CRYPTO_ENABLED)
    target_sources(my_target PRIVATE advanced_crypto.c)
endif()

# Platform-specific code
if(TFM_PLATFORM STREQUAL "rpi/rp2350")
    target_sources(my_target PRIVATE rp2350_specific.c)
endif()
```

### Custom Build Targets
```cmake
# Define custom target
add_custom_target(generate_keys
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/generate_keys.py
    COMMENT "Generating cryptographic keys"
)

# Add dependency
add_dependencies(my_partition generate_keys)

# Post-install processing
install(CODE "
    message(STATUS \"Generating UF2 files...\")
    execute_process(
        COMMAND ${CMAKE_SOURCE_DIR}/pico_uf2.sh ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
")
```

### Build Time Validation
```cmake
# Compile-time checks
add_custom_command(
    TARGET my_target PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Checking model encryption..."
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/verify_model.py
    COMMENT "Verifying encrypted model integrity"
)

# Post-link validation
add_custom_command(
    TARGET my_target POST_BUILD
    COMMAND arm-none-eabi-objdump -h $<TARGET_FILE:my_target>
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/check_memory_usage.py $<TARGET_FILE:my_target>
    COMMENT "Analyzing memory usage"
)
```

## Build Performance Optimization

### Compiler Cache
```bash
# Use ccache (if available)
export CC="ccache arm-none-eabi-gcc"
export CXX="ccache arm-none-eabi-g++"

# Check cache statistics
ccache -s
```

### Parallel Build Optimization
```cmake
# Set number of parallel jobs in CMake
set(CMAKE_BUILD_PARALLEL_LEVEL 8)

# Limit memory usage during linking
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-keep-memory")
```

### Build Time Profiling
```bash
# Measure build time
time ./build.sh

# Detailed timing information
cmake --build build/spe --verbose -- --time

# Per-compilation unit timing
CMAKE_VERBOSE_MAKEFILE=ON cmake --build build/spe
```

## Deployment Automation

### Automated Device Deployment
```bash
#!/bin/bash
# auto_deploy.sh

# Detect device
if picotool info >/dev/null 2>&1; then
    echo "Pico device detected"
    
    # Auto-deploy
    picotool erase
    picotool load build/spe/bin/bl2.uf2
    picotool load build/spe/bin/tfm_s_ns_signed.uf2
    picotool reboot
    
    echo "Deployment complete"
else
    echo "Pico device not found"
    echo "Connect in BOOTSEL mode"
fi
```

### Release Packaging
```cmake
# Release packaging using CPack
include(CPack)

set(CPACK_PACKAGE_NAME "pico2w-tfm-tinyml")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION "TinyMaix TF-M Integration for Pico 2W")

# Include UF2 files
install(FILES 
    ${CMAKE_BINARY_DIR}/build/spe/bin/bl2.uf2
    ${CMAKE_BINARY_DIR}/build/spe/bin/tfm_s_ns_signed.uf2
    DESTINATION firmware
)

# Include documentation
install(DIRECTORY docs/ DESTINATION docs)
install(FILES README.md DESTINATION .)
```

## Environment-Specific Build Configurations

### Development Environment
```bash
# Quick build for developers
export DEV_BUILD=1
export SKIP_TESTS=1
./build.sh DEV_MODE
```

### CI/CD Environment
```bash
# Strict build for CI
export STRICT_BUILD=1
export ENABLE_ALL_WARNINGS=1
export TREAT_WARNINGS_AS_ERRORS=1
./build.sh
```

### Production Environment
```bash
# Production release build
export PRODUCTION_BUILD=1
export OPTIMIZE_SIZE=1
export STRIP_DEBUG=1
./build.sh
```

## Next Steps

- **[TF-M Architecture](./tfm-architecture.md)**: Understand the overall system
- **[Secure Partitions](./secure-partitions.md)**: Add custom secure services
- **[Troubleshooting](./troubleshooting.md)**: Solve build and runtime issues