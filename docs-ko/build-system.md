# 빌드 시스템 가이드

## 빠른 시작

```bash
# 표준 빌드
./build.sh

# 클린 빌드  
./build.sh clean

# 개발 모드 (DEV_MODE)
./build.sh DEV_MODE

# 클린 + DEV_MODE
./build.sh clean DEV_MODE
```

## 빌드 아키텍처

### 2단계 빌드 프로세스
1. **SPE 빌드**: 보안 처리 환경 (Secure Processing Environment)
2. **NSPE 빌드**: 비보안 처리 환경 (Non-Secure Processing Environment) - SPE에 의존

### 빌드 흐름
```
모델 암호화 → SPE 빌드 → NSPE 빌드 → UF2 생성 → 배포
```

## 빌드 스크립트 분석

### 메인 스크립트: `build.sh`

#### DEV_MODE 감지
```bash
# DEV_MODE 인수 확인
DEV_MODE_OPT=""
if [[ "$*" == *"DEV_MODE"* ]]; then
    DEV_MODE_OPT="-DDEV_MODE=ON"
    echo "DEV_MODE 활성화 - HUK 키 파생 디버그 기능 사용 가능"
fi
```

#### 모델 암호화 단계
```bash
echo "테스트 키를 사용한 TinyMaix 모델 암호화..."
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

#### SPE 빌드
```bash
cmake -S ./tflm_spe -B build/spe \
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

#### NSPE 빌드
```bash
cmake -S ./tflm_ns -B build/nspe \
    -DTFM_PLATFORM=rpi/rp2350 \
    -DPICO_BOARD=pico2_w \
    -DCONFIG_SPE_PATH="build/spe/api_ns" \
    -DTFM_TOOLCHAIN_FILE="build/spe/api_ns/cmake/toolchain_ns_GNUARM.cmake" \
    ${DEV_MODE_OPT}

cmake --build build/nspe -- -j8
```

#### 배포
```bash
./pico_uf2.sh . ./build
picotool erase && picotool load build/spe/bin/bl2.uf2 && picotool load build/spe/bin/tfm_s_ns_signed.uf2 && picotool reboot
```

## CMake 구성

### SPE 구성: `tflm_spe/config/config_tflm.cmake`

#### 플랫폼 설정
```cmake
set(TFM_PLATFORM                        "rpi/rp2350" CACHE STRING   "빌드할 TF-M 플랫폼")
set(TFM_PROFILE                         ""          CACHE STRING    "사용할 프로필")
```

#### 파티션 제어
```cmake
set(TFM_PARTITION_CRYPTO                ON          CACHE BOOL      "PSA 암호화를 위한 Crypto 파티션 활성화")
set(TFM_PARTITION_ECHO_SERVICE           ON          CACHE BOOL      "Echo Service 파티션 활성화")
set(TFM_PARTITION_TINYMAIX_INFERENCE    ON          CACHE BOOL      "TinyMaix Inference 파티션 활성화")

# DEV_MODE 옵션
set(DEV_MODE                            OFF         CACHE BOOL      "디버그 기능이 포함된 개발 모드 활성화")
```

#### 테스트 구성
```cmake
# 모든 테스트 관련 구성은 TFLM에서 비활성화됨
set(TFM_PARTITION_AUDIT_LOG             OFF         CACHE BOOL      "Audit Log 파티션 활성화")
set(TEST_S                              OFF         CACHE BOOL      "S 회귀 테스트 빌드 여부")
set(TEST_NS                             OFF         CACHE BOOL      "NS 회귀 테스트 빌드 여부")
```

### NSPE 구성: `tflm_ns/CMakeLists.txt`

#### 의존성
```cmake
if (NOT DEFINED CONFIG_SPE_PATH OR NOT EXISTS ${CONFIG_SPE_PATH})
    message(FATAL_ERROR "CONFIG_SPE_PATH = ${CONFIG_SPE_PATH}가 정의되지 않았거나 잘못되었습니다.")
endif()
```

#### 컴파일 정의
```cmake
target_compile_definitions(tfm_ns
    PUBLIC
        TFM_NS_LOG
        $<$<BOOL:${DEV_MODE}>:DEV_MODE>
)
```

### 파티션 빌드: `partitions/tinymaix_inference/CMakeLists.txt`

#### 조건부 빌드
```cmake
if (NOT TFM_PARTITION_TINYMAIX_INFERENCE)
    return()
endif()
```

#### 소스 파일
```cmake
target_sources(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        tinymaix_inference.c
        tinymaix/src/tm_model.c
        tinymaix/src/tm_layers.c
        ../../models/encrypted_mnist_model_psa.c
)
```

#### DEV_MODE 컴파일
```cmake
target_compile_definitions(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        TFM_PARTITION_TINYMAIX_INFERENCE
        $<$<BOOL:${DEV_MODE}>:DEV_MODE>
)
```

## 빌드 타겟 및 출력

### SPE 빌드 산출물
```
build/spe/bin/
├── bl2.uf2                    # 부트로더 (MCUBoot)
├── tfm_s.bin                  # 보안 펌웨어 바이너리
├── tfm_s_ns_signed.uf2        # 결합된 서명된 펌웨어
└── ...
```

### NSPE 빌드 산출물
```
build/nspe/bin/
├── tfm_ns.axf                 # 비보안 애플리케이션 (ELF)
├── tfm_ns.bin                 # 비보안 바이너리
└── tfm_ns.map                 # 메모리 맵
```

### 생성된 API 파일
```
build/spe/api_ns/
├── interface/
│   ├── include/               # PSA API 헤더
│   └── src/                   # PSA API 구현
├── cmake/                     # CMake 구성
└── platform/                 # 플랫폼별 파일
```

## 개발 빌드 vs 프로덕션

### DEV_MODE 효과

#### CMake 변수
- `DEV_MODE=ON`: 디버그 기능 활성화
- 모든 빌드 타겟(SPE, NSPE, 파티션)에 전파

#### 코드 컴파일
```c
#ifdef DEV_MODE
// DEV_MODE에서만 컴파일되는 디버그 코드
tfm_tinymaix_status_t tfm_tinymaix_get_model_key(uint8_t* key_buffer, size_t key_buffer_size);
#endif
```

#### 테스트 실행
- **프로덕션**: 모든 표준 테스트 (Echo, PSA Crypto, TinyMaix 추론)
- **DEV_MODE**: HUK 키 디버깅 테스트만

## 고급 빌드 옵션

### 수동 CMake 구성

#### SPE 수동 빌드
```bash
mkdir -p build/spe
cd build/spe

cmake ../../tflm_spe \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DTFM_PROFILE=profile_medium \
  -DTFM_PARTITION_ECHO_SERVICE=ON \
  -DTFM_PARTITION_TINYMAIX_INFERENCE=ON \
  -DDEV_MODE=ON

make -j8 install
```

#### NSPE 수동 빌드
```bash
mkdir -p build/nspe
cd build/nspe

cmake ../../tflm_ns \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DCONFIG_SPE_PATH=../spe/api_ns \
  -DDEV_MODE=ON

make -j8
```

### 커스텀 파티션 빌드
```bash
# 커스텀 파티션 활성화
cmake -DTFM_PARTITION_MY_SERVICE=ON ...

# 특정 구성으로
cmake -DMY_SERVICE_STACK_SIZE=0x4000 ...
```

## 툴체인 구성

### GNU ARM 툴체인
```cmake
# toolchain_GNUARM.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# TrustZone이 있는 Cortex-M33
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")
```

### Pico SDK 통합
```cmake
# Pico SDK 포함
find_package(pico-sdk REQUIRED)

# Pico 2W 특정 설정
set(PICO_BOARD pico2_w)
set(PICO_PLATFORM rp2350)
```

## 빌드 최적화

### 병렬 빌드
```bash
# 여러 코어 사용
cmake --build build/spe -- -j8
cmake --build build/nspe -- -j8

# 또는 make 사용
make -j8
```

### 증분 빌드
```bash
# 변경된 컴포넌트만 재빌드
cmake --build build/spe
cmake --build build/nspe
```

### 클린 빌드
```bash
# 특정 빌드 클린
rm -rf build/spe build/nspe

# 또는 빌드 스크립트 사용
./build.sh clean
```

## 메모리 구성

### 메모리 레이아웃
```
# linker.ld 구성
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x10000000, LENGTH = 2048K
  RAM (rwx)       : ORIGIN = 0x20000000, LENGTH = 264K
  SCRATCH_X (rwx) : ORIGIN = 0x20040000, LENGTH = 4K
  SCRATCH_Y (rwx) : ORIGIN = 0x20041000, LENGTH = 4K
}
```

### 파티션 메모리
```yaml
# 매니페스트 YAML에서
stack_size: "0x2000"  # TinyMaix 파티션용 8KB 스택
```

## 문제 해결

### 일반적인 빌드 문제

#### SPE 빌드 실패
```
오류: TF-M 플랫폼을 찾을 수 없음
```
**해결책**: `TFM_PLATFORM=rpi/rp2350` 설정 확인

#### NSPE 빌드 실패
```
오류: CONFIG_SPE_PATH를 찾을 수 없음
```
**해결책**: SPE가 먼저 성공적으로 빌드되었는지 확인

#### 파티션 빌드 실패
```
오류: 매니페스트 검증 실패
```
**해결책**: 매니페스트 파일의 YAML 구문 확인

#### 메모리 문제
```
오류: '.text' 섹션이 'FLASH' 영역에 맞지 않음
```
**해결책**: 코드 크기 최적화 또는 메모리 레이아웃 조정

### 디버그 기법

#### 상세 빌드
```bash
cmake --build build/spe -- VERBOSE=1
```

#### 빌드 로그
```bash
# 빌드 출력 리디렉션
./build.sh 2>&1 | tee build.log
```

#### 의존성 분석
```bash
# 빌드 의존성 표시
cmake --build build/spe --target help
```

## 지속적 통합

### 자동화된 빌드
```bash
#!/bin/bash
# ci-build.sh

set -e

echo "CI 빌드 시작..."

# 클린 빌드
./build.sh clean

# 프로덕션 빌드 테스트
echo "프로덕션 빌드 테스트 중..."
./build.sh

# DEV_MODE 빌드 테스트
echo "DEV_MODE 빌드 테스트 중..."
./build.sh clean DEV_MODE

echo "CI 빌드 성공적으로 완료"
```

### 빌드 검증
```bash
# 빌드 출력 검증
test -f build/spe/bin/bl2.uf2 || exit 1
test -f build/spe/bin/tfm_s_ns_signed.uf2 || exit 1
test -f build/nspe/bin/tfm_ns.bin || exit 1

echo "빌드 검증 통과"
```

## 고급 빌드 기능

### 조건부 컴파일
```cmake
# 기능별 조건부 빌드
if(ENABLE_ADVANCED_CRYPTO)
    target_compile_definitions(my_target PRIVATE ADVANCED_CRYPTO_ENABLED)
    target_sources(my_target PRIVATE advanced_crypto.c)
endif()

# 플랫폼별 코드
if(TFM_PLATFORM STREQUAL "rpi/rp2350")
    target_sources(my_target PRIVATE rp2350_specific.c)
endif()
```

### 커스텀 빌드 타겟
```cmake
# 커스텀 타겟 정의
add_custom_target(generate_keys
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/generate_keys.py
    COMMENT "Generating cryptographic keys"
)

# 의존성 추가
add_dependencies(my_partition generate_keys)

# 설치 후 처리
install(CODE "
    message(STATUS \"Generating UF2 files...\")
    execute_process(
        COMMAND ${CMAKE_SOURCE_DIR}/pico_uf2.sh ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
")
```

### 빌드 타임 검증
```cmake
# 컴파일 시점 검사
add_custom_command(
    TARGET my_target PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Checking model encryption..."
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/verify_model.py
    COMMENT "Verifying encrypted model integrity"
)

# 링크 후 검증
add_custom_command(
    TARGET my_target POST_BUILD
    COMMAND arm-none-eabi-objdump -h $<TARGET_FILE:my_target>
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/check_memory_usage.py $<TARGET_FILE:my_target>
    COMMENT "Analyzing memory usage"
)
```

## 빌드 성능 최적화

### 컴파일러 캐시
```bash
# ccache 사용 (가능한 경우)
export CC="ccache arm-none-eabi-gcc"
export CXX="ccache arm-none-eabi-g++"

# 캐시 통계 확인
ccache -s
```

### 병렬 빌드 최적화
```cmake
# CMake에서 병렬 작업 수 설정
set(CMAKE_BUILD_PARALLEL_LEVEL 8)

# 링크 시 메모리 사용량 제한
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-keep-memory")
```

### 빌드 시간 프로파일링
```bash
# 빌드 시간 측정
time ./build.sh

# 상세한 타이밍 정보
cmake --build build/spe --verbose -- --time

# 컴파일 단위별 시간 측정
CMAKE_VERBOSE_MAKEFILE=ON cmake --build build/spe
```

## 배포 자동화

### 자동 디바이스 배포
```bash
#!/bin/bash
# auto_deploy.sh

# 디바이스 감지
if picotool info >/dev/null 2>&1; then
    echo "Pico 디바이스 감지됨"
    
    # 자동 배포
    picotool erase
    picotool load build/spe/bin/bl2.uf2
    picotool load build/spe/bin/tfm_s_ns_signed.uf2
    picotool reboot
    
    echo "배포 완료"
else
    echo "Pico 디바이스를 찾을 수 없음"
    echo "BOOTSEL 모드로 연결하세요"
fi
```

### 릴리스 패키징
```cmake
# CPack을 사용한 릴리스 패키징
include(CPack)

set(CPACK_PACKAGE_NAME "pico2w-tfm-tflm")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION "TinyMaix TF-M Integration for Pico 2W")

# UF2 파일 포함
install(FILES 
    ${CMAKE_BINARY_DIR}/build/spe/bin/bl2.uf2
    ${CMAKE_BINARY_DIR}/build/spe/bin/tfm_s_ns_signed.uf2
    DESTINATION firmware
)

# 문서 포함
install(DIRECTORY docs/ DESTINATION docs)
install(FILES README.md DESTINATION .)
```

## 환경별 빌드 구성

### 개발 환경
```bash
# 개발자용 빠른 빌드
export DEV_BUILD=1
export SKIP_TESTS=1
./build.sh DEV_MODE
```

### CI/CD 환경
```bash
# CI에서 사용하는 엄격한 빌드
export STRICT_BUILD=1
export ENABLE_ALL_WARNINGS=1
export TREAT_WARNINGS_AS_ERRORS=1
./build.sh
```

### 프로덕션 환경
```bash
# 프로덕션 릴리스 빌드
export PRODUCTION_BUILD=1
export OPTIMIZE_SIZE=1
export STRIP_DEBUG=1
./build.sh
```

## 다음 단계

빌드 시스템을 이해했다면 다음 문서를 참조하세요:

- **[TF-M 아키텍처](./tfm-architecture.md)** - 전체 시스템 이해
- **[보안 파티션](./secure-partitions.md)** - 커스텀 보안 서비스 추가
- **[문제 해결](./troubleshooting.md)** - 빌드 및 런타임 문제 해결