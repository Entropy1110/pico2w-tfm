#!/bin/bash
set -e


export PROJECT_ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" # 스크립트가 있는 디렉토리를 루트로 설정
export TFM_SOURCE_DIR="${PROJECT_ROOT_DIR}/pico2w-trusted-firmware-m"
export PICO_SDK_DIR="${PROJECT_ROOT_DIR}/pico-sdk"
export EXAMPLE_PROJECT_DIR="${PROJECT_ROOT_DIR}/pico2w-tfm-exmaple" # 사용자 예제 및 빌드 디렉토리 상위

export SPE_BUILD_DIR="${EXAMPLE_PROJECT_DIR}/build/spe"
export NSPE_BUILD_DIR="${EXAMPLE_PROJECT_DIR}/build/nspe"

NPROC=$(getconf _NPROCESSORS_ONLN 2>/dev/null || getconf NPROCESSORS_ONLN 2>/dev/null || echo 1)
echo "Using ${NPROC} cores for building."

# --- 1. SPE 빌드 ---
echo ""
echo "#####################################"
echo "# Building SPE (Secure Processing Environment)"
echo "#####################################"
mkdir -p "${SPE_BUILD_DIR}"
cd "${SPE_BUILD_DIR}" # SPE 빌드 디렉토리로 이동하여 CMake 실행
rm -rf ./* # 이전 빌드 캐시 정리

cmake -S "${TFM_SOURCE_DIR}" \
      -B . \
      -DTFM_PLATFORM=rpi/rp2350 \
      -DPICO_BOARD=pico2_w \
      -DTFM_PROFILE=profile_medium \
      -DPICO_SDK_PATH="${PICO_SDK_DIR}" \
      -DPLATFORM_DEFAULT_PROVISIONING=OFF

cmake --build . -- -j${NPROC} install
echo "SPE Build completed."
cd "${PROJECT_ROOT_DIR}"

echo ""
echo "######################################"
echo "# Building NSPE (Non-Secure Processing Environment)"
echo "######################################"
mkdir -p "${NSPE_BUILD_DIR}"
cd "${NSPE_BUILD_DIR}"

cmake -S "${EXAMPLE_PROJECT_DIR}" \
      -B . \
      -DTFM_PLATFORM=rpi/rp2350 \
      -DPICO_SDK_PATH="${PICO_SDK_DIR}" \
      -DCONFIG_SPE_PATH="${SPE_BUILD_DIR}/api_ns" \
      -DTFM_TOOLCHAIN_FILE="${SPE_BUILD_DIR}/api_ns/cmake/toolchain_ns_GNUARM.cmake" \
      -DPICO_BOARD=pico2_w

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
    ./pico_uf2.sh pico2w-tfm-exmaple build
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
echo "# Build process finished."
echo "######################################"