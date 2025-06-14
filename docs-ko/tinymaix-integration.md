# TinyMaix 통합 가이드

## 개요

TinyMaix는 마이크로컨트롤러에 최적화된 경량 신경망 추론 라이브러리입니다. 이 프로젝트에서는 TinyMaix를 TF-M 보안 파티션에 통합하여 하드웨어 보안을 갖춘 머신러닝 추론을 제공합니다.

## TinyMaix 아키텍처

### 라이브러리 구조
```
TinyMaix Library Structure:
┌─────────────────────────────────────────┐
│               TinyMaix API              │
├─────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────────┐   │
│  │   Model     │  │      Layer      │   │
│  │  Management │  │   Operations    │   │
│  │ (tm_model)  │  │  (tm_layers)    │   │
│  └─────────────┘  └─────────────────┘   │
├─────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────────┐   │
│  │    Math     │  │    Platform     │   │
│  │   Utils     │  │   Abstraction   │   │
│  │ (tm_utils)  │  │   (tm_port)     │   │
│  └─────────────┘  └─────────────────┘   │
└─────────────────────────────────────────┘
```

### 메모리 레이아웃
```
TinyMaix Memory Layout in Secure Partition:
┌─────────────────────────────────────────┐ ← High Address
│        Output Buffers (Stack)          │
├─────────────────────────────────────────┤
│        Intermediate Layers             │
├─────────────────────────────────────────┤
│        Input Buffers                   │
├─────────────────────────────────────────┤
│        Model Data (Decrypted)          │
├─────────────────────────────────────────┤
│        TinyMaix Runtime Code           │
└─────────────────────────────────────────┘ ← Low Address
```

## 보안 파티션 통합

### 서비스 인터페이스 정의
```c
// interface/include/tfm_tinymaix_inference_defs.h
#ifndef TFM_TINYMAIX_INFERENCE_DEFS_H
#define TFM_TINYMAIX_INFERENCE_DEFS_H

#include <stdint.h>
#include <stddef.h>

/* 서비스 식별자 */
#define TFM_TINYMAIX_SERVICE_SID    0x00000107U

/* 입력/출력 크기 */
#define TINYMAIX_INPUT_SIZE         (28 * 28)    /* MNIST 28x28 이미지 */
#define TINYMAIX_OUTPUT_SIZE        (10)         /* 10개 클래스 확률 */
#define TINYMAIX_MAX_CLASSES        (10)         /* MNIST 숫자 0-9 */

/* 서비스 상태 코드 */
typedef enum {
    TINYMAIX_STATUS_SUCCESS = 0,
    TINYMAIX_STATUS_ERROR_INIT = -1,
    TINYMAIX_STATUS_ERROR_MODEL_LOAD = -2,
    TINYMAIX_STATUS_ERROR_INPUT_INVALID = -3,
    TINYMAIX_STATUS_ERROR_INFERENCE_FAILED = -4,
    TINYMAIX_STATUS_ERROR_DECRYPTION_FAILED = -5,
    TINYMAIX_STATUS_ERROR_INSUFFICIENT_MEMORY = -6
} tfm_tinymaix_status_t;

/* API 함수 선언 */
tfm_tinymaix_status_t tfm_tinymaix_load_encrypted_model(void);

tfm_tinymaix_status_t tfm_tinymaix_run_inference(int* predicted_class);

tfm_tinymaix_status_t tfm_tinymaix_run_inference_with_data(
    const uint8_t* input_data, 
    size_t input_size,
    int* predicted_class);

tfm_tinymaix_status_t tfm_tinymaix_get_input_size(size_t* input_size);

tfm_tinymaix_status_t tfm_tinymaix_get_output_size(size_t* output_size);

#ifdef DEV_MODE
tfm_tinymaix_status_t tfm_tinymaix_get_model_key(uint8_t* key_buffer, size_t key_buffer_size);
#endif

#endif /* TFM_TINYMAIX_INFERENCE_DEFS_H */
```

### 클라이언트 API 구현
```c
// interface/src/tfm_tinymaix_inference_api.c
#include "tfm_tinymaix_inference_defs.h"
#include "psa/client.h"

/* 내부 IPC 메시지 타입 */
#define TINYMAIX_IPC_LOAD_MODEL         (0x1001U)
#define TINYMAIX_IPC_RUN_INFERENCE      (0x1002U)
#define TINYMAIX_IPC_RUN_INFERENCE_DATA (0x1003U)
#define TINYMAIX_IPC_GET_INPUT_SIZE     (0x1004U)
#define TINYMAIX_IPC_GET_OUTPUT_SIZE    (0x1005U)

#ifdef DEV_MODE
#define TINYMAIX_IPC_GET_MODEL_KEY      (0x1006U)
#endif

/* 암호화된 모델 로드 */
tfm_tinymaix_status_t tfm_tinymaix_load_encrypted_model(void)
{
    psa_handle_t handle;
    psa_status_t status;
    tfm_tinymaix_status_t result;
    
    /* 서비스 연결 */
    handle = psa_connect(TFM_TINYMAIX_SERVICE_SID, 1);
    if (handle <= 0) {
        return TINYMAIX_STATUS_ERROR_INIT;
    }
    
    /* 모델 로드 요청 */
    psa_outvec_t out_vec[] = {
        {&result, sizeof(result)}
    };
    
    status = psa_call(handle, TINYMAIX_IPC_LOAD_MODEL,
                     NULL, 0, out_vec, 1);
    
    psa_close(handle);
    
    return (status == PSA_SUCCESS) ? result : TINYMAIX_STATUS_ERROR_INIT;
}

/* 내장 이미지로 추론 실행 */
tfm_tinymaix_status_t tfm_tinymaix_run_inference(int* predicted_class)
{
    psa_handle_t handle;
    psa_status_t status;
    tfm_tinymaix_status_t result;
    
    if (!predicted_class) {
        return TINYMAIX_STATUS_ERROR_INPUT_INVALID;
    }
    
    handle = psa_connect(TFM_TINYMAIX_SERVICE_SID, 1);
    if (handle <= 0) {
        return TINYMAIX_STATUS_ERROR_INIT;
    }
    
    /* 결과 버퍼 */
    typedef struct {
        tfm_tinymaix_status_t status;
        int predicted_class;
    } inference_result_t;
    
    inference_result_t inference_result;
    
    psa_outvec_t out_vec[] = {
        {&inference_result, sizeof(inference_result)}
    };
    
    status = psa_call(handle, TINYMAIX_IPC_RUN_INFERENCE,
                     NULL, 0, out_vec, 1);
    
    psa_close(handle);
    
    if (status == PSA_SUCCESS && inference_result.status == TINYMAIX_STATUS_SUCCESS) {
        *predicted_class = inference_result.predicted_class;
        return TINYMAIX_STATUS_SUCCESS;
    }
    
    return inference_result.status;
}

/* 사용자 데이터로 추론 실행 */
tfm_tinymaix_status_t tfm_tinymaix_run_inference_with_data(
    const uint8_t* input_data,
    size_t input_size, 
    int* predicted_class)
{
    psa_handle_t handle;
    psa_status_t status;
    
    if (!input_data || !predicted_class || input_size != TINYMAIX_INPUT_SIZE) {
        return TINYMAIX_STATUS_ERROR_INPUT_INVALID;
    }
    
    handle = psa_connect(TFM_TINYMAIX_SERVICE_SID, 1);
    if (handle <= 0) {
        return TINYMAIX_STATUS_ERROR_INIT;
    }
    
    /* 입력 데이터 전송 */
    psa_invec_t in_vec[] = {
        {input_data, input_size}
    };
    
    /* 결과 수신 */
    typedef struct {
        tfm_tinymaix_status_t status;
        int predicted_class;
    } inference_result_t;
    
    inference_result_t inference_result;
    
    psa_outvec_t out_vec[] = {
        {&inference_result, sizeof(inference_result)}
    };
    
    status = psa_call(handle, TINYMAIX_IPC_RUN_INFERENCE_DATA,
                     in_vec, 1, out_vec, 1);
    
    psa_close(handle);
    
    if (status == PSA_SUCCESS && inference_result.status == TINYMAIX_STATUS_SUCCESS) {
        *predicted_class = inference_result.predicted_class;
        return TINYMAIX_STATUS_SUCCESS;
    }
    
    return inference_result.status;
}

#ifdef DEV_MODE
/* DEV_MODE: HUK 파생 키 추출 */
tfm_tinymaix_status_t tfm_tinymaix_get_model_key(uint8_t* key_buffer, size_t key_buffer_size)
{
    psa_handle_t handle;
    psa_status_t status;
    tfm_tinymaix_status_t result = TINYMAIX_STATUS_SUCCESS;
    
    if (!key_buffer || key_buffer_size < 16) {
        return TINYMAIX_STATUS_ERROR_INPUT_INVALID;
    }
    
    handle = psa_connect(TFM_TINYMAIX_SERVICE_SID, 1);
    if (handle <= 0) {
        return TINYMAIX_STATUS_ERROR_INIT;
    }
    
    psa_outvec_t out_vec[] = {
        {key_buffer, key_buffer_size},
        {&result, sizeof(result)}
    };
    
    status = psa_call(handle, TINYMAIX_IPC_GET_MODEL_KEY,
                     NULL, 0, out_vec, 2);
    
    psa_close(handle);
    
    return (status == PSA_SUCCESS) ? result : TINYMAIX_STATUS_ERROR_INIT;
}
#endif
```

## 보안 파티션 서비스 구현

### 파티션 매니페스트
```yaml
# partitions/tinymaix_inference/tinymaix_inference_manifest.yaml
{
  "description": "TF-M TinyMaix Neural Network Inference Service",
  "psa_framework_version": "1.1",
  "name": "TFM_SP_TINYMAIX_INFERENCE", 
  "type": "APPLICATION-ROT",
  "priority": "NORMAL",
  "id": "0x00000107",
  "entry_point": "tfm_tinymaix_inference_main",
  "stack_size": "0x2000",  # 8KB - 신경망 연산을 위한 큰 스택
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
      "*tinymaix*"  # TinyMaix 정적 라이브러리 포함
    ]
  }
}
```

### 서비스 메인 구현
```c
// partitions/tinymaix_inference/tinymaix_inference.c
#include "tfm_sp_log.h"
#include "psa/service.h"
#include "psa_manifest/tfm_tinymaix_inference.h"
#include "tfm_crypto_defs.h"
#include "encrypted_mnist_model_psa.h"

/* TinyMaix 라이브러리 */
#include "tinymaix/include/tinymaix.h"

/* 내부 상수 */
#define MODEL_BUFFER_SIZE           (32 * 1024)     /* 32KB 모델 버퍼 */
#define INPUT_BUFFER_SIZE           (28 * 28)       /* MNIST 입력 크기 */
#define OUTPUT_BUFFER_SIZE          (10 * 4)        /* 10개 float 출력 */
#define DERIVED_KEY_LEN             (16)            /* AES-128 키 크기 */

/* 내장 MNIST 테스트 이미지 (숫자 '2') */
static const uint8_t builtin_test_image[INPUT_BUFFER_SIZE] = {
    /* 28x28 픽셀 데이터 - 손으로 쓴 숫자 '2' 패턴 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* ... 실제 MNIST 데이터 패턴 ... */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* 전역 상태 */
static bool g_model_loaded = false;
static tm_mdl_t g_model;
static uint8_t g_model_buffer[MODEL_BUFFER_SIZE];
static uint8_t g_input_buffer[INPUT_BUFFER_SIZE];
static float g_output_buffer[OUTPUT_BUFFER_SIZE / sizeof(float)];

/* 내부 함수 선언 */
static psa_status_t decrypt_model_cbc(const uint8_t* encrypted_data,
                                     size_t encrypted_size,
                                     const uint8_t* key,
                                     uint8_t* decrypted_data,
                                     size_t* decrypted_size);

static psa_status_t derive_key_from_huk(const char* label,
                                       uint8_t* derived_key,
                                       size_t key_length);

static bool validate_pkcs7_padding(const uint8_t* data, size_t size);
static int find_max_class(const float* output, size_t output_count);

/* TinyMaix 서비스 메인 진입점 */
void tfm_tinymaix_inference_main(void)
{
    psa_msg_t msg;
    
    LOG_INFSIG("[TinyMaix] Service started\\r\\n");
    
    while (1) {
        /* PSA 메시지 대기 */
        psa_status_t status = psa_get(PSA_WAIT_ANY, &msg);
        if (status != PSA_SUCCESS) {
            continue;
        }
        
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                LOG_INFSIG("[TinyMaix] Client connected\\r\\n");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case PSA_IPC_CALL:
                handle_tinymaix_request(&msg);
                break;
                
            case PSA_IPC_DISCONNECT:
                LOG_INFSIG("[TinyMaix] Client disconnected\\r\\n");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            default:
                LOG_ERRSIG("[TinyMaix] Unknown message type: %d\\r\\n", msg.type);
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

/* TinyMaix 요청 처리 */
static void handle_tinymaix_request(psa_msg_t *msg)
{
    uint32_t operation = msg->rhandle;
    psa_status_t status = PSA_SUCCESS;
    
    switch (operation) {
        case TINYMAIX_IPC_LOAD_MODEL:
            status = handle_load_model(msg);
            break;
            
        case TINYMAIX_IPC_RUN_INFERENCE:
            status = handle_run_inference(msg);
            break;
            
        case TINYMAIX_IPC_RUN_INFERENCE_DATA:
            status = handle_run_inference_with_data(msg);
            break;
            
        case TINYMAIX_IPC_GET_INPUT_SIZE:
            status = handle_get_input_size(msg);
            break;
            
        case TINYMAIX_IPC_GET_OUTPUT_SIZE:
            status = handle_get_output_size(msg);
            break;
            
#ifdef DEV_MODE
        case TINYMAIX_IPC_GET_MODEL_KEY:
            status = handle_get_model_key(msg);
            break;
#endif
            
        default:
            LOG_ERRSIG("[TinyMaix] Unknown operation: %d\\r\\n", operation);
            status = PSA_ERROR_NOT_SUPPORTED;
            break;
    }
    
    psa_reply(msg.handle, status);
}

/* 암호화된 모델 로드 */
static psa_status_t handle_load_model(psa_msg_t *msg)
{
    psa_status_t status;
    uint8_t derived_key[DERIVED_KEY_LEN];
    uint8_t decrypted_model[MODEL_BUFFER_SIZE];
    size_t decrypted_size = sizeof(decrypted_model);
    tfm_tinymaix_status_t result = TINYMAIX_STATUS_SUCCESS;
    
    LOG_INFSIG("[TinyMaix] Loading encrypted MNIST model...\\r\\n");
    
    /* HUK에서 모델 복호화 키 파생 */
    const char *huk_label = "pico2w-tinymaix-model-aes128-v1.0";
    status = derive_key_from_huk(huk_label, derived_key, sizeof(derived_key));
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix] Key derivation failed: %d\\r\\n", status);
        result = TINYMAIX_STATUS_ERROR_DECRYPTION_FAILED;
        goto cleanup;
    }
    
    /* 내장 암호화 모델 복호화 */
    status = decrypt_model_cbc(encrypted_mnist_model_psa_data,
                              encrypted_mnist_model_psa_size,
                              derived_key,
                              decrypted_model,
                              &decrypted_size);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix] Model decryption failed: %d\\r\\n", status);
        result = TINYMAIX_STATUS_ERROR_DECRYPTION_FAILED;
        goto cleanup;
    }
    
    LOG_INFSIG("[TinyMaix] Model decrypted, size: %d bytes\\r\\n", decrypted_size);
    
    /* TinyMaix 모델 로드 */
    tm_stat_t tm_status = tm_load(&g_model, decrypted_model,
                                 g_model_buffer, sizeof(g_model_buffer));
    if (tm_status != TM_OK) {
        LOG_ERRSIG("[TinyMaix] Model load failed: %d\\r\\n", tm_status);
        result = TINYMAIX_STATUS_ERROR_MODEL_LOAD;
        goto cleanup;
    }
    
    g_model_loaded = true;
    LOG_INFSIG("[TinyMaix] Model loaded successfully\\r\\n");
    
cleanup:
    /* 민감한 복호화 키 메모리 정리 */
    memset(derived_key, 0, sizeof(derived_key));
    
    /* 결과 전송 */
    psa_write(msg->handle, 0, &result, sizeof(result));
    
    return PSA_SUCCESS;
}

/* 내장 이미지로 추론 실행 */
static psa_status_t handle_run_inference(psa_msg_t *msg)
{
    tfm_tinymaix_status_t result = TINYMAIX_STATUS_SUCCESS;
    int predicted_class = -1;
    
    if (!g_model_loaded) {
        LOG_ERRSIG("[TinyMaix] Model not loaded\\r\\n");
        result = TINYMAIX_STATUS_ERROR_MODEL_LOAD;
        goto reply;
    }
    
    LOG_INFSIG("[TinyMaix] Running inference with builtin image...\\r\\n");
    
    /* 내장 테스트 이미지를 입력 버퍼로 복사 */
    memcpy(g_input_buffer, builtin_test_image, sizeof(builtin_test_image));
    
    /* TinyMaix 추론 실행 */
    tm_stat_t tm_status = tm_run(&g_model, g_input_buffer, g_output_buffer);
    if (tm_status != TM_OK) {
        LOG_ERRSIG("[TinyMaix] Inference failed: %d\\r\\n", tm_status);
        result = TINYMAIX_STATUS_ERROR_INFERENCE_FAILED;
        goto reply;
    }
    
    /* 최대 확률 클래스 찾기 */
    predicted_class = find_max_class(g_output_buffer, 10);
    
    LOG_INFSIG("[TinyMaix] Inference completed, predicted: %d\\r\\n", predicted_class);
    
reply:
    /* 결과 구조체 */
    typedef struct {
        tfm_tinymaix_status_t status;
        int predicted_class;
    } inference_result_t;
    
    inference_result_t inference_result = {
        .status = result,
        .predicted_class = predicted_class
    };
    
    psa_write(msg->handle, 0, &inference_result, sizeof(inference_result));
    
    return PSA_SUCCESS;
}

/* 사용자 데이터로 추론 실행 */
static psa_status_t handle_run_inference_with_data(psa_msg_t *msg)
{
    tfm_tinymaix_status_t result = TINYMAIX_STATUS_SUCCESS;
    int predicted_class = -1;
    
    if (!g_model_loaded) {
        result = TINYMAIX_STATUS_ERROR_MODEL_LOAD;
        goto reply;
    }
    
    /* 입력 데이터 읽기 */
    size_t bytes_read = psa_read(msg->handle, 0, g_input_buffer, sizeof(g_input_buffer));
    if (bytes_read != INPUT_BUFFER_SIZE) {
        LOG_ERRSIG("[TinyMaix] Invalid input size: %d\\r\\n", bytes_read);
        result = TINYMAIX_STATUS_ERROR_INPUT_INVALID;
        goto reply;
    }
    
    LOG_INFSIG("[TinyMaix] Running inference with custom data...\\r\\n");
    
    /* TinyMaix 추론 실행 */
    tm_stat_t tm_status = tm_run(&g_model, g_input_buffer, g_output_buffer);
    if (tm_status != TM_OK) {
        LOG_ERRSIG("[TinyMaix] Inference failed: %d\\r\\n", tm_status);
        result = TINYMAIX_STATUS_ERROR_INFERENCE_FAILED;
        goto reply;
    }
    
    /* 최대 확률 클래스 찾기 */
    predicted_class = find_max_class(g_output_buffer, 10);
    
    LOG_INFSIG("[TinyMaix] Custom inference completed, predicted: %d\\r\\n", predicted_class);
    
reply:
    typedef struct {
        tfm_tinymaix_status_t status;
        int predicted_class;
    } inference_result_t;
    
    inference_result_t inference_result = {
        .status = result,
        .predicted_class = predicted_class
    };
    
    psa_write(msg->handle, 0, &inference_result, sizeof(inference_result));
    
    return PSA_SUCCESS;
}

#ifdef DEV_MODE
/* DEV_MODE: HUK 파생 키 추출 */
static psa_status_t handle_get_model_key(psa_msg_t *msg)
{
    uint8_t derived_key[DERIVED_KEY_LEN];
    psa_status_t status;
    tfm_tinymaix_status_t result = TINYMAIX_STATUS_SUCCESS;
    
    LOG_INFSIG("[TinyMaix] Getting HUK-derived model key (DEV_MODE)\\r\\n");
    
    /* HUK에서 키 파생 */
    const char *huk_label = "pico2w-tinymaix-model-aes128-v1.0";
    status = derive_key_from_huk(huk_label, derived_key, sizeof(derived_key));
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix] Key derivation failed: %d\\r\\n", status);
        result = TINYMAIX_STATUS_ERROR_DECRYPTION_FAILED;
    } else {
        /* 키 16진수 출력 (디버깅용) */
        LOG_INFSIG("HUK-derived key: ");
        for (int i = 0; i < sizeof(derived_key); i++) {
            LOG_INFSIG("%02x", derived_key[i]);
        }
        LOG_INFSIG("\\r\\n");
    }
    
    /* 키와 상태 전송 */
    psa_write(msg->handle, 0, derived_key, sizeof(derived_key));
    psa_write(msg->handle, 1, &result, sizeof(result));
    
    /* 보안을 위해 키 메모리 정리 */
    memset(derived_key, 0, sizeof(derived_key));
    
    return PSA_SUCCESS;
}
#endif

/* AES-CBC 모델 복호화 */
static psa_status_t decrypt_model_cbc(const uint8_t* encrypted_data,
                                     size_t encrypted_size,
                                     const uint8_t* key,
                                     uint8_t* decrypted_data,
                                     size_t* decrypted_size)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;
    psa_status_t status;
    
    /* 최소 헤더 크기 검증 (magic + version + size + iv) */
    if (encrypted_size < 28) {
        LOG_ERRSIG("[TinyMaix] Encrypted data too small\\r\\n");
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* 패키지 헤더 파싱 */
    const uint8_t* magic = encrypted_data;
    const uint32_t* version = (const uint32_t*)(encrypted_data + 4);
    const uint32_t* original_size = (const uint32_t*)(encrypted_data + 8);
    const uint8_t* iv = encrypted_data + 12;
    const uint8_t* ciphertext = encrypted_data + 28;
    size_t ciphertext_size = encrypted_size - 28;
    
    /* 매직 헤더 검증 */
    if (memcmp(magic, "TMAX", 4) != 0) {
        LOG_ERRSIG("[TinyMaix] Invalid model magic\\r\\n");
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* 버전 검증 (CBC = 3) */
    if (*version != 3) {
        LOG_ERRSIG("[TinyMaix] Unsupported model version: %d\\r\\n", *version);
        return PSA_ERROR_NOT_SUPPORTED;
    }
    
    LOG_INFSIG("[TinyMaix] Decrypting model: version %d, size %d\\r\\n", 
               *version, *original_size);
    
    /* AES-128 키 설정 */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    
    /* 키 가져오기 */
    status = psa_import_key(&attributes, key, 16, &key_id);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix] Key import failed: %d\\r\\n", status);
        return status;
    }
    
    /* CBC 복호화 (패딩 포함) */
    size_t output_length;
    status = psa_cipher_decrypt(key_id, PSA_ALG_CBC_NO_PADDING,
                               ciphertext, ciphertext_size,
                               decrypted_data, *decrypted_size, &output_length);
    
    /* 키 정리 */
    psa_destroy_key(key_id);
    
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix] Decryption failed: %d\\r\\n", status);
        return status;
    }
    
    /* PKCS7 패딩 검증 및 제거 */
    if (!validate_pkcs7_padding(decrypted_data, output_length)) {
        LOG_ERRSIG("[TinyMaix] Invalid PKCS7 padding\\r\\n");
        return PSA_ERROR_INVALID_SIGNATURE;
    }
    
    /* 패딩 크기 계산 */
    uint8_t padding_size = decrypted_data[output_length - 1];
    *decrypted_size = output_length - padding_size;
    
    LOG_INFSIG("[TinyMaix] Model decrypted successfully: %d bytes\\r\\n", *decrypted_size);
    
    return PSA_SUCCESS;
}

/* PKCS7 패딩 검증 */
static bool validate_pkcs7_padding(const uint8_t* data, size_t size)
{
    if (size == 0) return false;
    
    uint8_t padding_size = data[size - 1];
    
    /* 패딩 크기 유효성 검사 */
    if (padding_size == 0 || padding_size > 16 || padding_size > size) {
        return false;
    }
    
    /* 모든 패딩 바이트가 동일한지 확인 */
    for (size_t i = size - padding_size; i < size; i++) {
        if (data[i] != padding_size) {
            return false;
        }
    }
    
    return true;
}

/* 최대 확률 클래스 찾기 */
static int find_max_class(const float* output, size_t output_count)
{
    if (!output || output_count == 0) {
        return -1;
    }
    
    int max_class = 0;
    float max_value = output[0];
    
    for (size_t i = 1; i < output_count; i++) {
        if (output[i] > max_value) {
            max_value = output[i];
            max_class = i;
        }
    }
    
    /* 디버그 출력 */
    LOG_INFSIG("[TinyMaix] Output probabilities: ");
    for (size_t i = 0; i < output_count; i++) {
        LOG_INFSIG("%.3f ", output[i]);
    }
    LOG_INFSIG("\\r\\n");
    
    return max_class;
}
```

## 모델 준비 및 암호화

### 원본 MNIST 모델
```c
// models/mnist_valid_q.h - TinyMaix 형식 모델
#ifndef MNIST_VALID_Q_H
#define MNIST_VALID_Q_H

#include <stdint.h>

/* 모델 메타데이터 */
#define MDL_BUF_LEN (32*1024)
#define LBUF_LEN (8*1024)

/* 양자화된 MNIST 모델 데이터 */
const uint8_t mnist_valid_q[] = {
    /* TinyMaix 바이너리 모델 데이터 */
    0x54, 0x4D, 0x44, 0x4C,  /* TMDL 매직 */
    0x01, 0x00, 0x00, 0x00,  /* 버전 */
    /* ... 실제 모델 가중치 데이터 ... */
};

#endif
```

### 암호화 스크립트 사용
```bash
# HUK 파생 키를 사용한 모델 암호화
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header

# 생성된 파일들:
# - models/encrypted_mnist_model_psa.bin (암호화된 바이너리)
# - models/encrypted_mnist_model_psa.h (C 헤더)
# - models/encrypted_mnist_model_psa.c (C 소스)
```

### 생성된 암호화 모델
```c
// models/encrypted_mnist_model_psa.c
#include "encrypted_mnist_model_psa.h"

/* 암호화된 모델 데이터 (CBC 포맷) */
const uint8_t encrypted_mnist_model_psa_data[] = {
    /* PSA CBC 패키지 헤더 */
    0x54, 0x4D, 0x41, 0x58,  /* TMAX 매직 */
    0x03, 0x00, 0x00, 0x00,  /* 버전 3 (CBC) */
    0x00, 0x40, 0x00, 0x00,  /* 원본 크기 */
    /* 16바이트 IV */
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    /* 암호화된 모델 데이터 */
    0xAB, 0xCD, 0xEF, 0x01, /* ... */
};

const size_t encrypted_mnist_model_psa_size = sizeof(encrypted_mnist_model_psa_data);
```

## 테스트 및 검증

### 기본 추론 테스트
```c
// tflm_ns/tinymaix_inference_test.c
void test_tinymaix_basic_functionality(void)
{
    printf("[TinyMaix Test] Testing encrypted model functionality...\\n");
    
    tfm_tinymaix_status_t status;
    int predicted_class = -1;
    
    /* 1. 암호화된 모델 로드 */
    printf("[TinyMaix Test] 1. Loading builtin encrypted MNIST model...\\n");
    status = tfm_tinymaix_load_encrypted_model();
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Encrypted model loaded successfully\\n");
    } else {
        printf("[TinyMaix Test] ✗ Model load failed: %d\\n", status);
        return;
    }
    
    /* 2. 내장 이미지 추론 */
    printf("[TinyMaix Test] 2. Running inference with built-in image...\\n");
    status = tfm_tinymaix_run_inference(&predicted_class);
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Built-in inference successful\\n");
        printf("[TinyMaix Test] ✓ Predicted digit: %d\\n", predicted_class);
    } else {
        printf("[TinyMaix Test] ✗ Built-in inference failed: %d\\n", status);
        return;
    }
    
    /* 3. 커스텀 이미지 추론 */
    printf("[TinyMaix Test] 3. Running inference with custom image...\\n");
    
    uint8_t custom_image[28*28];
    create_test_digit_pattern(custom_image, 7); /* 숫자 '7' 생성 */
    
    status = tfm_tinymaix_run_inference_with_data(custom_image, sizeof(custom_image), 
                                                  &predicted_class);
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Custom inference successful\\n");
        printf("[TinyMaix Test] ✓ Predicted digit: %d\\n", predicted_class);
    } else {
        printf("[TinyMaix Test] ✗ Custom inference failed: %d\\n", status);
    }
}

/* 테스트용 숫자 패턴 생성 */
static void create_test_digit_pattern(uint8_t* image, int digit)
{
    memset(image, 0, 28*28);
    
    switch (digit) {
        case 7:
            /* '7' 패턴 그리기 */
            for (int x = 10; x < 18; x++) {
                image[8 * 28 + x] = 255;  /* 상단 가로선 */
            }
            for (int y = 8; y < 20; y++) {
                image[y * 28 + 17] = 255; /* 우측 세로선 */
            }
            break;
            
        default:
            /* 기본 사각형 패턴 */
            for (int y = 10; y < 18; y++) {
                for (int x = 10; x < 18; x++) {
                    image[y * 28 + x] = 255;
                }
            }
            break;
    }
}
```

### DEV_MODE 테스트
```c
#ifdef DEV_MODE
void test_tinymaix_get_model_key(void)
{
    printf("[TinyMaix Test] Testing HUK-derived model key (DEV_MODE)...\\n");
    
    tfm_tinymaix_status_t status;
    uint8_t key_buffer[16];
    
    /* HUK 파생 키 추출 */
    status = tfm_tinymaix_get_model_key(key_buffer, sizeof(key_buffer));
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix Test] ✓ Model key retrieved successfully\\n");
        printf("[TinyMaix Test] ✓ Key (hex): ");
        for (int i = 0; i < 16; i++) {
            printf("%02x", key_buffer[i]);
        }
        printf("\\n");
    } else {
        printf("[TinyMaix Test] ✗ Failed to get model key: %d\\n", status);
    }
}
#endif
```

## 성능 최적화

### 메모리 사용 최적화
```c
/* 정적 메모리 할당으로 성능 향상 */
#define TINYMAIX_MEMORY_POOL_SIZE   (64 * 1024)
static uint8_t g_tinymaix_memory_pool[TINYMAIX_MEMORY_POOL_SIZE];
static size_t g_memory_pool_offset = 0;

/* 메모리 풀에서 할당 */
static void* tinymaix_malloc(size_t size)
{
    size = (size + 3) & ~3;  /* 4바이트 정렬 */
    
    if (g_memory_pool_offset + size > TINYMAIX_MEMORY_POOL_SIZE) {
        LOG_ERRSIG("[TinyMaix] Memory pool exhausted\\r\\n");
        return NULL;
    }
    
    void* ptr = &g_tinymaix_memory_pool[g_memory_pool_offset];
    g_memory_pool_offset += size;
    
    return ptr;
}

/* 메모리 풀 리셋 */
static void tinymaix_memory_reset(void)
{
    g_memory_pool_offset = 0;
    memset(g_tinymaix_memory_pool, 0, sizeof(g_tinymaix_memory_pool));
}
```

### 연산 최적화
```c
/* ARM Cortex-M33 최적화 설정 */
#define TM_MDL_TYPE TM_MDL_FP32      /* 32비트 부동소수점 */
#define TM_ARCH     TM_ARCH_ARM_CM4  /* ARM Cortex-M4 최적화 */
#define TM_OPT_LEVEL 2               /* 최적화 레벨 2 */

/* TinyMaix 설정 재정의 */
#ifndef TM_ARCH
#define TM_ARCH TM_ARCH_ARM_CM4
#endif

#ifndef TM_OPT_LEVEL  
#define TM_OPT_LEVEL 2
#endif
```

## 문제 해결

### 모델 로드 실패
```bash
# 증상: 모델 로드 시 복호화 오류
[TinyMaix] Model decryption failed: -135

# 해결 방법:
# 1. HUK 키 재추출
./build.sh DEV_MODE
# 시리얼에서 키 복사

# 2. 모델 재암호화
echo "새로운_HUK_키" | xxd -r -p > models/model_key_psa.bin
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

### 추론 실패
```c
// 증상: 추론 시 메모리 부족 오류
[TinyMaix] Inference failed: -2

// 해결 방법: 스택 크기 증가
// partitions/tinymaix_inference/tinymaix_inference_manifest.yaml
"stack_size": "0x4000",  // 8KB → 16KB로 증가
```

### 성능 저하
```c
// 성능 측정 코드 추가
uint32_t start_time = osKernelGetTickCount();
tm_stat_t result = tm_run(&g_model, input, output);
uint32_t end_time = osKernelGetTickCount();

LOG_INFSIG("[TinyMaix] Inference time: %d ms\\r\\n", end_time - start_time);

// 일반적인 성능: 50-200ms (MNIST)
// 최적화 후: 20-50ms
```

## 다음 단계

TinyMaix 통합을 완료했다면 다음 문서를 참조하세요:

- **[모델 암호화](./model-encryption.md)** - 고급 암호화 기법
- **[PSA API](./psa-api.md)** - 보안 통신 최적화  
- **[테스트 프레임워크](./testing-framework.md)** - ML 테스트 작성
- **[문제 해결](./troubleshooting.md)** - TinyMaix 관련 문제 해결