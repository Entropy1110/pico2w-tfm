# PSA API 가이드

## 개요

PSA (Platform Security Architecture) API는 ARM에서 정의한 표준화된 보안 프레임워크입니다. TF-M에서 PSA API는 보안 세계와 비보안 세계 간의 안전한 통신을 위한 표준 인터페이스를 제공합니다.

## PSA 서비스 모델

### 클라이언트-서버 아키텍처
```
PSA 통신 모델:
┌─────────────────────────────────────────┐
│           비보안 세계 (NSPE)              │
├─────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────────┐   │
│  │ 애플리케이션  │  │    PSA Client   │   │
│  │    코드     │──│      API        │   │
│  └─────────────┘  └─────────────────┘   │
└─────────────────────┬───────────────────┘
                     │ PSA 호출
          ┌──────────▼──────────┐
          │    PSA Router       │  ← TF-M 핵심
          │  (Message Queue)    │
          └──────────┬──────────┘
                     │ IPC 메시지
┌─────────────────────▼───────────────────┐
│            보안 세계 (SPE)               │
├─────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────────┐   │
│  │   보안 서비스 │  │   PSA Service   │   │
│  │  (Echo,     │──│    Handler      │   │
│  │  TinyMaix)  │  │                 │   │
│  └─────────────┘  └─────────────────┘   │
└─────────────────────────────────────────┘
```

### 연결 기반 vs 비연결 기반
```c
/* 연결 기반 서비스 (Connection-based) */
// 1. 연결 설정
psa_handle_t handle = psa_connect(SERVICE_SID, 1);

// 2. 서비스 호출 (여러 번 가능)
psa_status_t status = psa_call(handle, OPERATION_1, in_vec, 1, out_vec, 1);
psa_status_t status = psa_call(handle, OPERATION_2, in_vec, 1, out_vec, 1);

// 3. 연결 해제
psa_close(handle);

/* 비연결 기반 서비스 (Stateless) */
// 직접 호출 (PSA Crypto, Storage 등)
psa_status_t status = psa_crypto_function(parameters...);
```

## 클라이언트 API 패턴

### 기본 호출 패턴
```c
// interface/src/tfm_echo_api.c - Echo 서비스 예제
#include "tfm_echo_defs.h"
#include "psa/client.h"

#define TFM_ECHO_SERVICE_SID    0x00000105U

tfm_echo_status_t tfm_echo_call(const uint8_t* input, size_t input_size,
                                uint8_t* output, size_t output_size)
{
    psa_handle_t handle;
    psa_status_t status;
    
    /* 1. 서비스 연결 */
    handle = psa_connect(TFM_ECHO_SERVICE_SID, 1);
    if (handle <= 0) {
        return ECHO_STATUS_ERROR_CONNECTION_FAILED;
    }
    
    /* 2. 입력/출력 벡터 설정 */
    psa_invec_t in_vec[] = {
        {input, input_size}
    };
    psa_outvec_t out_vec[] = {
        {output, output_size}
    };
    
    /* 3. PSA 호출 */
    status = psa_call(handle, 0, in_vec, 1, out_vec, 1);
    
    /* 4. 연결 해제 */
    psa_close(handle);
    
    /* 5. 결과 변환 */
    return (status == PSA_SUCCESS) ? ECHO_STATUS_SUCCESS : ECHO_STATUS_ERROR;
}
```

### 고급 호출 패턴 - 다중 연산
```c
// interface/src/tfm_tinymaix_inference_api.c - TinyMaix 예제
tfm_tinymaix_status_t tfm_tinymaix_full_inference_pipeline(
    const uint8_t* input_data,
    size_t input_size,
    int* predicted_class)
{
    psa_handle_t handle;
    psa_status_t status;
    tfm_tinymaix_status_t result;
    
    /* 서비스 연결 */
    handle = psa_connect(TFM_TINYMAIX_SERVICE_SID, 1);
    if (handle <= 0) {
        return TINYMAIX_STATUS_ERROR_INIT;
    }
    
    /* 1. 모델 로드 */
    typedef struct {
        tfm_tinymaix_status_t status;
    } load_result_t;
    
    load_result_t load_result;
    psa_outvec_t load_out_vec[] = {{&load_result, sizeof(load_result)}};
    
    status = psa_call(handle, TINYMAIX_IPC_LOAD_MODEL, NULL, 0, load_out_vec, 1);
    if (status != PSA_SUCCESS || load_result.status != TINYMAIX_STATUS_SUCCESS) {
        psa_close(handle);
        return load_result.status;
    }
    
    /* 2. 입력 데이터 설정 */
    psa_invec_t input_vec[] = {{input_data, input_size}};
    
    typedef struct {
        tfm_tinymaix_status_t status;
    } input_result_t;
    
    input_result_t input_result;
    psa_outvec_t input_out_vec[] = {{&input_result, sizeof(input_result)}};
    
    status = psa_call(handle, TINYMAIX_IPC_SET_INPUT, input_vec, 1, input_out_vec, 1);
    if (status != PSA_SUCCESS || input_result.status != TINYMAIX_STATUS_SUCCESS) {
        psa_close(handle);
        return input_result.status;
    }
    
    /* 3. 추론 실행 */
    typedef struct {
        tfm_tinymaix_status_t status;
        int predicted_class;
    } inference_result_t;
    
    inference_result_t inference_result;
    psa_outvec_t inference_out_vec[] = {{&inference_result, sizeof(inference_result)}};
    
    status = psa_call(handle, TINYMAIX_IPC_RUN_INFERENCE, NULL, 0, inference_out_vec, 1);
    
    /* 연결 해제 */
    psa_close(handle);
    
    /* 결과 반환 */
    if (status == PSA_SUCCESS && inference_result.status == TINYMAIX_STATUS_SUCCESS) {
        *predicted_class = inference_result.predicted_class;
        return TINYMAIX_STATUS_SUCCESS;
    }
    
    return inference_result.status;
}
```

### 에러 처리 및 재시도
```c
/* 견고한 PSA 클라이언트 구현 */
static tfm_service_status_t robust_psa_call_with_retry(
    uint32_t service_sid,
    uint32_t operation,
    const psa_invec_t* in_vec,
    size_t in_len,
    psa_outvec_t* out_vec,
    size_t out_len,
    int max_retries)
{
    psa_handle_t handle;
    psa_status_t status;
    int retry_count = 0;
    
    while (retry_count < max_retries) {
        /* 연결 시도 */
        handle = psa_connect(service_sid, 1);
        if (handle <= 0) {
            /* 연결 실패 - 잠시 대기 후 재시도 */
            if (handle == PSA_ERROR_CONNECTION_BUSY) {
                osDelay(10); /* 10ms 대기 */
                retry_count++;
                continue;
            } else {
                /* 복구 불가능한 오류 */
                return SERVICE_STATUS_CONNECTION_FAILED;
            }
        }
        
        /* PSA 호출 */
        status = psa_call(handle, operation, in_vec, in_len, out_vec, out_len);
        
        /* 연결 해제 */
        psa_close(handle);
        
        /* 결과 확인 */
        if (status == PSA_SUCCESS) {
            return SERVICE_STATUS_SUCCESS;
        } else if (status == PSA_ERROR_CONNECTION_BUSY || 
                   status == PSA_ERROR_INSUFFICIENT_MEMORY) {
            /* 일시적 오류 - 재시도 */
            osDelay(20); /* 20ms 대기 */
            retry_count++;
            continue;
        } else {
            /* 영구적 오류 */
            return SERVICE_STATUS_OPERATION_FAILED;
        }
    }
    
    return SERVICE_STATUS_MAX_RETRIES_EXCEEDED;
}
```

## 서비스 구현 패턴

### 기본 서비스 루프
```c
// partitions/echo_service/echo_service.c
void tfm_echo_service_main(void)
{
    psa_msg_t msg;
    
    LOG_INFSIG("[Echo] Service started\\r\\n");
    
    while (1) {
        /* PSA 메시지 대기 */
        psa_status_t status = psa_get(PSA_WAIT_ANY, &msg);
        if (status != PSA_SUCCESS) {
            /* 메시지 수신 실패 - 계속 대기 */
            continue;
        }
        
        /* 메시지 타입별 처리 */
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                handle_client_connect(&msg);
                break;
                
            case PSA_IPC_CALL:
                handle_service_call(&msg);
                break;
                
            case PSA_IPC_DISCONNECT:
                handle_client_disconnect(&msg);
                break;
                
            default:
                LOG_ERRSIG("[Echo] Unknown message type: %d\\r\\n", msg.type);
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}

/* 클라이언트 연결 처리 */
static void handle_client_connect(psa_msg_t *msg)
{
    /* 연결 허용 여부 확인 */
    if (get_active_connection_count() >= MAX_CONNECTIONS) {
        LOG_ERRSIG("[Echo] Too many connections\\r\\n");
        psa_reply(msg->handle, PSA_ERROR_CONNECTION_REFUSED);
        return;
    }
    
    /* 연결 정보 저장 */
    connection_info_t* conn = allocate_connection();
    if (conn) {
        conn->client_id = msg->client_id;
        conn->handle = msg->handle;
        conn->connected_time = osKernelGetTickCount();
        
        LOG_INFSIG("[Echo] Client connected: ID=%d\\r\\n", msg->client_id);
        psa_reply(msg->handle, PSA_SUCCESS);
    } else {
        LOG_ERRSIG("[Echo] Failed to allocate connection\\r\\n");
        psa_reply(msg->handle, PSA_ERROR_INSUFFICIENT_MEMORY);
    }
}
```

### 다중 연산 서비스
```c
// partitions/tinymaix_inference/tinymaix_inference.c
static void handle_service_call(psa_msg_t *msg)
{
    uint32_t operation = msg->rhandle; /* 연산 타입 */
    psa_status_t status = PSA_SUCCESS;
    
    /* 연산별 분기 처리 */
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
    
    /* 응답 전송 */
    psa_reply(msg->handle, status);
}

/* 모델 로드 연산 처리 */
static psa_status_t handle_load_model(psa_msg_t *msg)
{
    tfm_tinymaix_status_t result = TINYMAIX_STATUS_SUCCESS;
    
    /* 입력 데이터 읽기 (필요한 경우) */
    if (msg->in_size[0] > 0) {
        uint8_t input_buffer[256];
        size_t bytes_read = psa_read(msg->handle, 0, input_buffer, sizeof(input_buffer));
        /* 입력 데이터 처리... */
    }
    
    /* 실제 모델 로드 로직 */
    result = perform_model_loading();
    
    /* 결과 전송 */
    psa_write(msg->handle, 0, &result, sizeof(result));
    
    return PSA_SUCCESS;
}
```

### 비동기 처리 패턴
```c
/* 비동기 작업을 위한 큐 시스템 */
typedef struct async_task {
    uint32_t task_id;
    uint32_t operation;
    psa_handle_t client_handle;
    uint8_t* input_data;
    size_t input_size;
    uint32_t timestamp;
    enum task_status status;
} async_task_t;

static async_task_t g_task_queue[MAX_ASYNC_TASKS];
static uint32_t g_next_task_id = 1;

/* 비동기 작업 시작 */
static psa_status_t start_async_operation(psa_msg_t *msg)
{
    /* 작업 큐에 추가 */
    async_task_t* task = allocate_task_slot();
    if (!task) {
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }
    
    task->task_id = g_next_task_id++;
    task->operation = msg->rhandle;
    task->client_handle = msg->handle;
    task->timestamp = osKernelGetTickCount();
    task->status = TASK_STATUS_PENDING;
    
    /* 입력 데이터 복사 */
    if (msg->in_size[0] > 0) {
        task->input_data = malloc(msg->in_size[0]);
        if (task->input_data) {
            task->input_size = psa_read(msg->handle, 0, task->input_data, msg->in_size[0]);
        }
    }
    
    /* 작업 ID 반환 */
    psa_write(msg->handle, 0, &task->task_id, sizeof(task->task_id));
    
    LOG_INFSIG("[Service] Async task started: ID=%d\\r\\n", task->task_id);
    
    return PSA_SUCCESS;
}

/* 비동기 작업 상태 확인 */
static psa_status_t check_async_status(psa_msg_t *msg)
{
    uint32_t task_id;
    
    /* 작업 ID 읽기 */
    size_t bytes_read = psa_read(msg->handle, 0, &task_id, sizeof(task_id));
    if (bytes_read != sizeof(task_id)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* 작업 찾기 */
    async_task_t* task = find_task_by_id(task_id);
    if (!task) {
        return PSA_ERROR_DOES_NOT_EXIST;
    }
    
    /* 상태 정보 구조체 */
    typedef struct {
        enum task_status status;
        uint32_t progress_percent;
        uint32_t estimated_time_remaining;
    } task_status_info_t;
    
    task_status_info_t status_info = {
        .status = task->status,
        .progress_percent = calculate_progress(task),
        .estimated_time_remaining = estimate_remaining_time(task)
    };
    
    /* 상태 정보 전송 */
    psa_write(msg->handle, 0, &status_info, sizeof(status_info));
    
    return PSA_SUCCESS;
}
```

## PSA 벡터 처리

### 입력 벡터 (psa_invec_t)
```c
/* 단일 입력 벡터 */
void simple_input_example(void)
{
    const char* message = "Hello, Secure World!";
    
    psa_invec_t in_vec[] = {
        {message, strlen(message)}
    };
    
    psa_call(handle, OPERATION_ECHO, in_vec, 1, NULL, 0);
}

/* 다중 입력 벡터 */
void multiple_input_example(void)
{
    uint32_t operation_type = NEURAL_NETWORK_INFERENCE;
    uint8_t model_data[1024] = {/* ... */};
    uint8_t input_image[784] = {/* MNIST 28x28 이미지 */};
    
    psa_invec_t in_vec[] = {
        {&operation_type, sizeof(operation_type)},  /* 연산 타입 */
        {model_data, sizeof(model_data)},           /* 모델 데이터 */
        {input_image, sizeof(input_image)}          /* 입력 이미지 */
    };
    
    psa_call(handle, OPERATION_ML_INFERENCE, in_vec, 3, NULL, 0);
}

/* 서비스에서 입력 벡터 읽기 */
static psa_status_t handle_multiple_inputs(psa_msg_t *msg)
{
    uint32_t operation_type;
    uint8_t model_buffer[1024];
    uint8_t input_buffer[784];
    
    /* 각 입력 벡터 개별 읽기 */
    if (msg->in_size[0] == sizeof(operation_type)) {
        psa_read(msg->handle, 0, &operation_type, sizeof(operation_type));
    }
    
    if (msg->in_size[1] > 0 && msg->in_size[1] <= sizeof(model_buffer)) {
        psa_read(msg->handle, 1, model_buffer, msg->in_size[1]);
    }
    
    if (msg->in_size[2] == sizeof(input_buffer)) {
        psa_read(msg->handle, 2, input_buffer, sizeof(input_buffer));
    }
    
    /* 처리 로직... */
    return PSA_SUCCESS;
}
```

### 출력 벡터 (psa_outvec_t)
```c
/* 단일 출력 벡터 */
void simple_output_example(void)
{
    char response_buffer[256];
    
    psa_outvec_t out_vec[] = {
        {response_buffer, sizeof(response_buffer)}
    };
    
    psa_status_t status = psa_call(handle, OPERATION_GET_INFO, NULL, 0, out_vec, 1);
    
    if (status == PSA_SUCCESS) {
        printf("Service response: %s\\n", response_buffer);
    }
}

/* 구조화된 출력 */
void structured_output_example(void)
{
    typedef struct {
        tfm_tinymaix_status_t status;
        int predicted_class;
        float confidence_scores[10];
        uint32_t inference_time_ms;
    } inference_result_t;
    
    inference_result_t result;
    
    psa_outvec_t out_vec[] = {
        {&result, sizeof(result)}
    };
    
    psa_status_t status = psa_call(handle, TINYMAIX_IPC_RUN_INFERENCE, 
                                  NULL, 0, out_vec, 1);
    
    if (status == PSA_SUCCESS && result.status == TINYMAIX_STATUS_SUCCESS) {
        printf("Predicted class: %d\\n", result.predicted_class);
        printf("Confidence: %.2f%%\\n", result.confidence_scores[result.predicted_class] * 100);
        printf("Inference time: %d ms\\n", result.inference_time_ms);
    }
}

/* 서비스에서 출력 벡터 쓰기 */
static psa_status_t handle_get_inference_result(psa_msg_t *msg)
{
    inference_result_t result = {
        .status = TINYMAIX_STATUS_SUCCESS,
        .predicted_class = g_last_prediction,
        .inference_time_ms = g_last_inference_time
    };
    
    /* 신뢰도 점수 복사 */
    memcpy(result.confidence_scores, g_output_probabilities, 
           sizeof(result.confidence_scores));
    
    /* 결과 전송 */
    psa_write(msg->handle, 0, &result, sizeof(result));
    
    return PSA_SUCCESS;
}
```

## 고급 PSA 패턴

### 연결 풀링 (Connection Pooling)
```c
/* 연결 풀 관리 */
typedef struct connection_pool {
    psa_handle_t handles[MAX_POOLED_CONNECTIONS];
    bool in_use[MAX_POOLED_CONNECTIONS];
    uint32_t last_used[MAX_POOLED_CONNECTIONS];
    uint32_t service_sid;
} connection_pool_t;

static connection_pool_t g_tinymaix_pool = {
    .service_sid = TFM_TINYMAIX_SERVICE_SID
};

/* 연결 풀에서 핸들 가져오기 */
static psa_handle_t get_pooled_connection(uint32_t service_sid)
{
    connection_pool_t* pool = find_pool_by_sid(service_sid);
    if (!pool) return PSA_NULL_HANDLE;
    
    /* 사용 가능한 연결 찾기 */
    for (int i = 0; i < MAX_POOLED_CONNECTIONS; i++) {
        if (!pool->in_use[i]) {
            /* 기존 연결이 있으면 재사용 */
            if (pool->handles[i] > 0) {
                pool->in_use[i] = true;
                pool->last_used[i] = osKernelGetTickCount();
                return pool->handles[i];
            }
            
            /* 새 연결 생성 */
            psa_handle_t handle = psa_connect(service_sid, 1);
            if (handle > 0) {
                pool->handles[i] = handle;
                pool->in_use[i] = true;
                pool->last_used[i] = osKernelGetTickCount();
                return handle;
            }
        }
    }
    
    return PSA_NULL_HANDLE; /* 사용 가능한 슬롯 없음 */
}

/* 연결을 풀에 반환 */
static void return_pooled_connection(psa_handle_t handle)
{
    for (int i = 0; i < MAX_POOLED_CONNECTIONS; i++) {
        if (g_tinymaix_pool.handles[i] == handle) {
            g_tinymaix_pool.in_use[i] = false;
            g_tinymaix_pool.last_used[i] = osKernelGetTickCount();
            break;
        }
    }
}

/* 풀링을 사용한 호출 */
tfm_tinymaix_status_t pooled_tinymaix_call(uint32_t operation,
                                          const psa_invec_t* in_vec,
                                          size_t in_len,
                                          psa_outvec_t* out_vec,
                                          size_t out_len)
{
    psa_handle_t handle = get_pooled_connection(TFM_TINYMAIX_SERVICE_SID);
    if (handle <= 0) {
        return TINYMAIX_STATUS_ERROR_INIT;
    }
    
    psa_status_t status = psa_call(handle, operation, in_vec, in_len, out_vec, out_len);
    
    return_pooled_connection(handle);
    
    return (status == PSA_SUCCESS) ? TINYMAIX_STATUS_SUCCESS : TINYMAIX_STATUS_ERROR;
}
```

### 배치 처리 최적화
```c
/* 배치 추론 요청 구조체 */
typedef struct batch_inference_request {
    uint32_t batch_size;
    uint32_t input_size_per_item;
    uint8_t batch_data[];  /* batch_size * input_size_per_item 바이트 */
} batch_inference_request_t;

typedef struct batch_inference_result {
    tfm_tinymaix_status_t status;
    uint32_t processed_count;
    int predicted_classes[MAX_BATCH_SIZE];
    float confidence_scores[MAX_BATCH_SIZE];
    uint32_t total_time_ms;
} batch_inference_result_t;

/* 배치 추론 클라이언트 API */
tfm_tinymaix_status_t tfm_tinymaix_batch_inference(
    const uint8_t* input_batch,
    uint32_t batch_size,
    uint32_t input_size_per_item,
    int* predicted_classes,
    float* confidence_scores)
{
    psa_handle_t handle;
    psa_status_t status;
    
    /* 배치 요청 구조체 생성 */
    size_t request_size = sizeof(batch_inference_request_t) + 
                         (batch_size * input_size_per_item);
    batch_inference_request_t* request = malloc(request_size);
    if (!request) {
        return TINYMAIX_STATUS_ERROR_INSUFFICIENT_MEMORY;
    }
    
    request->batch_size = batch_size;
    request->input_size_per_item = input_size_per_item;
    memcpy(request->batch_data, input_batch, batch_size * input_size_per_item);
    
    /* PSA 호출 */
    handle = psa_connect(TFM_TINYMAIX_SERVICE_SID, 1);
    if (handle <= 0) {
        free(request);
        return TINYMAIX_STATUS_ERROR_INIT;
    }
    
    psa_invec_t in_vec[] = {
        {request, request_size}
    };
    
    batch_inference_result_t result;
    psa_outvec_t out_vec[] = {
        {&result, sizeof(result)}
    };
    
    status = psa_call(handle, TINYMAIX_IPC_BATCH_INFERENCE, in_vec, 1, out_vec, 1);
    
    psa_close(handle);
    free(request);
    
    /* 결과 복사 */
    if (status == PSA_SUCCESS && result.status == TINYMAIX_STATUS_SUCCESS) {
        memcpy(predicted_classes, result.predicted_classes, 
               batch_size * sizeof(int));
        memcpy(confidence_scores, result.confidence_scores, 
               batch_size * sizeof(float));
        return TINYMAIX_STATUS_SUCCESS;
    }
    
    return result.status;
}

/* 서비스에서 배치 처리 */
static psa_status_t handle_batch_inference(psa_msg_t *msg)
{
    batch_inference_request_t* request;
    batch_inference_result_t result = {0};
    uint32_t start_time, end_time;
    
    /* 요청 데이터 읽기 */
    size_t request_size = msg->in_size[0];
    request = malloc(request_size);
    if (!request) {
        result.status = TINYMAIX_STATUS_ERROR_INSUFFICIENT_MEMORY;
        goto reply;
    }
    
    psa_read(msg->handle, 0, request, request_size);
    
    /* 배치 크기 검증 */
    if (request->batch_size > MAX_BATCH_SIZE) {
        result.status = TINYMAIX_STATUS_ERROR_INPUT_INVALID;
        goto cleanup;
    }
    
    LOG_INFSIG("[TinyMaix] Processing batch: %d items\\r\\n", request->batch_size);
    
    start_time = osKernelGetTickCount();
    
    /* 배치 내 각 항목 처리 */
    for (uint32_t i = 0; i < request->batch_size; i++) {
        const uint8_t* input_data = &request->batch_data[i * request->input_size_per_item];
        
        /* 개별 추론 실행 */
        tm_stat_t tm_status = tm_run(&g_model, input_data, g_output_buffer);
        if (tm_status != TM_OK) {
            LOG_ERRSIG("[TinyMaix] Batch item %d failed: %d\\r\\n", i, tm_status);
            break;
        }
        
        /* 결과 저장 */
        result.predicted_classes[i] = find_max_class(g_output_buffer, 10);
        result.confidence_scores[i] = g_output_buffer[result.predicted_classes[i]];
        result.processed_count++;
    }
    
    end_time = osKernelGetTickCount();
    result.total_time_ms = end_time - start_time;
    
    if (result.processed_count == request->batch_size) {
        result.status = TINYMAIX_STATUS_SUCCESS;
        LOG_INFSIG("[TinyMaix] Batch completed: %d/%d items in %d ms\\r\\n", 
                   result.processed_count, request->batch_size, result.total_time_ms);
    } else {
        result.status = TINYMAIX_STATUS_ERROR_INFERENCE_FAILED;
        LOG_ERRSIG("[TinyMaix] Batch partially failed: %d/%d items\\r\\n",
                   result.processed_count, request->batch_size);
    }
    
cleanup:
    free(request);
    
reply:
    /* 결과 전송 */
    psa_write(msg->handle, 0, &result, sizeof(result));
    
    return PSA_SUCCESS;
}
```

## PSA 암호화 API

### 대칭 키 암호화
```c
/* AES-128-CBC 암호화 예제 */
psa_status_t encrypt_data_aes_cbc(const uint8_t* plaintext,
                                 size_t plaintext_size,
                                 const uint8_t* key,
                                 uint8_t* ciphertext,
                                 size_t ciphertext_buffer_size,
                                 size_t* ciphertext_size)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;
    psa_status_t status;
    
    /* PSA Crypto 초기화 */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS && status != PSA_ERROR_ALREADY_EXISTS) {
        return status;
    }
    
    /* AES-128 키 속성 설정 */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    
    /* 키 가져오기 */
    status = psa_import_key(&attributes, key, 16, &key_id);
    if (status != PSA_SUCCESS) {
        return status;
    }
    
    /* 암호화 실행 */
    status = psa_cipher_encrypt(key_id, PSA_ALG_CBC_NO_PADDING,
                               plaintext, plaintext_size,
                               ciphertext, ciphertext_buffer_size, ciphertext_size);
    
    /* 키 정리 */
    psa_destroy_key(key_id);
    
    return status;
}

/* 해시 계산 */
psa_status_t compute_sha256_hash(const uint8_t* input,
                                size_t input_size,
                                uint8_t* hash,
                                size_t hash_buffer_size,
                                size_t* hash_size)
{
    psa_status_t status;
    
    /* PSA Crypto 초기화 */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS && status != PSA_ERROR_ALREADY_EXISTS) {
        return status;
    }
    
    /* SHA-256 해시 계산 */
    status = psa_hash_compute(PSA_ALG_SHA_256,
                             input, input_size,
                             hash, hash_buffer_size, hash_size);
    
    return status;
}
```

### HUK 키 파생
```c
/* HUK에서 애플리케이션별 키 파생 */
psa_status_t derive_application_key(const char* application_label,
                                   const char* key_purpose,
                                   uint8_t* derived_key,
                                   size_t key_size)
{
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t status;
    char full_label[128];
    
    /* 전체 라벨 생성 */
    snprintf(full_label, sizeof(full_label), 
             "pico2w-%s-%s-v1.0", application_label, key_purpose);
    
    /* HKDF-SHA256 설정 */
    status = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (status != PSA_SUCCESS) return status;
    
    /* HUK를 입력 키로 사용 */
    status = psa_key_derivation_input_key(&op, PSA_KEY_DERIVATION_INPUT_SECRET,
                                         TFM_BUILTIN_KEY_ID_HUK);
    if (status != PSA_SUCCESS) goto cleanup;
    
    /* 라벨을 정보로 사용 */
    status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO,
                                           (const uint8_t*)full_label, strlen(full_label));
    if (status != PSA_SUCCESS) goto cleanup;
    
    /* 파생 키 생성 */
    status = psa_key_derivation_output_bytes(&op, derived_key, key_size);
    
cleanup:
    psa_key_derivation_abort(&op);
    return status;
}

/* 사용 예 */
void example_key_derivation(void)
{
    uint8_t model_key[16];
    uint8_t storage_key[32];
    uint8_t auth_key[16];
    
    /* 다양한 목적의 키 파생 */
    derive_application_key("tinymaix", "model-encryption", model_key, 16);
    derive_application_key("storage", "data-encryption", storage_key, 32);
    derive_application_key("auth", "session-key", auth_key, 16);
}
```

## 성능 최적화

### PSA 호출 최적화
```c
/* PSA 호출 성능 측정 */
typedef struct psa_call_stats {
    uint32_t total_calls;
    uint32_t total_time_ms;
    uint32_t min_time_ms;
    uint32_t max_time_ms;
    uint32_t error_count;
} psa_call_stats_t;

static psa_call_stats_t g_call_stats = {0};

/* 성능 측정이 포함된 PSA 호출 래퍼 */
static psa_status_t measured_psa_call(psa_handle_t handle,
                                     uint32_t operation,
                                     const psa_invec_t* in_vec,
                                     size_t in_len,
                                     psa_outvec_t* out_vec,
                                     size_t out_len)
{
    uint32_t start_time = osKernelGetTickCount();
    
    psa_status_t status = psa_call(handle, operation, in_vec, in_len, out_vec, out_len);
    
    uint32_t end_time = osKernelGetTickCount();
    uint32_t call_time = end_time - start_time;
    
    /* 통계 업데이트 */
    g_call_stats.total_calls++;
    g_call_stats.total_time_ms += call_time;
    
    if (g_call_stats.min_time_ms == 0 || call_time < g_call_stats.min_time_ms) {
        g_call_stats.min_time_ms = call_time;
    }
    
    if (call_time > g_call_stats.max_time_ms) {
        g_call_stats.max_time_ms = call_time;
    }
    
    if (status != PSA_SUCCESS) {
        g_call_stats.error_count++;
    }
    
    return status;
}

/* 성능 통계 출력 */
void print_psa_performance_stats(void)
{
    if (g_call_stats.total_calls == 0) return;
    
    uint32_t avg_time = g_call_stats.total_time_ms / g_call_stats.total_calls;
    
    printf("=== PSA Call Performance Stats ===\\n");
    printf("Total calls: %d\\n", g_call_stats.total_calls);
    printf("Average time: %d ms\\n", avg_time);
    printf("Min time: %d ms\\n", g_call_stats.min_time_ms);
    printf("Max time: %d ms\\n", g_call_stats.max_time_ms);
    printf("Error rate: %.1f%%\\n", 
           (float)g_call_stats.error_count * 100.0f / g_call_stats.total_calls);
}
```

### 메모리 최적화
```c
/* 제로 카피 PSA 통신 */
typedef struct zero_copy_buffer {
    uint8_t* data;
    size_t size;
    size_t capacity;
    bool is_shared;
} zero_copy_buffer_t;

/* 공유 메모리 영역 설정 */
static uint8_t g_shared_memory[SHARED_MEMORY_SIZE] __attribute__((aligned(32)));
static bool g_shared_memory_in_use = false;

/* 제로 카피 버퍼 할당 */
static zero_copy_buffer_t* allocate_zero_copy_buffer(size_t required_size)
{
    if (g_shared_memory_in_use || required_size > SHARED_MEMORY_SIZE) {
        return NULL;
    }
    
    zero_copy_buffer_t* buffer = malloc(sizeof(zero_copy_buffer_t));
    if (!buffer) return NULL;
    
    buffer->data = g_shared_memory;
    buffer->size = 0;
    buffer->capacity = SHARED_MEMORY_SIZE;
    buffer->is_shared = true;
    
    g_shared_memory_in_use = true;
    
    return buffer;
}

/* 제로 카피 PSA 호출 */
static psa_status_t zero_copy_psa_call(psa_handle_t handle,
                                      uint32_t operation,
                                      zero_copy_buffer_t* buffer)
{
    /* 공유 메모리를 직접 PSA 벡터로 사용 */
    psa_invec_t in_vec[] = {
        {buffer->data, buffer->size}
    };
    
    psa_outvec_t out_vec[] = {
        {buffer->data, buffer->capacity}
    };
    
    psa_status_t status = psa_call(handle, operation, in_vec, 1, out_vec, 1);
    
    /* 출력 크기 업데이트 */
    if (status == PSA_SUCCESS) {
        buffer->size = out_vec[0].len;
    }
    
    return status;
}
```

## 에러 처리 및 디버깅

### PSA 에러 코드 분석
```c
/* PSA 에러 코드를 문자열로 변환 */
const char* psa_error_to_string(psa_status_t status)
{
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
        case PSA_ERROR_INSUFFICIENT_MEMORY: return "INSUFFICIENT_MEMORY";
        case PSA_ERROR_INSUFFICIENT_STORAGE: return "INSUFFICIENT_STORAGE";
        case PSA_ERROR_DOES_NOT_EXIST: return "DOES_NOT_EXIST";
        case PSA_ERROR_ALREADY_EXISTS: return "ALREADY_EXISTS";
        case PSA_ERROR_CORRUPTION_DETECTED: return "CORRUPTION_DETECTED";
        case PSA_ERROR_INVALID_SIGNATURE: return "INVALID_SIGNATURE";
        default: return "UNKNOWN_ERROR";
    }
}

/* 상세한 에러 리포팅 */
void report_psa_error(const char* function_name,
                     psa_status_t status,
                     const char* additional_info)
{
    LOG_ERRSIG("PSA Error in %s: %s (%d)\\r\\n", 
               function_name, psa_error_to_string(status), status);
    
    if (additional_info) {
        LOG_ERRSIG("Additional info: %s\\r\\n", additional_info);
    }
    
    /* 에러별 복구 제안 */
    switch (status) {
        case PSA_ERROR_CONNECTION_REFUSED:
            LOG_ERRSIG("Suggestion: Check if service is enabled and running\\r\\n");
            break;
            
        case PSA_ERROR_INVALID_HANDLE:
            LOG_ERRSIG("Suggestion: Verify psa_connect() was successful\\r\\n");
            break;
            
        case PSA_ERROR_BUFFER_TOO_SMALL:
            LOG_ERRSIG("Suggestion: Increase buffer size or check data size\\r\\n");
            break;
            
        case PSA_ERROR_INSUFFICIENT_MEMORY:
            LOG_ERRSIG("Suggestion: Reduce memory usage or increase partition stack\\r\\n");
            break;
    }
}

/* 사용 예 */
void example_error_handling(void)
{
    psa_handle_t handle = psa_connect(TFM_TINYMAIX_SERVICE_SID, 1);
    if (handle <= 0) {
        report_psa_error("psa_connect", handle, "TinyMaix service connection failed");
        return;
    }
    
    psa_status_t status = psa_call(handle, TINYMAIX_IPC_LOAD_MODEL, NULL, 0, NULL, 0);
    if (status != PSA_SUCCESS) {
        report_psa_error("psa_call", status, "Model loading operation failed");
    }
    
    psa_close(handle);
}
```

### PSA 통신 디버깅
```c
#ifdef DEBUG_PSA_CALLS
/* PSA 호출 추적을 위한 디버그 매크로 */
#define PSA_CALL_TRACE(handle, op, in_len, out_len) \\
    do { \\
        LOG_INFSIG("[PSA] Call: handle=%d, op=%d, in_len=%d, out_len=%d\\r\\n", \\
                   handle, op, in_len, out_len); \\
    } while(0)

#define PSA_CONNECT_TRACE(sid, version, result) \\
    do { \\
        LOG_INFSIG("[PSA] Connect: SID=0x%08x, version=%d, handle=%d\\r\\n", \\
                   sid, version, result); \\
    } while(0)

#define PSA_CLOSE_TRACE(handle) \\
    do { \\
        LOG_INFSIG("[PSA] Close: handle=%d\\r\\n", handle); \\
    } while(0)

/* 디버그가 포함된 PSA 함수 래퍼 */
static psa_handle_t debug_psa_connect(uint32_t sid, uint32_t version)
{
    psa_handle_t handle = psa_connect(sid, version);
    PSA_CONNECT_TRACE(sid, version, handle);
    return handle;
}

static psa_status_t debug_psa_call(psa_handle_t handle,
                                  uint32_t operation,
                                  const psa_invec_t* in_vec,
                                  size_t in_len,
                                  psa_outvec_t* out_vec,
                                  size_t out_len)
{
    PSA_CALL_TRACE(handle, operation, in_len, out_len);
    
    psa_status_t status = psa_call(handle, operation, in_vec, in_len, out_vec, out_len);
    
    LOG_INFSIG("[PSA] Call result: %s (%d)\\r\\n", psa_error_to_string(status), status);
    
    return status;
}

static void debug_psa_close(psa_handle_t handle)
{
    PSA_CLOSE_TRACE(handle);
    psa_close(handle);
}

#else
/* 릴리스 빌드에서는 일반 PSA 함수 사용 */
#define debug_psa_connect   psa_connect
#define debug_psa_call      psa_call
#define debug_psa_close     psa_close
#endif
```

## 다음 단계

PSA API를 마스터했다면 다음 문서를 참조하세요:

- **[보안 파티션](./secure-partitions.md)** - PSA 서비스 구현 심화
- **[TinyMaix 통합](./tinymaix-integration.md)** - PSA를 활용한 ML 서비스
- **[테스트 프레임워크](./testing-framework.md)** - PSA 호출 테스트 작성
- **[문제 해결](./troubleshooting.md)** - PSA 관련 문제 해결