#!/bin/bash

# TF-M Provision Application Build Script
# This script builds both SPE and NSPE for the provision application

set -e

PROJECT_ROOT="/Users/entropy/pico2w-tfm-tflm"
BUILD_DIR="${PROJECT_ROOT}/build"
PICO_SDK_PATH="/Users/entropy/pico2w-tfm-tflm/pico-sdk"

echo "========================================"
echo "TF-M Provision Application Build"
echo "========================================"

# Clean previous build artifacts
echo "Cleaning previous build directories..."
# rm -rf "${BUILD_DIR}/spe"
rm -rf "${BUILD_DIR}/nspe"
echo "Cleaning complete."

# Create build directories
mkdir -p "${BUILD_DIR}/spe"
mkdir -p "${BUILD_DIR}/nspe"

echo ""
echo "Building SPE (Secure Processing Environment)..."
echo "==============================================="

cd "${PROJECT_ROOT}"

cmake -S ./tflm_spe -B "${BUILD_DIR}/spe" \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DTFM_PROFILE=profile_medium \
  -DPLATFORM_DEFAULT_PROVISIONING=OFF \
  -DTFM_TOOLCHAIN_FILE="${PROJECT_ROOT}/pico2w-trusted-firmware-m/toolchain_GNUARM.cmake" \
  -DCONFIG_TFM_SOURCE_PATH="${PROJECT_ROOT}/pico2w-trusted-firmware-m" \
  -DTFM_NS_REG_TEST=OFF \
  -DTFM_S_REG_TEST=OFF \
  -DTFM_PARTITION_ECHO_SERVICE=ON

echo ""
echo "Installing SPE build artifacts..."
cmake --build "${BUILD_DIR}/spe" -- -j8 install

echo ""
echo "Building NSPE (Non-Secure Processing Environment)..."
echo "====================================================="

cmake -S ./tflm_ns -B "${BUILD_DIR}/nspe" \
    -DTFM_PLATFORM=rpi/rp2350 \
    -DPICO_BOARD=pico2_w \
    -DCONFIG_SPE_PATH="${BUILD_DIR}/spe/api_ns" \
    -DTFM_TOOLCHAIN_FILE="${BUILD_DIR}/spe/api_ns/cmake/toolchain_ns_GNUARM.cmake"

cmake --build "${BUILD_DIR}/nspe" -- -j8

echo ""
echo "========================================"
echo "Build completed successfully!"
echo "========================================"
echo "SPE binaries: ${BUILD_DIR}/spe/bin/"
echo "NSPE binaries: ${BUILD_DIR}/nspe/bin/"
echo ""
./pico_uf2.sh . ./build
picotool erase && picotool load ${BUILD_DIR}/spe/bin/bl2.uf2 && picotool load ${BUILD_DIR}/spe/bin/tfm_s_ns_signed.uf2 && picotool reboot
