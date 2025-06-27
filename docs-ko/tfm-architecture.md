# TF-M 아키텍처 가이드

## 개요

TF-M (Trusted Firmware-M)은 ARMv8-M 아키텍처와 TrustZone 기술을 기반으로 하는 보안 프레임워크입니다. 이 프로젝트는 Raspberry Pi Pico 2W (RP2350)에서 TF-M을 사용하여 보안 머신러닝 추론을 구현합니다.

## ARM TrustZone 아키텍처

### 이중 세계 모델
```
┌─────────────────────────┬─────────────────────────┐
│     보안 세계 (SPE)      │    비보안 세계 (NSPE)    │
│   Secure Processing     │  Non-Secure Processing  │
│     Environment         │      Environment        │
├─────────────────────────┼─────────────────────────┤
│ • TF-M Core             │ • 애플리케이션 코드       │
│ • 보안 파티션            │ • RTOS (FreeRTOS)       │
│ • PSA 서비스             │ • 테스트 프레임워크       │
│ • TinyMaix 추론         │ • PSA 클라이언트 API     │
│ • 암호화 서비스          │                         │
└─────────────────────────┴─────────────────────────┘
```

### 메모리 격리
```
Flash Memory Layout (RP2350):
┌──────────────────────────────────────┐ 0x10200000
│               Flash End              │
├──────────────────────────────────────┤
│         Non-Secure Code              │
│         (NSPE Applications)          │
├──────────────────────────────────────┤ 0x10080000
│         Secure Code                  │
│         (TF-M + Partitions)         │
├──────────────────────────────────────┤ 0x10011000
│         Bootloader (MCUBoot)         │
├──────────────────────────────────────┤ 0x10000000
│         Flash Start                  │
└──────────────────────────────────────┘

RAM Memory Layout:
┌──────────────────────────────────────┐ 0x20042000
│               RAM End                │
├──────────────────────────────────────┤
│         Non-Secure RAM               │
│         (Application Data)           │
├──────────────────────────────────────┤ 0x20020000
│         Secure RAM                   │
│         (TF-M Stack/Heap)           │
├──────────────────────────────────────┤ 0x20000000
│         RAM Start                    │
└──────────────────────────────────────┘
```

## TF-M 컴포넌트 구조

### 핵심 컴포넌트
```
TF-M Core Architecture:
┌─────────────────────────────────────────────────────┐
│                   SPE (Secure)                      │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │    PSA      │  │  Partition  │  │   Service   │  │
│  │   Router    │  │   Manager   │  │   Manager   │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │    Echo     │  │  TinyMaix   │  │    Crypto   │  │
│  │   Service   │  │  Inference  │  │   Service   │  │
│  │  (PID 444)  │  │  (PID 445)  │  │  (Built-in) │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────┤
│                Hardware Abstraction                 │
└─────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────┐
│                  NSPE (Non-Secure)                  │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │   Test      │  │  Application│  │    PSA      │  │
│  │   Runner    │  │   Broker    │  │  Client API │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────┤
│                   FreeRTOS RTOS                     │
└─────────────────────────────────────────────────────┘
```

## PSA (Platform Security Architecture)

### PSA 서비스 모델
```c
// PSA 클라이언트-서버 통신 모델
// 1. 연결 설정
psa_handle_t handle = psa_connect(SERVICE_SID, 1);

// 2. 서비스 호출
psa_status_t status = psa_call(handle, operation_type, 
                              in_vec, num_invec,
                              out_vec, num_outvec);

// 3. 연결 종료
psa_close(handle);
```

### 서비스 식별자 할당
```yaml
# 프로젝트 서비스 ID 매핑
Service Name        SID          PID   Type
─────────────────────────────────────────────
Echo Service       0x00000105    444   Connection-based
TinyMaix Inference 0x00000107    445   Connection-based
PSA Crypto         Built-in      -     API-based
PSA Storage        Built-in      -     API-based
```

## 부트 시퀀스

### 시스템 부팅 과정
```
1. MCUBoot (Bootloader)
   ├── 하드웨어 초기화
   ├── 보안 펌웨어 검증
   └── TF-M 로드 및 실행

2. TF-M Secure Boot
   ├── TrustZone 구성
   ├── 보안 파티션 초기화
   ├── PSA 서비스 등록
   └── Non-Secure 세계로 점프

3. Non-Secure Boot
   ├── FreeRTOS 초기화
   ├── PSA 클라이언트 설정
   ├── 애플리케이션 시작
   └── 테스트 실행
```

### 초기화 순서도
```
MCUBoot → TF-M Core → Secure Partitions → NSPE Applications
    ↓         ↓            ↓                    ↓
  HW Init   PSA Init   Service Init        App Init
  Image     TrustZone  Echo Service        Test Runner
  Verify    Setup      TinyMaix Init       PSA Clients
```

## 보안 파티션 아키텍처

### 파티션 유형과 역할
```c
// APPLICATION-ROT (Application Root of Trust)
// - 사용자 정의 보안 서비스
// - 연결 기반 PSA 서비스 모델
// - 독립적인 메모리 공간

typedef struct partition_metadata {
    uint32_t partition_id;           // 고유 파티션 ID
    uint32_t service_count;          // 제공하는 서비스 수
    psa_signal_t signal_mask;        // 처리 가능한 신호
    uint32_t stack_size;             // 스택 크기
    bool connection_based;           // 연결 기반 여부
} partition_metadata_t;
```

### Echo 서비스 구조
```c
// Echo 서비스 - 기본 PSA 통신 예제
#define ECHO_SERVICE_SID    0x00000105U
#define ECHO_SERVICE_PID    444

void echo_service_main(void)
{
    psa_msg_t msg;
    
    while (1) {
        // PSA 메시지 수신 대기
        psa_status_t status = psa_get(PSA_WAIT_ANY, &msg);
        
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case PSA_IPC_CALL:
                handle_echo_request(&msg);
                break;
                
            case PSA_IPC_DISCONNECT:
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
        }
    }
}
```

### TinyMaix 추론 서비스 구조
```c
// TinyMaix 서비스 - ML 추론 보안 구현
#define TINYMAIX_SERVICE_SID    0x00000107U
#define TINYMAIX_SERVICE_PID    445

// 서비스 메시지 타입
#define TINYMAIX_IPC_LOAD_MODEL     (0x1001U)
#define TINYMAIX_IPC_SET_INPUT      (0x1002U)
#define TINYMAIX_IPC_RUN_INFERENCE  (0x1003U)

#ifdef DEV_MODE
#define TINYMAIX_IPC_GET_MODEL_KEY  (0x1004U)
#endif

// 모델 상태 관리
static bool g_model_loaded = false;
static tm_mdl_t g_model;
static uint8_t g_model_buffer[MODEL_BUFFER_SIZE];
```

## 암호화 서비스

### HUK (Hardware Unique Key) 기반 키 파생
```c
// HUK에서 모델 암호화 키 파생
psa_status_t derive_key_from_huk(const char* label, 
                                uint8_t* derived_key, 
                                size_t key_length)
{
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t status;
    
    // HKDF-SHA256으로 키 파생 설정
    status = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (status != PSA_SUCCESS) return status;
    
    // HUK를 입력 키로 사용
    status = psa_key_derivation_input_key(&op, PSA_KEY_DERIVATION_INPUT_SECRET,
                                         TFM_BUILTIN_KEY_ID_HUK);
    if (status != PSA_SUCCESS) goto cleanup;
    
    // 라벨을 Salt로 사용
    status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO,
                                           (const uint8_t*)label, strlen(label));
    if (status != PSA_SUCCESS) goto cleanup;
    
    // 파생 키 생성
    status = psa_key_derivation_output_bytes(&op, derived_key, key_length);
    
cleanup:
    psa_key_derivation_abort(&op);
    return status;
}
```

### AES-CBC 모델 복호화
```c
// 암호화된 모델 복호화 (보안 파티션 내부)
psa_status_t decrypt_model_cbc(const uint8_t* encrypted_data,
                              size_t encrypted_size,
                              const uint8_t* key,
                              uint8_t* decrypted_data,
                              size_t* decrypted_size)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;
    psa_status_t status;
    
    // AES-128 키 설정
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    
    // 키 가져오기
    status = psa_import_key(&attributes, key, 16, &key_id);
    if (status != PSA_SUCCESS) return status;
    
    // CBC 복호화 수행
    status = psa_cipher_decrypt(key_id, PSA_ALG_CBC_NO_PADDING,
                               encrypted_data, encrypted_size,
                               decrypted_data, *decrypted_size, decrypted_size);
    
    // 키 정리
    psa_destroy_key(key_id);
    return status;
}
```

## DEV_MODE vs 프로덕션 모드

### 컴파일 시점 구분
```c
// DEV_MODE에서만 활성화되는 디버그 기능
#ifdef DEV_MODE
    // HUK 키 노출 - 개발 시에만 사용
    case TINYMAIX_IPC_GET_MODEL_KEY:
        status = handle_get_model_key(&msg);
        break;
        
    // 추가 디버그 로깅
    #define DEBUG_LOG(fmt, ...) INFO_UNPRIV(\"[DEBUG] \" fmt, ##__VA_ARGS__)
#else
    // 프로덕션에서는 디버그 기능 비활성화
    #define DEBUG_LOG(fmt, ...)
#endif
```

### 테스트 실행 모드
```c
// 빌드 모드에 따른 테스트 분기
void run_all_tests(void* arg)
{
#ifdef DEV_MODE
    LOG_MSG(\"DEV_MODE: Only HUK key debugging test\\r\\n\");
    test_tinymaix_get_model_key();  // HUK 키 추출만 실행
#else
    LOG_MSG(\"Production Mode: All tests\\r\\n\");
    test_echo_service();            // 모든 표준 테스트 실행
    test_psa_encryption();
    test_psa_hash();
    test_tinymaix_inference();
#endif
}
```

## 성능 고려사항

### TrustZone 컨텍스트 스위칭
```c
// PSA 호출 시 발생하는 보안/비보안 세계 전환
// 오버헤드를 최소화하기 위한 배치 처리
typedef struct batch_request {
    uint32_t operation_count;
    psa_invec_t input_vectors[MAX_BATCH_SIZE];
    psa_outvec_t output_vectors[MAX_BATCH_SIZE];
} batch_request_t;

// 여러 연산을 하나의 PSA 호출로 배치 처리
psa_status_t batch_inference_request(batch_request_t* batch);
```

### 메모리 최적화
```yaml
# 파티션별 메모리 할당 최적화
Echo Service:
  stack_size: \"0x1000\"     # 4KB - 간단한 문자열 처리
  
TinyMaix Inference:
  stack_size: \"0x2000\"     # 8KB - ML 모델 추론
  heap_size: \"0x4000\"      # 16KB - 모델 버퍼
```

## 디버깅 및 모니터링

### 보안 파티션 로깅
```c
// TF-M 로깅 매크로 사용
#include \"tfm_log.h\"

void debug_partition_state(void)
{
    INFO_UNPRIV(\"Partition ID: %d\\n\", PARTITION_ID);
    INFO_UNPRIV(\"Service count: %d\\n\", service_count);
    INFO_UNPRIV(\"Active connections: %d\\n\", active_connections);
    
#ifdef DEV_MODE
    // 민감한 정보는 DEV_MODE에서만 출력
    INFO_UNPRIV(\"Memory usage: %d bytes\\n\", get_memory_usage());
#endif
}
```

### 성능 프로파일링
```c
// 추론 성능 측정
uint32_t start_tick = osKernelGetTickCount();
tm_stat_t inference_result = tm_run(&model, input_data, output_data);
uint32_t end_tick = osKernelGetTickCount();

INFO_UNPRIV(\"Inference time: %d ms\\n\", end_tick - start_tick);
INFO_UNPRIV(\"Memory peak: %d bytes\\n\", tm_get_memory_peak());
```

## 다음 단계

TF-M 아키텍처를 이해했다면 다음 문서를 참조하세요:

- **[보안 파티션 개발](./secure-partitions.md)** - 커스텀 보안 서비스 구현
- **[PSA API](./psa-api.md)** - 클라이언트-서버 통신 패턴
- **[빌드 시스템](./build-system.md)** - 프로젝트 빌드 및 배포
- **[문제 해결](./troubleshooting.md)** - 일반적인 문제와 해결 방법