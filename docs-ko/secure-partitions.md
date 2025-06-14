# 보안 파티션 개발 가이드

## 개요

보안 파티션(Secure Partition)은 TF-M에서 PSA 서비스를 제공하는 독립적인 보안 컴포넌트입니다. 각 파티션은 격리된 메모리 공간에서 실행되며, 표준화된 PSA API를 통해 비보안 세계와 통신합니다.

## 파티션 아키텍처

### 파티션 유형
```
TF-M Partition Types:
┌─────────────────────────────────────┐
│           PSA-ROT                   │  ← 플랫폼 Root of Trust
│  • PSA Crypto                       │
│  • PSA Storage                      │  
│  • Platform Services                │
├─────────────────────────────────────┤
│        APPLICATION-ROT              │  ← 애플리케이션 Root of Trust  
│  • Echo Service (PID 444)           │
│  • TinyMaix Inference (PID 445)     │
│  • Custom Services                  │
└─────────────────────────────────────┘
```

### 메모리 격리
```
각 파티션의 독립적 메모리 공간:
┌─────────────────────────────────────┐
│        Partition N Stack            │
├─────────────────────────────────────┤
│        Partition N Heap             │
├─────────────────────────────────────┤
│        Partition N Code             │
├─────────────────────────────────────┤
│           PSA Router                │
├─────────────────────────────────────┤
│        Partition 1 Code             │
├─────────────────────────────────────┤
│        Partition 1 Heap             │
├─────────────────────────────────────┤
│        Partition 1 Stack            │
└─────────────────────────────────────┘
```

## Echo 서비스 구현 분석

### 매니페스트 파일: `echo_service_manifest.yaml`
```yaml
{
  "description": "TF-M Echo Service",
  "psa_framework_version": "1.1",
  "name": "TFM_SP_ECHO_SERVICE",
  "type": "APPLICATION-ROT",
  "priority": "NORMAL",
  "id": "0x00000105",
  "entry_point": "tfm_echo_service_main",
  "stack_size": "0x1000",
  "services": [
    {
      "name": "TFM_ECHO_SERVICE",
      "sid": "0x00000105",
      "connection_based": true,
      "non_secure_clients": true,
      "version": 1,
      "version_policy": "STRICT"
    }
  ],
  "mmio_regions": [],
  "irqs": [],
  "linker_pattern": {
    "library_list": []
  }
}
```

### 서비스 구현: `echo_service.c`
```c
#include "tfm_sp_log.h"
#include "psa/service.h"
#include "psa_manifest/tfm_echo_service.h"

/* Echo 서비스 메인 진입점 */
void tfm_echo_service_main(void)
{
    psa_msg_t msg;
    
    while (1) {
        /* PSA 메시지 대기 - 블로킹 호출 */
        psa_status_t status = psa_get(PSA_WAIT_ANY, &msg);
        if (status != PSA_SUCCESS) {
            continue;
        }
        
        /* 메시지 타입에 따른 처리 분기 */
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                /* 클라이언트 연결 요청 처리 */
                LOG_INFSIG("Echo Service: Client connected\\r\\n");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case PSA_IPC_CALL:
                /* 실제 에코 기능 수행 */
                handle_echo_request(&msg);
                break;
                
            case PSA_IPC_DISCONNECT:
                /* 클라이언트 연결 해제 */
                LOG_INFSIG("Echo Service: Client disconnected\\r\\n");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            default:
                /* 지원하지 않는 메시지 타입 */
                LOG_ERRSIG("Echo Service: Invalid message type: %d\\r\\n", msg.type);
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

/* Echo 요청 처리 함수 */
static void handle_echo_request(psa_msg_t *msg)
{
    uint8_t input_buffer[ECHO_BUFFER_SIZE];
    uint8_t output_buffer[ECHO_BUFFER_SIZE];
    size_t bytes_read;
    
    /* 입력 데이터 읽기 */
    bytes_read = psa_read(msg->handle, 0, input_buffer, sizeof(input_buffer));
    if (bytes_read <= 0) {
        LOG_ERRSIG("Echo Service: Failed to read input\\r\\n");
        psa_reply(msg->handle, PSA_ERROR_COMMUNICATION_FAILURE);
        return;
    }
    
    /* 입력을 출력으로 복사 (에코 동작) */
    memcpy(output_buffer, input_buffer, bytes_read);
    
    /* 결과 전송 */
    psa_write(msg->handle, 0, output_buffer, bytes_read);
    
    /* 요청 완료 응답 */
    psa_reply(msg->handle, PSA_SUCCESS);
    
    LOG_INFSIG("Echo Service: Processed %d bytes\\r\\n", bytes_read);
}
```

### CMake 빌드 구성: `CMakeLists.txt`
```cmake
# Echo 서비스 빌드 구성
if (NOT TFM_PARTITION_ECHO_SERVICE)
    return()
endif()

# 파티션 타겟 생성
add_library(tfm_app_rot_partition_echo_service STATIC)

# 소스 파일 지정
target_sources(tfm_app_rot_partition_echo_service
    PRIVATE
        echo_service.c
)

# 헤더 파일 경로
target_include_directories(tfm_app_rot_partition_echo_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/interface/include
)

# 링크 라이브러리
target_link_libraries(tfm_app_rot_partition_echo_service
    PRIVATE
        tfm_secure_api
        tfm_sprt
        platform_s
)

# 컴파일 정의
target_compile_definitions(tfm_app_rot_partition_echo_service
    PRIVATE
        TFM_PARTITION_ECHO_SERVICE
)
```

## TinyMaix 추론 서비스 구현

### 고급 매니페스트: `tinymaix_inference_manifest.yaml`
```yaml
{
  "description": "TF-M TinyMaix Inference Service",
  "psa_framework_version": "1.1", 
  "name": "TFM_SP_TINYMAIX_INFERENCE",
  "type": "APPLICATION-ROT",
  "priority": "NORMAL",
  "id": "0x00000107",
  "entry_point": "tfm_tinymaix_inference_main",
  "stack_size": "0x2000",  # 8KB - ML 연산용 큰 스택
  "services": [
    {
      "name": "TFM_TINYMAIX_INFERENCE_SERVICE",
      "sid": "0x00000107",
      "connection_based": true,
      "non_secure_clients": true,
      "version": 1,
      "version_policy": "STRICT"
    }
  ],
  "mmio_regions": [],
  "irqs": [],
  "linker_pattern": {
    "library_list": [
      "*tinymaix*"  # TinyMaix 라이브러리 링크
    ]
  }
}
```

### 고급 서비스 구현: `tinymaix_inference.c`
```c
#include "tfm_sp_log.h"
#include "psa/service.h"
#include "psa_manifest/tfm_tinymaix_inference.h"
#include "tinymaix.h"
#include "encrypted_mnist_model_psa.h"

/* 서비스 메시지 타입 정의 */
#define TINYMAIX_IPC_LOAD_MODEL     (0x1001U)
#define TINYMAIX_IPC_SET_INPUT      (0x1002U)  
#define TINYMAIX_IPC_RUN_INFERENCE  (0x1003U)
#define TINYMAIX_IPC_GET_OUTPUT     (0x1004U)

#ifdef DEV_MODE
#define TINYMAIX_IPC_GET_MODEL_KEY  (0x1005U)
#endif

/* 전역 상태 변수 */
static bool g_model_loaded = false;
static tm_mdl_t g_model;
static uint8_t g_model_buffer[MODEL_BUFFER_SIZE];
static uint8_t g_input_buffer[INPUT_BUFFER_SIZE];
static uint8_t g_output_buffer[OUTPUT_BUFFER_SIZE];

/* TinyMaix 서비스 메인 진입점 */
void tfm_tinymaix_inference_main(void)
{
    psa_msg_t msg;
    
    LOG_INFSIG("TinyMaix Inference Service Started\\r\\n");
    
    while (1) {
        psa_status_t status = psa_get(PSA_WAIT_ANY, &msg);
        if (status != PSA_SUCCESS) {
            continue;
        }
        
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                LOG_INFSIG("TinyMaix: Client connected\\r\\n");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case PSA_IPC_CALL:
                handle_tinymaix_request(&msg);
                break;
                
            case PSA_IPC_DISCONNECT:
                LOG_INFSIG("TinyMaix: Client disconnected\\r\\n");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            default:
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

/* TinyMaix 요청 처리 */
static void handle_tinymaix_request(psa_msg_t *msg)
{
    uint32_t operation = msg->rhandle;  /* 연산 타입 */
    psa_status_t status = PSA_SUCCESS;
    
    switch (operation) {
        case TINYMAIX_IPC_LOAD_MODEL:
            status = handle_load_model(msg);
            break;
            
        case TINYMAIX_IPC_SET_INPUT:
            status = handle_set_input(msg);
            break;
            
        case TINYMAIX_IPC_RUN_INFERENCE:
            status = handle_run_inference(msg);
            break;
            
        case TINYMAIX_IPC_GET_OUTPUT:
            status = handle_get_output(msg);
            break;
            
#ifdef DEV_MODE
        case TINYMAIX_IPC_GET_MODEL_KEY:
            status = handle_get_model_key(msg);
            break;
#endif
            
        default:
            LOG_ERRSIG("TinyMaix: Unknown operation: %d\\r\\n", operation);
            status = PSA_ERROR_NOT_SUPPORTED;
            break;
    }
    
    psa_reply(msg.handle, status);
}

/* 암호화된 모델 로드 및 복호화 */
static psa_status_t handle_load_model(psa_msg_t *msg)
{
    psa_status_t status;
    uint8_t derived_key[DERIVED_KEY_LEN];
    uint8_t decrypted_model[MODEL_BUFFER_SIZE];
    size_t decrypted_size;
    
    LOG_INFSIG("TinyMaix: Loading encrypted model...\\r\\n");
    
    /* HUK에서 모델 복호화 키 파생 */
    const char *huk_label = "pico2w-tinymaix-model-aes128-v1.0";
    status = derive_key_from_huk(huk_label, derived_key, sizeof(derived_key));
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("TinyMaix: Key derivation failed: %d\\r\\n", status);
        return status;
    }
    
    /* 내장된 암호화 모델 복호화 */
    status = decrypt_model_cbc(encrypted_mnist_model_psa_data,
                              encrypted_mnist_model_psa_size,
                              derived_key,
                              decrypted_model,
                              &decrypted_size);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("TinyMaix: Model decryption failed: %d\\r\\n", status);
        return status;
    }
    
    /* TinyMaix 모델 로드 */
    tm_stat_t tm_status = tm_load(&g_model, decrypted_model, 
                                 g_model_buffer, sizeof(g_model_buffer));
    if (tm_status != TM_OK) {
        LOG_ERRSIG("TinyMaix: Model load failed: %d\\r\\n", tm_status);
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    g_model_loaded = true;
    LOG_INFSIG("TinyMaix: Model loaded successfully\\r\\n");
    
    return PSA_SUCCESS;
}

/* 입력 데이터 설정 */
static psa_status_t handle_set_input(psa_msg_t *msg)
{
    if (!g_model_loaded) {
        LOG_ERRSIG("TinyMaix: Model not loaded\\r\\n");
        return PSA_ERROR_BAD_STATE;
    }
    
    /* 클라이언트에서 입력 데이터 읽기 */
    size_t bytes_read = psa_read(msg->handle, 0, g_input_buffer, 
                                sizeof(g_input_buffer));
    if (bytes_read != INPUT_SIZE) {
        LOG_ERRSIG("TinyMaix: Invalid input size: %d\\r\\n", bytes_read);
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    LOG_INFSIG("TinyMaix: Input data set (%d bytes)\\r\\n", bytes_read);
    return PSA_SUCCESS;
}

/* 추론 실행 */
static psa_status_t handle_run_inference(psa_msg_t *msg)
{
    if (!g_model_loaded) {
        return PSA_ERROR_BAD_STATE;
    }
    
    LOG_INFSIG("TinyMaix: Running inference...\\r\\n");
    
    /* TinyMaix 추론 실행 */
    tm_stat_t tm_status = tm_run(&g_model, g_input_buffer, g_output_buffer);
    if (tm_status != TM_OK) {
        LOG_ERRSIG("TinyMaix: Inference failed: %d\\r\\n", tm_status);
        return PSA_ERROR_GENERIC_ERROR;
    }
    
    LOG_INFSIG("TinyMaix: Inference completed\\r\\n");
    return PSA_SUCCESS;
}

/* 출력 데이터 반환 */
static psa_status_t handle_get_output(psa_msg_t *msg)
{
    if (!g_model_loaded) {
        return PSA_ERROR_BAD_STATE;
    }
    
    /* 출력 데이터를 클라이언트에게 전송 */
    psa_write(msg->handle, 0, g_output_buffer, OUTPUT_SIZE);
    
    LOG_INFSIG("TinyMaix: Output data sent\\r\\n");
    return PSA_SUCCESS;
}

#ifdef DEV_MODE
/* DEV_MODE: HUK 파생 키 추출 (디버깅용) */
static psa_status_t handle_get_model_key(psa_msg_t *msg)
{
    uint8_t derived_key[DERIVED_KEY_LEN];
    psa_status_t status;
    
    LOG_INFSIG("TinyMaix: Getting HUK-derived model key (DEV_MODE)\\r\\n");
    
    /* HUK에서 키 파생 */
    const char *huk_label = "pico2w-tinymaix-model-aes128-v1.0";
    status = derive_key_from_huk(huk_label, derived_key, sizeof(derived_key));
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("TinyMaix: Key derivation failed: %d\\r\\n", status);
        return status;
    }
    
    /* 키를 클라이언트에게 전송 */
    psa_write(msg->handle, 0, derived_key, sizeof(derived_key));
    
    /* 디버그 출력 - HUK 키 16진수 표시 */
    LOG_INFSIG("HUK-derived key: ");
    for (int i = 0; i < sizeof(derived_key); i++) {
        LOG_INFSIG("%02x", derived_key[i]);
    }
    LOG_INFSIG("\\r\\n");
    
    return PSA_SUCCESS;
}
#endif
```

## 새로운 파티션 생성 가이드

### 1단계: 디렉터리 구조 생성
```bash
# 새 파티션을 위한 디렉터리 생성
mkdir -p partitions/my_custom_service

# 필요한 파일들 생성
touch partitions/my_custom_service/my_custom_service.c
touch partitions/my_custom_service/my_custom_service_manifest.yaml
touch partitions/my_custom_service/CMakeLists.txt
```

### 2단계: 매니페스트 파일 작성
```yaml
# partitions/my_custom_service/my_custom_service_manifest.yaml
{
  "description": "My Custom Secure Service",
  "psa_framework_version": "1.1",
  "name": "TFM_SP_MY_CUSTOM_SERVICE", 
  "type": "APPLICATION-ROT",
  "priority": "NORMAL",
  "id": "0x00000108",  # 고유한 파티션 ID
  "entry_point": "tfm_my_custom_service_main",
  "stack_size": "0x1000",
  "services": [
    {
      "name": "TFM_MY_CUSTOM_SERVICE",
      "sid": "0x00000108",  # 고유한 서비스 ID
      "connection_based": true,
      "non_secure_clients": true,
      "version": 1,
      "version_policy": "STRICT"
    }
  ],
  "mmio_regions": [],
  "irqs": [],
  "linker_pattern": {
    "library_list": []
  }
}
```

### 3단계: 서비스 구현
```c
// partitions/my_custom_service/my_custom_service.c
#include "tfm_sp_log.h"
#include "psa/service.h"
#include "psa_manifest/tfm_my_custom_service.h"

/* 커스텀 서비스 메시지 타입 */
#define MY_SERVICE_IPC_OPERATION_1  (0x2001U)
#define MY_SERVICE_IPC_OPERATION_2  (0x2002U)

/* 서비스 상태 */
static bool g_service_initialized = false;

/* 메인 서비스 루프 */
void tfm_my_custom_service_main(void)
{
    psa_msg_t msg;
    
    LOG_INFSIG("My Custom Service Started\\r\\n");
    
    /* 서비스 초기화 */
    initialize_custom_service();
    
    while (1) {
        psa_status_t status = psa_get(PSA_WAIT_ANY, &msg);
        if (status != PSA_SUCCESS) {
            continue;
        }
        
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case PSA_IPC_CALL:
                handle_custom_request(&msg);
                break;
                
            case PSA_IPC_DISCONNECT:
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            default:
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

/* 커스텀 요청 처리 */
static void handle_custom_request(psa_msg_t *msg)
{
    uint32_t operation = msg->rhandle;
    psa_status_t status = PSA_SUCCESS;
    
    switch (operation) {
        case MY_SERVICE_IPC_OPERATION_1:
            status = handle_operation_1(msg);
            break;
            
        case MY_SERVICE_IPC_OPERATION_2:
            status = handle_operation_2(msg);
            break;
            
        default:
            status = PSA_ERROR_NOT_SUPPORTED;
            break;
    }
    
    psa_reply(msg.handle, status);
}

/* 서비스 초기화 */
static void initialize_custom_service(void)
{
    /* 서비스별 초기화 로직 */
    g_service_initialized = true;
    LOG_INFSIG("Custom service initialized\\r\\n");
}

/* 연산 1 처리 */
static psa_status_t handle_operation_1(psa_msg_t *msg)
{
    /* 구체적인 비즈니스 로직 구현 */
    LOG_INFSIG("Handling operation 1\\r\\n");
    return PSA_SUCCESS;
}

/* 연산 2 처리 */
static psa_status_t handle_operation_2(psa_msg_t *msg)
{
    /* 구체적인 비즈니스 로직 구현 */
    LOG_INFSIG("Handling operation 2\\r\\n");
    return PSA_SUCCESS;
}
```

### 4단계: CMake 구성
```cmake
# partitions/my_custom_service/CMakeLists.txt
if (NOT TFM_PARTITION_MY_CUSTOM_SERVICE)
    return()
endif()

add_library(tfm_app_rot_partition_my_custom_service STATIC)

target_sources(tfm_app_rot_partition_my_custom_service
    PRIVATE
        my_custom_service.c
)

target_include_directories(tfm_app_rot_partition_my_custom_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/interface/include
)

target_link_libraries(tfm_app_rot_partition_my_custom_service
    PRIVATE
        tfm_secure_api
        tfm_sprt
        platform_s
)

target_compile_definitions(tfm_app_rot_partition_my_custom_service
    PRIVATE
        TFM_PARTITION_MY_CUSTOM_SERVICE
)
```

### 5단계: 매니페스트 등록
```json
// partitions/manifest_list.yaml에 추가
{
  "description": "My Custom Service",
  "manifest": "partitions/my_custom_service/my_custom_service_manifest.yaml",
  "output_path": "partitions/my_custom_service",
  "conditional": "TFM_PARTITION_MY_CUSTOM_SERVICE",
  "version_major": 0,
  "version_minor": 1,
  "pid": 446  // 고유한 PID
}
```

### 6단계: 빌드 시스템에 추가
```cmake
# spe/config/config_tflm.cmake에 추가
set(TFM_PARTITION_MY_CUSTOM_SERVICE  ON  CACHE BOOL  "Enable My Custom Service")
```

## 클라이언트 API 개발

### 헤더 파일 생성
```c
// interface/include/tfm_my_custom_service_defs.h
#ifndef TFM_MY_CUSTOM_SERVICE_DEFS_H
#define TFM_MY_CUSTOM_SERVICE_DEFS_H

#include <stdint.h>
#include <stddef.h>

/* 서비스 ID */
#define TFM_MY_CUSTOM_SERVICE_SID   0x00000108U

/* 반환 상태 */
typedef enum {
    MY_CUSTOM_STATUS_SUCCESS = 0,
    MY_CUSTOM_STATUS_ERROR   = -1,
    MY_CUSTOM_STATUS_INVALID = -2
} tfm_my_custom_status_t;

/* API 함수 선언 */
tfm_my_custom_status_t tfm_my_custom_operation_1(const uint8_t* input, 
                                                 size_t input_size,
                                                 uint8_t* output,
                                                 size_t output_size);

tfm_my_custom_status_t tfm_my_custom_operation_2(uint32_t parameter);

#endif /* TFM_MY_CUSTOM_SERVICE_DEFS_H */
```

### 클라이언트 API 구현
```c
// interface/src/tfm_my_custom_service_api.c
#include "tfm_my_custom_service_defs.h"
#include "psa/client.h"

#define MY_SERVICE_IPC_OPERATION_1  (0x2001U)
#define MY_SERVICE_IPC_OPERATION_2  (0x2002U)

tfm_my_custom_status_t tfm_my_custom_operation_1(const uint8_t* input,
                                                 size_t input_size,
                                                 uint8_t* output,
                                                 size_t output_size)
{
    psa_handle_t handle;
    psa_status_t status;
    
    /* 서비스 연결 */
    handle = psa_connect(TFM_MY_CUSTOM_SERVICE_SID, 1);
    if (handle <= 0) {
        return MY_CUSTOM_STATUS_ERROR;
    }
    
    /* 입력/출력 벡터 설정 */
    psa_invec_t in_vec[] = {
        {input, input_size}
    };
    psa_outvec_t out_vec[] = {
        {output, output_size}
    };
    
    /* PSA 호출 */
    status = psa_call(handle, MY_SERVICE_IPC_OPERATION_1,
                     in_vec, 1, out_vec, 1);
    
    /* 연결 해제 */
    psa_close(handle);
    
    return (status == PSA_SUCCESS) ? MY_CUSTOM_STATUS_SUCCESS : MY_CUSTOM_STATUS_ERROR;
}

tfm_my_custom_status_t tfm_my_custom_operation_2(uint32_t parameter)
{
    psa_handle_t handle;
    psa_status_t status;
    
    handle = psa_connect(TFM_MY_CUSTOM_SERVICE_SID, 1);
    if (handle <= 0) {
        return MY_CUSTOM_STATUS_ERROR;
    }
    
    psa_invec_t in_vec[] = {
        {&parameter, sizeof(parameter)}
    };
    
    status = psa_call(handle, MY_SERVICE_IPC_OPERATION_2,
                     in_vec, 1, NULL, 0);
    
    psa_close(handle);
    
    return (status == PSA_SUCCESS) ? MY_CUSTOM_STATUS_SUCCESS : MY_CUSTOM_STATUS_ERROR;
}
```

## 고급 파티션 기능

### 인터럽트 처리
```yaml
# 인터럽트를 사용하는 파티션 매니페스트
"irqs": [
  {
    "source": "TFM_TIMER0_IRQ",
    "signal": "TIMER_SIGNAL",
    "tfm_irq_priority": 64
  }
]
```

```c
// 인터럽트 처리 코드
static void handle_timer_interrupt(void)
{
    psa_signal_t signals = psa_wait(TIMER_SIGNAL, PSA_BLOCK);
    
    if (signals & TIMER_SIGNAL) {
        /* 타이머 인터럽트 처리 */
        process_timer_event();
        
        /* 인터럽트 완료 신호 */
        psa_eoi(TIMER_SIGNAL);
    }
}
```

### MMIO 영역 접근
```yaml
# MMIO 영역 매핑
"mmio_regions": [
  {
    "name": "TFM_PERIPHERAL_GPIO",
    "permission": "READ-WRITE"
  }
]
```

### 보안 통신 패턴
```c
/* 보안 데이터 전송을 위한 패턴 */
typedef struct secure_message {
    uint32_t magic;           /* 메시지 검증용 매직 넘버 */
    uint32_t sequence;        /* 리플레이 공격 방지용 시퀀스 */
    uint16_t data_length;     /* 데이터 길이 */
    uint16_t checksum;        /* 간단한 체크섬 */
    uint8_t data[];          /* 실제 데이터 */
} secure_message_t;

/* 메시지 무결성 검증 */
static bool verify_message_integrity(const secure_message_t* msg)
{
    if (msg->magic != SECURE_MESSAGE_MAGIC) {
        return false;
    }
    
    /* 체크섬 계산 및 검증 */
    uint16_t calculated_checksum = calculate_checksum(msg->data, msg->data_length);
    return (calculated_checksum == msg->checksum);
}
```

## 성능 최적화

### 메모리 사용 최적화
```c
/* 정적 메모리 할당으로 성능 향상 */
#define MAX_CONCURRENT_CONNECTIONS 4
#define BUFFER_POOL_SIZE 8

static uint8_t g_buffer_pool[BUFFER_POOL_SIZE][BUFFER_SIZE];
static bool g_buffer_used[BUFFER_POOL_SIZE];
static connection_context_t g_connections[MAX_CONCURRENT_CONNECTIONS];

/* 버퍼 풀에서 빈 버퍼 할당 */
static uint8_t* allocate_buffer(void)
{
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!g_buffer_used[i]) {
            g_buffer_used[i] = true;
            return g_buffer_pool[i];
        }
    }
    return NULL;  /* 사용 가능한 버퍼 없음 */
}

/* 버퍼 해제 */
static void free_buffer(uint8_t* buffer)
{
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (g_buffer_pool[i] == buffer) {
            g_buffer_used[i] = false;
            break;
        }
    }
}
```

### 배치 처리 최적화
```c
/* 여러 연산을 하나의 PSA 호출로 배치 처리 */
typedef struct batch_operation {
    uint32_t operation_count;
    uint32_t operations[MAX_BATCH_SIZE];
    uint8_t data[MAX_BATCH_DATA_SIZE];
} batch_operation_t;

static psa_status_t handle_batch_request(psa_msg_t *msg)
{
    batch_operation_t batch;
    psa_status_t status = PSA_SUCCESS;
    
    /* 배치 요청 읽기 */
    size_t bytes_read = psa_read(msg->handle, 0, &batch, sizeof(batch));
    if (bytes_read != sizeof(batch)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* 배치 내 각 연산 순차 처리 */
    for (uint32_t i = 0; i < batch.operation_count; i++) {
        status = process_single_operation(batch.operations[i], &batch.data[i * OPERATION_DATA_SIZE]);
        if (status != PSA_SUCCESS) {
            break;  /* 하나라도 실패하면 중단 */
        }
    }
    
    return status;
}
```

## 디버깅 및 모니터링

### 파티션 상태 모니터링
```c
/* 파티션 통계 정보 */
typedef struct partition_stats {
    uint32_t message_count;       /* 처리된 메시지 수 */
    uint32_t error_count;         /* 오류 발생 횟수 */
    uint32_t active_connections;  /* 활성 연결 수 */
    uint32_t peak_memory_usage;   /* 최대 메모리 사용량 */
} partition_stats_t;

static partition_stats_t g_partition_stats = {0};

/* 통계 업데이트 매크로 */
#define UPDATE_STATS(field) do { g_partition_stats.field++; } while(0)

/* 주기적 통계 출력 */
static void print_partition_stats(void)
{
    LOG_INFSIG("=== Partition Statistics ===\\r\\n");
    LOG_INFSIG("Messages processed: %d\\r\\n", g_partition_stats.message_count);
    LOG_INFSIG("Errors occurred: %d\\r\\n", g_partition_stats.error_count);
    LOG_INFSIG("Active connections: %d\\r\\n", g_partition_stats.active_connections);
    LOG_INFSIG("Peak memory usage: %d bytes\\r\\n", g_partition_stats.peak_memory_usage);
}
```

### 에러 처리 및 복구
```c
/* 견고한 에러 처리 패턴 */
static psa_status_t robust_operation_handler(psa_msg_t *msg)
{
    psa_status_t status = PSA_SUCCESS;
    void* allocated_resource = NULL;
    
    /* 리소스 할당 */
    allocated_resource = allocate_operation_resource();
    if (!allocated_resource) {
        UPDATE_STATS(error_count);
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }
    
    /* 연산 수행 */
    status = perform_operation(msg, allocated_resource);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("Operation failed: %d\\r\\n", status);
        UPDATE_STATS(error_count);
        
        /* 실패 시 상태 복구 */
        restore_partition_state();
    } else {
        UPDATE_STATS(message_count);
    }
    
    /* 리소스 정리 */
    if (allocated_resource) {
        free_operation_resource(allocated_resource);
    }
    
    return status;
}

/* 파티션 상태 복구 */
static void restore_partition_state(void)
{
    /* 전역 상태 초기화 */
    reset_global_state();
    
    /* 메모리 정리 */
    cleanup_memory_allocations();
    
    /* 통계 정보 업데이트 */
    LOG_INFSIG("Partition state restored\\r\\n");
}
```

## 다음 단계

보안 파티션 개발을 완료했다면 다음 문서를 참조하세요:

- **[PSA API](./psa-api.md)** - 클라이언트-서버 통신 심화
- **[TinyMaix 통합](./tinymaix-integration.md)** - ML 기능 추가
- **[테스트 프레임워크](./testing-framework.md)** - 파티션 테스트 작성
- **[문제 해결](./troubleshooting.md)** - 파티션 관련 문제 해결