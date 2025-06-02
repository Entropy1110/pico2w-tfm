#!/bin/bash
set -e

export PROJECT_ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export TFM_SOURCE_DIR="${PROJECT_ROOT_DIR}/pico2w-trusted-firmware-m"
export PICO_SDK_DIR="${PROJECT_ROOT_DIR}/pico-sdk"
export TFLM_SPE_DIR="${PROJECT_ROOT_DIR}/tflm_spe"
export TFLM_NS_DIR="${PROJECT_ROOT_DIR}/tflm_ns"
export APP_BROKER_DIR="${PROJECT_ROOT_DIR}/app_broker"
export SPE_BUILD_DIR="${PROJECT_ROOT_DIR}/build/spe"
export NSPE_BUILD_DIR="${PROJECT_ROOT_DIR}/build/nspe"

NPROC=$(getconf _NPROCESSORS_ONLN 2>/dev/null || getconf NPROCESSORS_ONLN 2>/dev/null || echo 1)
echo "Using ${NPROC} cores for building."

echo ""
echo "#####################################"
echo "# Building SPE (Secure Processing Environment)"
echo "#####################################"

# Clean and create SPE build directory
mkdir -p "${SPE_BUILD_DIR}"
cd "${SPE_BUILD_DIR}"
rm -rf ./*

# Configure SPE build
cmake -S "${TFLM_SPE_DIR}" \
      -B . \
      -DTFM_PLATFORM=rpi/rp2350 \
      -DPICO_BOARD=pico2_w \
      -DTFM_PROFILE=profile_medium \
      -DPICO_SDK_PATH="${PICO_SDK_DIR}" \
      -DTFM_TOOLCHAIN_FILE="${TFM_SOURCE_DIR}/toolchain_GNUARM.cmake" \
      -DCONFIG_TFM_SOURCE_PATH="${TFM_SOURCE_DIR}" \
      -DTFM_EXTRA_PARTITION_PATHS="${TFLM_SPE_DIR}/partitions" \
      -DTFM_EXTRA_MANIFEST_LIST_FILES="${TFLM_SPE_DIR}/tflm_manifest_list.txt" \
      -DPLATFORM_DEFAULT_PROVISIONING=OFF

# Build and install SPE
cmake --build . --target install -- -j${NPROC}
echo "SPE Build completed."

cd "${PROJECT_ROOT_DIR}"

echo ""
echo "######################################"
echo "# Building NSPE (Non-Secure Processing Environment)"
echo "######################################"

# Clean and create NSPE build directory
mkdir -p "${NSPE_BUILD_DIR}"
cd "${NSPE_BUILD_DIR}"
rm -rf ./*

# Configure NSPE build
cmake -S "${TFLM_NS_DIR}" \
      -B . \
      -DTFM_PLATFORM=rpi/rp2350 \
      -DCONFIG_SPE_PATH="${SPE_BUILD_DIR}/api_ns" \
      -DTFM_TOOLCHAIN_FILE="${SPE_BUILD_DIR}/api_ns/cmake/toolchain_ns_GNUARM.cmake" \
      -DPICO_BOARD=pico2_w

# Build NSPE
cmake --build . -- -j${NPROC}
echo "NSPE Build completed."

cd "${PROJECT_ROOT_DIR}"

echo ""
echo "######################################"
echo "# Converting binaries to UF2 format"
echo "######################################"

if [ -f "./pico_uf2.sh" ]; then
    chmod +x ./pico_uf2.sh
    chmod +x ./uf2conv.py
    ./pico_uf2.sh . build
    echo "UF2 conversion completed."
    echo "Generated UF2 files can be found in ${SPE_BUILD_DIR}/bin/"
    echo "  - bl2.uf2"
    echo "  - tfm_s_ns_signed.uf2"
    echo "  - provisioning_bundle.uf2 (if provisioning was built)"
else
    echo "Warning: pico_uf2.sh not found in ${PROJECT_ROOT_DIR}. Skipping UF2 conversion."
fi

echo ""
echo "######################################"
echo "# Build Summary"
echo "######################################"
echo "SPE binaries: ${SPE_BUILD_DIR}/bin/"
echo "NSPE binaries: ${NSPE_BUILD_DIR}/"

# Show generated files
echo ""
echo "Generated files:"
if [ -f "${SPE_BUILD_DIR}/bin/tfm_s.axf" ]; then
    echo "  ✓ SPE: ${SPE_BUILD_DIR}/bin/tfm_s.axf"
fi
if [ -f "${NSPE_BUILD_DIR}/tfm_ns.elf" ]; then
    echo "  ✓ NSPE: ${NSPE_BUILD_DIR}/tfm_ns.elf"
fi
if [ -f "${SPE_BUILD_DIR}/bin/bl2.uf2" ]; then
    echo "  ✓ UF2: ${SPE_BUILD_DIR}/bin/bl2.uf2"
fi
if [ -f "${SPE_BUILD_DIR}/bin/tfm_s_ns_signed.uf2" ]; then
    echo "  ✓ UF2: ${SPE_BUILD_DIR}/bin/tfm_s_ns_signed.uf2"
fi

echo ""
echo "######################################"
echo "# Build process finished."
echo "######################################"
