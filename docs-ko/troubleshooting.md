# 문제 해결 가이드

## 일반적인 문제와 해결책

### 빌드 문제

#### 1. SPE 빌드 실패

**오류: TF-M 플랫폼을 찾을 수 없음**
```
CMake Error: rpi/rp2350에 대한 플랫폼 정의를 찾을 수 없음
```
**원인**: TF-M 플랫폼 구성이 누락되었거나 경로가 잘못됨  
**해결책**:
```bash
# TF-M 소스 경로 확인
ls pico2w-trusted-firmware-m/platform/ext/target/rpi/rp2350

# 빌드 명령 검증
cmake -S ./tflm_spe -B build/spe -DTFM_PLATFORM=rpi/rp2350 ...
```

**오류: 매니페스트 검증 실패**
```
Error: 잘못된 매니페스트 파일: partitions/my_service/manifest.yaml
```
**원인**: YAML 구문 오류 또는 잘못된 파티션 구성  
**해결책**:
```bash
# YAML 구문 검증
python3 -c "import yaml; yaml.safe_load(open('partitions/my_service/manifest.yaml'))"

# 필수 필드 확인
cat partitions/echo_service/echo_service_manifest.yaml  # 참고용
```

**오류: 파티션을 찾을 수 없음**
```
Error: TFM_PARTITION_MY_SERVICE가 ON이지만 파티션을 찾을 수 없음
```
**원인**: 파티션이 manifest_list.yaml에 등록되지 않음  
**해결책**:
```yaml
# partitions/manifest_list.yaml에 추가
{
  "description": "My Service",
  "manifest": "partitions/my_service/my_service_manifest.yaml",
  "output_path": "partitions/my_service", 
  "conditional": "TFM_PARTITION_MY_SERVICE",
  "version_major": 0,
  "version_minor": 1,
  "pid": 446  # 고유한 PID 선택
}
```

#### 2. NSPE 빌드 실패

**오류: CONFIG_SPE_PATH를 찾을 수 없음**
```
FATAL_ERROR CONFIG_SPE_PATH = build/spe/api_ns가 정의되지 않았거나 잘못됨
```
**원인**: SPE 빌드가 완료되지 않았거나 경로가 잘못됨  
**해결책**:
```bash
# SPE가 먼저 빌드되었는지 확인
cmake --build build/spe -- -j8 install

# API 파일이 존재하는지 확인
ls build/spe/api_ns/interface/
```

**오류: PSA 함수에 대한 정의되지 않은 참조**
```
undefined reference to `psa_connect'
```
**원인**: 링크에서 PSA API 라이브러리 누락  
**해결책**:
```cmake
# CMakeLists.txt에서
target_link_libraries(my_target
    PRIVATE
        tfm_api_ns  # PSA API 라이브러리 추가
)
```

#### 3. 모델 암호화 문제

**오류: 모델 암호화 스크립트 실패**
```
FileNotFoundError: [Errno 2] No such file or directory: 'models/mnist_valid_q.h'
```
**원인**: 모델 소스 파일 누락  
**해결책**:
```bash
# 모델 파일이 존재하는지 확인
ls models/mnist_valid_q.h

# 수동으로 암호화 실행
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

### 런타임 문제

#### 1. 디바이스 부팅 문제

**증상: 디바이스가 부팅되지 않거나 시리얼 출력이 없음**
**디버깅 단계**:
```bash
# UF2 파일이 존재하는지 확인
ls build/spe/bin/bl2.uf2
ls build/spe/bin/tfm_s_ns_signed.uf2

# 수동 배포 시도
picotool info
picotool erase
picotool load build/spe/bin/bl2.uf2
picotool load build/spe/bin/tfm_s_ns_signed.uf2
picotool reboot

# 시리얼 연결 확인
# Linux/Mac: screen /dev/ttyACM0 115200
# Windows: PuTTY 또는 유사한 도구 사용
```

#### 2. PSA 서비스 실패

**오류: PSA_ERROR_CONNECTION_REFUSED (-150)**
```
[Service Test] ✗ 연결 실패: -150
```
**원인**: 서비스를 사용할 수 없거나 보안 정책이 거부함  
**해결책**:
```c
// 서비스 SID가 올바른지 확인
#define MY_SERVICE_SID 0x00000108U  // 매니페스트와 일치해야 함

// 파티션이 활성화되었는지 확인
// config_tflm.cmake에서:
set(TFM_PARTITION_MY_SERVICE ON CACHE BOOL "Enable My Service")

// 매니페스트에서 non_secure_clients 확인
"non_secure_clients": true
```

**오류: PSA_ERROR_INVALID_HANDLE (-136)**
```
[Service Test] ✗ 잘못된 핸들: -136
```
**원인**: 핸들이 제대로 획득되지 않았거나 이미 닫힘  
**해결책**:
```c
// 사용 전에 핸들이 유효한지 확인
psa_handle_t handle = psa_connect(SERVICE_SID, 1);
if (handle <= 0) {
    // 핸들 연결 오류 처리
    return SERVICE_ERROR_CONNECTION_FAILED;
}

// 호출에 핸들 사용
psa_status_t status = psa_call(handle, operation, ...);

// 항상 핸들 닫기
psa_close(handle);
```

#### 3. TinyMaix 추론 문제

**오류: 모델 로드 실패**
```
[TinyMaix Test] ✗ 모델 로드 실패: -3
```
**디버깅 단계**:
```c
// 보안 파티션 로그에서 확인
INFO_UNPRIV("모델 복호화 상태: %d\\n", decrypt_status);
INFO_UNPRIV("TinyMaix 로드 상태: %d\\n", tm_load_status);

// 일반적인 원인:
// 1. 잘못된 암호화 키
// 2. 손상된 암호화 모델
// 3. 메모리 부족
// 4. 잘못된 모델 형식
```

**오류: 잘못된 모델 매직**
```
ERROR: 잘못된 모델 매직
```
**원인**: 복호화 실패 또는 잘못된 키  
**해결책**:
```bash
# 모델 암호화 검증
python3 test_cbc_decrypt.py

# DEV_MODE에서 키 파생 확인
./build.sh DEV_MODE
# 출력에서 "HUK-derived key:" 찾기

# 올바른 키로 모델 재암호화
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

**오류: 추론 실패**
```
[TinyMaix Test] ✗ 추론 실패: -4
```
**디버깅 단계**:
```c
// 모델이 로드되었는지 확인
if (!g_model_loaded) {
    return PSA_ERROR_BAD_STATE;
}

// 입력 차원 검증
if (input_size != 28*28) {
    return PSA_ERROR_INVALID_ARGUMENT;
}

// TinyMaix 오류 코드 확인
if (tm_res == TM_ERR_OOM) {
    // 메모리 부족 - 스택 크기 증가
}
```

### 메모리 문제

#### 1. 스택 오버플로

**증상: 임의 크래시 또는 하드 폴트**
**디버깅**:
```yaml
# 매니페스트에서 스택 크기 증가
stack_size: "0x4000"  # 0x2000에서 증가
```

**스택 사용량 확인**:
```c
// 보안 파티션에서
void check_stack_usage(void)
{
    extern uint32_t __stack_top__;
    extern uint32_t __stack_limit__;
    
    uint32_t stack_size = (uint32_t)&__stack_top__ - (uint32_t)&__stack_limit__;
    uint32_t current_sp;
    __asm volatile ("mov %0, sp" : "=r" (current_sp));
    uint32_t used = (uint32_t)&__stack_top__ - current_sp;
    
    INFO_UNPRIV("스택 사용량: %d/%d 바이트 (%.1f%%)\\n", 
                used, stack_size, (float)used * 100.0f / stack_size);
}
```

#### 2. 버퍼 오버플로

**오류: 보안 폴트 또는 메모리 손상**
**예방**:
```c
// 항상 버퍼 크기 검증
if (input_size > MAX_BUFFER_SIZE) {
    return PSA_ERROR_INVALID_ARGUMENT;
}

// 안전한 문자열 함수 사용
size_t bytes_to_copy = MIN(src_size, dst_size - 1);
memcpy(dst, src, bytes_to_copy);
dst[bytes_to_copy] = '\\0';

// psa_read 반환 값 확인
size_t bytes_read = psa_read(handle, 0, buffer, buffer_size);
if (bytes_read != expected_size) {
    return PSA_ERROR_COMMUNICATION_FAILURE;
}
```

### 암호화 문제

#### 1. 키 파생 실패

**오류: psa_key_derivation_output_bytes 실패: -135**
```
ERROR: psa_key_derivation_output_bytes 실패: -135
```
**원인**: HUK를 사용할 수 없거나 잘못된 파생 설정  
**해결책**:
```c
// PSA crypto 초기화 확인
psa_status_t status = psa_crypto_init();
if (status != PSA_SUCCESS && status != PSA_ERROR_ALREADY_EXISTS) {
    return status;
}

// HUK가 사용 가능한지 확인
// 플랫폼이 TFM_BUILTIN_KEY_ID_HUK를 지원하는지 확인

// 올바른 파생 알고리즘 사용
status = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
```

#### 2. 복호화 실패

**오류: PKCS7 패딩 검증 실패**
```
Invalid PKCS7 padding byte at position X
```
**디버깅**:
```c
// 암호화/복호화 키가 일치하는지 확인
// IV가 헤더에서 올바르게 추출되었는지 확인
// CBC 모드가 일관되게 사용되는지 확인

// DEV_MODE에서 패딩 디버그
#ifdef DEV_MODE
void debug_padding(const uint8_t* data, size_t size) {
    INFO_UNPRIV("마지막 16바이트: ");
    for (int i = 0; i < 16 && i < size; i++) {
        INFO_UNPRIV("%02x ", data[size - 16 + i]);
    }
    INFO_UNPRIV("\\n");
}
#endif
```

### 통신 문제

#### 1. 시리얼 출력 문제

**증상: 출력이 없거나 깨진 텍스트**
**해결책**:
```bash
# 보드레이트 확인 (115200이어야 함)
# 다른 시리얼 터미널 시도

# Linux/Mac
screen /dev/ttyACM0 115200
# 또는
minicom -D /dev/ttyACM0 -b 115200

# 디바이스 열거 확인
lsusb | grep Raspberry
```

#### 2. PSA 호출 타임아웃

**증상: 작업이 중단되거나 타임아웃**
**해결책**:
```c
// 클라이언트 호출에 타임아웃 구현
// 서비스가 메인 루프에서 응답하는지 확인

// 서비스에서 psa_reply가 항상 호출되는지 확인
switch (msg.type) {
    case MY_OPERATION:
        status = handle_operation(&msg);
        psa_reply(msg.handle, status);  // 항상 응답
        break;
    default:
        psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
        break;
}
```

## 디버깅 기법

### 1. DEV_MODE 활성화

```bash
# 디버그 기능으로 빌드
./build.sh DEV_MODE

# 이것은 다음을 활성화합니다:
# - 디버깅용 HUK 키 노출  
# - 추가 디버그 로깅
# - 키 파생 검증
```

### 2. 로깅 증가

```c
// 보안 파티션에서
#define DEBUG_VERBOSE
#ifdef DEBUG_VERBOSE
#define DEBUG_LOG(fmt, ...) INFO_UNPRIV("[DEBUG] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...)
#endif

void debug_function(void) {
    DEBUG_LOG("함수 진입\\n");
    DEBUG_LOG("매개변수: %d\\n", param);
    DEBUG_LOG("함수 종료: %d\\n", result);
}
```

### 3. 메모리 검사

```c
// 버퍼 내용 디버그
void debug_print_buffer(const char* name, const uint8_t* buf, size_t size) {
    INFO_UNPRIV("%s (%zu 바이트): ", name, size); // Use %zu for size_t
    for (size_t i = 0; i < size && i < 32; i++) { // Use size_t for loop variable
        INFO_UNPRIV("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) INFO_UNPRIV("\\n");
    }
    if (size > 32) INFO_UNPRIV("...");
    INFO_UNPRIV("\\n");
}
```

### 4. 오류 코드 분석

```c
// PSA 오류 코드 디코딩
const char* psa_error_to_string(psa_status_t status) {
    switch (status) {
        case PSA_SUCCESS: return "SUCCESS";
        case PSA_ERROR_GENERIC_ERROR: return "GENERIC_ERROR";
        case PSA_ERROR_NOT_SUPPORTED: return "NOT_SUPPORTED";
        case PSA_ERROR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case PSA_ERROR_INVALID_HANDLE: return "INVALID_HANDLE";
        case PSA_ERROR_BAD_STATE: return "BAD_STATE";
        case PSA_ERROR_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
        case PSA_ERROR_COMMUNICATION_FAILURE: return "COMMUNICATION_FAILURE";
        case PSA_ERROR_CONNECTION_REFUSED: return "CONNECTION_REFUSED";
        case PSA_ERROR_CONNECTION_BUSY: return "CONNECTION_BUSY";
        // Add more PSA error codes as needed
        default: return "UNKNOWN_PSA_ERROR"; // More specific default
    }
}

// 사용법
INFO_UNPRIV("작업에 걸린 시간: %lu ms\\n", end_time - start_time); // Use %lu for uint32_t
```

## 성능 디버깅

### 1. 타이밍 분석

```c
// 작업 타이밍 측정
uint32_t start_time = osKernelGetTickCount();
psa_status_t status = psa_call(handle, operation, ...);
uint32_t end_time = osKernelGetTickCount();

INFO_UNPRIV("작업에 걸린 시간: %lu ms\\n", end_time - start_time); // Use %lu for uint32_t
```

### 2. 메모리 사용량 추적

```c
// 파티션에서 메모리 할당 추적
static size_t total_allocated = 0;
static size_t peak_allocated = 0;

void* debug_malloc(size_t size) { // Corrected function signature
    void* ptr = malloc(size);
    if (ptr) {
        total_allocated += size;
        if (total_allocated > peak_allocated) {
            peak_allocated = total_allocated;
        }
        INFO_UNPRIV("할당된 바이트: %zu, 총: %zu, 최대: %zu\\n", // Use %zu for size_t
                   size, total_allocated, peak_allocated);
    }
    return ptr;
}

void debug_free(void* ptr, size_t size) { // Added size parameter for accurate tracking
    if (ptr) {
        free(ptr);
        total_allocated -= size;
        INFO_UNPRIV("해제된 바이트: %zu, 총: %zu\\n", size, total_allocated); // Use %zu for size_t
    }
}
```

### 3. 컨텍스트 스위치 분석

```c
// TrustZone 컨텍스트 스위치 모니터링
// Note: get_context_switch_count() is a placeholder for a platform-specific function.
// Implementation depends on the RTOS and hardware capabilities.
static uint32_t context_switch_count = 0; // This might be tracked by the RTOS

// ...existing code...
INFO_UNPRIV("PSA 호출에 %lu번의 컨텍스트 스위치 필요\\n", // Use %lu for uint32_t
           after_switches - before_switches);
```

## 빌드 환경 문제

### 1. 툴체인 문제

**오류: arm-none-eabi-gcc를 찾을 수 없음**
```bash
# GNU ARM 툴체인 설치
# Ubuntu/Debian:
sudo apt-get install gcc-arm-none-eabi

# macOS:
brew install arm-none-eabi-gcc

# 또는 ARM 웹사이트에서 다운로드
```

**오류: CMake 버전이 너무 오래됨**
```bash
# CMake 업데이트 (최소 3.21 필요)
pip3 install cmake --upgrade
```

### 2. Python 환경

**오류: 모듈을 찾을 수 없음**
```bash
# 필요한 Python 패키지 설치
pip3 install pycryptodome pyyaml

# 또는 requirements 파일이 있는 경우 사용
pip3 install -r requirements.txt
```

### 3. Git 서브모듈 문제

**오류: 서브모듈 디렉터리가 비어있음**
```bash
# 서브모듈 초기화 및 업데이트
git submodule update --init --recursive

# 또는 서브모듈과 함께 클론
git clone --recursive <repository_url>
```

## 하드웨어 디버깅

### 1. 디바이스 감지

```bash
# 디바이스가 감지되는지 확인
lsusb | grep "Raspberry Pi"

# 마운트 지점 확인
ls /media/*/RPI-RP2/ 2>/dev/null || echo "디바이스가 마운트되지 않음"

# 디바이스를 BOOTSEL 모드로 강제 진입
# USB 연결 중 BOOTSEL 버튼 누르고 있기
```

### 2. Picotool 문제

```bash
# picotool 설치
sudo apt-get install picotool

# 부트로더 모드에서 디바이스 확인
picotool info

# 디바이스가 감지되지 않는 경우:
sudo picotool erase
sudo picotool load file.uf2
```

### 3. 시리얼 연결 문제

```bash
# 디바이스 권한 확인
ls -l /dev/ttyACM*
# 필요한 경우 사용자를 dialout 그룹에 추가:
sudo usermod -a -G dialout $USER

# 시리얼 연결 테스트
echo "test" > /dev/ttyACM0
cat /dev/ttyACM0
```

## 복구 절차

### 1. 손상된 디바이스 복구

```bash
# BOOTSEL 모드로 강제 진입
# 1. USB 연결 해제
# 2. BOOTSEL 버튼 누르고 있기
# 3. 누른 상태로 USB 연결
# 4. BOOTSEL 버튼 놓기

# 모든 것 지우기
picotool erase

# 부트로더 다시 로드
picotool load build/spe/bin/bl2.uf2

# 메인 펌웨어 다시 로드
picotool load build/spe/bin/tfm_s_ns_signed.uf2

# 재부팅
picotool reboot
```

### 2. 빌드 복구

```bash
# 완전한 클린 재빌드
rm -rf build/
./build.sh clean

# SPE만 재빌드
rm -rf build/spe
cmake -S ./tflm_spe -B build/spe ...
cmake --build build/spe -- -j8 install

# NSPE만 재빌드
rm -rf build/nspe  
cmake -S ./tflm_ns -B build/nspe ...
cmake --build build/nspe -- -j8
```

### 3. Git 복구

```bash
# 깨끗한 상태로 리셋
git clean -fdx
git reset --hard HEAD

# 서브모듈 업데이트
git submodule update --init --recursive
```

## 도움 받기

### 1. 로그 수집

```bash
# 빌드 로그 수집
./build.sh 2>&1 | tee build.log

# 런타임 로그 수집
# 시리얼 터미널을 연결하고 출력 저장

# 시스템 정보 수집
uname -a
arm-none-eabi-gcc --version
cmake --version
python3 --version
```

### 2. 문제 보고

문제를 보고할 때 다음을 포함하세요:
- **빌드 로그**: 완전한 빌드 출력
- **런타임 로그**: 디바이스의 시리얼 출력
- **환경**: OS, 툴체인 버전
- **재현 단계**: 사용한 정확한 명령
- **예상 vs 실제 동작**

### 3. 일반적인 리소스

- **TF-M 문서**: https://tf-m-user-guide.trustedfirmware.org/
- **PSA API 사양**: https://developer.arm.com/architectures/security-architectures/platform-security-architecture
- **Raspberry Pi Pico 문서**: https://www.raspberrypi.org/documentation/microcontrollers/
- **프로젝트 저장소**: 문제 및 토론

## 고급 디버깅 기법

### 1. HEX 덤프 분석

```bash
# UF2 파일 내용 검사
hexdump -C build/spe/bin/tfm_s_ns_signed.uf2 | head -n 20 # Use -n for head

# 암호화된 모델 검사
hexdump -C models/encrypted_mnist_model_psa.bin | head -n 10 # Use -n for head
# 첫 4바이트는 "TMAX" (0x544D4158)이어야 함
```

### 2. 메모리 맵 분석

```bash
# ELF 파일 섹션 정보
arm-none-eabi-objdump -h build/nspe/bin/tfm_ns.axf

# 메모리 사용량 분석
arm-none-eabi-size build/nspe/bin/tfm_ns.axf

# 심볼 테이블 검사
arm-none-eabi-nm build/nspe/bin/tfm_ns.axf | grep -E "(psa_|tfm_)"
```

### 3. 런타임 메모리 덤프

```c
// 보안 파티션에서 메모리 덤프
void dump_memory_region(const char* name, const void* addr, size_t size)
{
    const uint8_t* bytes = (const uint8_t*)addr;

    INFO_UNPRIV("=== %s 메모리 덤프 (0x%08lx, %zu 바이트) ===\\n", // Use %lx for address, %zu for size_t
                name, (uintptr_t)addr, size); // Cast addr to uintptr_t for printing

    for (size_t i = 0; i < size; i += 16) {
        INFO_UNPRIV("%08lx: ", (uintptr_t)(addr + i)); // Cast addr to uintptr_t

        // 16진수 출력
        for (size_t j = 0; j < 16 && (i + j) < size; j++) {
            INFO_UNPRIV("%02x ", bytes[i + j]);
        }

        // ASCII 출력
        INFO_UNPRIV(" |");
        for (size_t j = 0; j < 16 && (i + j) < size; j++) {
            char c = bytes[i + j];
            INFO_UNPRIV("%c", (c >= 32 && c < 127) ? c : '.'); // Ensure printable ASCII
        }
        INFO_UNPRIV("|\\n");
    }
}

// 사용 예
dump_memory_region("Model Data", g_model_buffer, 256);
```

### 4. 성능 프로파일링

```c
// 상세한 성능 측정
typedef struct {
    const char* name;
    uint32_t start_time;
    uint64_t total_time; // Use uint64_t for total_time to prevent overflow
    uint32_t call_count;
    uint32_t min_time;
    uint32_t max_time;
} perf_counter_t;

#define MAX_PERF_COUNTERS 10 // Define MAX_PERF_COUNTERS
static perf_counter_t g_perf_counters[MAX_PERF_COUNTERS];
static uint8_t g_perf_counter_idx = 0; // Keep track of next available counter

void perf_init(void) { // Initialize counters
    memset(g_perf_counters, 0, sizeof(g_perf_counters));
    g_perf_counter_idx = 0;
}

void perf_start(const char* name) {
    for (int i = 0; i < g_perf_counter_idx; i++) { // Search existing counters
        if (g_perf_counters[i].name && strcmp(g_perf_counters[i].name, name) == 0) {
            g_perf_counters[i].start_time = osKernelGetTickCount();
            return;
        }
    }
    // Add new counter if not found and space available
    if (g_perf_counter_idx < MAX_PERF_COUNTERS) {
        g_perf_counters[g_perf_counter_idx].name = name;
        g_perf_counters[g_perf_counter_idx].start_time = osKernelGetTickCount();
        g_perf_counters[g_perf_counter_idx].min_time = UINT32_MAX; // Initialize min_time
        g_perf_counter_idx++;
    }
}

void perf_end(const char* name) {
    uint32_t end_time = osKernelGetTickCount();
    
    for (int i = 0; i < g_perf_counter_idx; i++) {
        if (g_perf_counters[i].name && strcmp(g_perf_counters[i].name, name) == 0) {
            uint32_t elapsed = end_time - g_perf_counters[i].start_time;

            g_perf_counters[i].total_time += elapsed;
            g_perf_counters[i].call_count++;
            
            if (g_perf_counters[i].min_time == 0 || elapsed < g_perf_counters[i].min_time) {
                g_perf_counters[i].min_time = elapsed;
            }
            
            if (elapsed > g_perf_counters[i].max_time) {
                g_perf_counters[i].max_time = elapsed;
            }
            break;
        }
    }
}

void perf_report(void) {
    INFO_UNPRIV("=== 성능 보고서 ===\\n");
    for (int i = 0; i < g_perf_counter_idx; i++) { // Iterate up to g_perf_counter_idx
        if (g_perf_counters[i].name && g_perf_counters[i].call_count > 0) {
            uint32_t avg = g_perf_counters[i].total_time / g_perf_counters[i].call_count;
            INFO_UNPRIV("%s: 호출 %lu회, 평균 %lu ms, 최소 %lu ms, 최대 %lu ms\\n", // Use %lu
                       g_perf_counters[i].name,
                       g_perf_counters[i].call_count, // This is uint32_t
                       avg, // This is uint32_t
                       g_perf_counters[i].min_time, // This is uint32_t
                       g_perf_counters[i].max_time); // This is uint32_t
        }
    }
}

// 사용 예
// perf_init(); // Call before starting any profiling
perf_start("inference");
tm_run(&g_model, input, output);
perf_end("inference");
```

## 다음 단계

문제를 해결한 후:
- **[TF-M 아키텍처](./tfm-architecture.md)**: 시스템을 더 잘 이해하기
- **[보안 파티션](./secure-partitions.md)**: 견고한 서비스 구현  
- **[테스트 프레임워크](./testing-framework.md)**: 포괄적인 테스트 추가