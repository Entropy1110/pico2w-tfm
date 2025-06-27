#!/bin/bash

# TF-M Provision Application Build Script
# This script builds both SPE and NSPE for the provision application

set -e

PROJECT_ROOT=$(dirname "$(realpath "$0")")
BUILD_DIR="${PROJECT_ROOT}/build"

# Check for DEV_MODE argument
DEV_MODE_OPT=""
if [[ "$*" == *"DEV_MODE"* ]]; then
    DEV_MODE_OPT="-DDEV_MODE=ON"
    echo "========================================"
    echo "TF-M Provision Application Build (DEV_MODE)"
    echo "========================================"
    echo "DEV_MODE enabled - HUK key derivation debug features available"
else
    echo "========================================"
    echo "TF-M Provision Application Build"
    echo "========================================"
fi

# Clean previous build artifacts
echo "Cleaning previous build directories..."
#if clean option is enabled, uncomment the following lines
if [ "$1" == "clean" ]; then
    echo "Cleaning SPE, NSPE, TinyMaix build directories..."
    rm -rf "${BUILD_DIR}/spe"
    rm -rf "${BUILD_DIR}/nspe"
else
    echo "Skipping clean. Use 'clean' argument to remove previous builds."
fi

echo "Cleaning complete."

# TinyMaix is now integrated directly in the secure partition
echo ""
echo "TinyMaix integrated directly in secure partition"
echo "================================================"

# Create build directories
mkdir -p "${BUILD_DIR}/spe"
mkdir -p "${BUILD_DIR}/nspe"

cd "${PROJECT_ROOT}"


echo ""
echo "TinyMaix Model encryption using Test Key..."
echo "==========================================="
python3 tools/tinymaix_model_encryptor.py --input models/mnist_valid_q.h --output models/encrypted_mnist_model_psa.bin --key-file models/model_key_psa.bin --generate-c-header


echo ""
echo "Building SPE (Secure Processing Environment)..."
echo "==============================================="


cmake -S ./spe -B "${BUILD_DIR}/spe" \
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

echo ""
echo "Installing SPE build artifacts..."
cmake --build "${BUILD_DIR}/spe" -- -j8 install

echo ""
echo "Building NSPE (Non-Secure Processing Environment)..."
echo "====================================================="

cmake -S ./nspe -B "${BUILD_DIR}/nspe" \
    -DTFM_PLATFORM=rpi/rp2350 \
    -DPICO_BOARD=pico2_w \
    -DCONFIG_SPE_PATH="${BUILD_DIR}/spe/api_ns" \
    -DTFM_TOOLCHAIN_FILE="${BUILD_DIR}/spe/api_ns/cmake/toolchain_ns_GNUARM.cmake" \
    ${DEV_MODE_OPT}

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
