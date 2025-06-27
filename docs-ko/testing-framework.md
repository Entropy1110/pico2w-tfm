# 테스트 프레임워크 가이드

## 개요

테스트 프레임워크는 TF-M 보안 파티션, PSA API 기능, TinyMaix 신경망 추론에 대한 포괄적인 검증을 제공합니다. 테스트는 디바이스 부팅 시 자동으로 실행되며 UART/USB 시리얼을 통해 상세한 피드백을 제공합니다.

## 테스트 아키텍처

### 테스트 구조
```
nspe/
├── test_runner.c              # 메인 테스트 관리자
├── echo_test_app.c            # Echo 서비스 테스트
├── psa_crypto_test.c          # PSA 암호화 API 테스트  
├── tinymaix_inference_test.c  # TinyMaix ML 테스트
└── app.h                      # 테스트 함수 선언
```

### 테스트 실행 흐름
```
디바이스 부팅 → RTOS 시작 → 테스트 실행기 → 개별 테스트 → 시리얼 출력
```

## 테스트 실행기

### 메인 테스트 컨트롤러: `test_runner.c`

#### DEV_MODE vs 프로덕션 테스트
```c
void run_all_tests(void* arg)
{
    UNUSED_VARIABLE(arg);
    
#ifdef DEV_MODE
    LOG_MSG("TF-M 테스트 시작 (DEV_MODE)...\\r\\n");
    LOG_MSG("DEV_MODE: HUK 파생 모델 키 테스트만 실행됩니다\\r\\n");
    
    /* DEV_MODE: HUK 키 파생 테스트만 실행 */
    test_tinymaix_comprehensive_suite();
    
    LOG_MSG("DEV_MODE 테스트 완료!\\r\\n");
#else
    LOG_MSG("TF-M 테스트 시작 (프로덕션 모드)...\\r\\n");
    
    /* 프로덕션 모드: DEV_MODE 특정 테스트를 제외한 모든 테스트 실행 */
    test_echo_service();
    test_psa_encryption(); 
    test_psa_hash();
    test_tinymaix_comprehensive_suite();
    
    LOG_MSG("모든 프로덕션 테스트 완료!\\r\\n");
#endif
}
```

### RTOS 통합
테스트는 RTOS 컨텍스트에서 적절한 작업 관리와 함께 실행됩니다:
```c
/* main_ns.c에서 */
osThreadNew(run_all_tests, NULL, &app_task_attr);
```

## 테스트 카테고리

### 1. Echo 서비스 테스트

#### 기본 Echo 기능
```c
void test_echo_service(void)
{
    printf("[Echo 테스트] Echo 서비스 테스트 중...\\n");
    
    tfm_echo_status_t status;
    const char* test_string = "안녕하세요, 보안 세계!";
    char output_buffer[256];
    
    /* 기본 에코 테스트 */
    status = tfm_echo_call((uint8_t*)test_string, strlen(test_string),
                          (uint8_t*)output_buffer, sizeof(output_buffer));
    
    if (status == ECHO_STATUS_SUCCESS) {
        printf("[Echo 테스트] ✓ 에코 성공: %s\\n", output_buffer);
    } else {
        printf("[Echo 테스트] ✗ 에코 실패: %d\\n", status);
    }
}
```

### 2. PSA 암호화 테스트

#### AES-128 암호화 테스트
```c
void test_psa_encryption(void)
{
    printf("[PSA 암호화 테스트] AES-128 암호화 테스트 중...\\n");
    
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;
    
    /* PSA crypto 초기화 */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS && status != PSA_ERROR_ALREADY_EXISTS) {
        printf("[PSA 암호화 테스트] ✗ 초기화 실패: %d\\n", status);
        return;
    }
    
    /* AES-128 키 생성 */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CTR);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    
    status = psa_generate_key(&attributes, &key_id);
    if (status == PSA_SUCCESS) {
        printf("[PSA 암호화 테스트] ✓ AES-128 키 생성됨\\n");
        
        /* 암호화/복호화 테스트 */
        test_aes_encrypt_decrypt(key_id);
        
        /* 정리 */
        psa_destroy_key(key_id);
    } else {
        printf("[PSA 암호화 테스트] ✗ 키 생성 실패: %d\\n", status);
    }
}
```

#### SHA-256 해시 테스트
```c
void test_psa_hash(void)
{
    printf("[PSA 해시 테스트] SHA-256 테스트 중...\\n");
    
    psa_status_t status;
    const char* message = "안녕하세요, TF-M 세계!";
    uint8_t hash[PSA_HASH_LENGTH(PSA_ALG_SHA_256)];
    size_t hash_length;
    
    /* SHA-256 해시 계산 */
    status = psa_hash_compute(PSA_ALG_SHA_256,
                             (const uint8_t*)message, strlen(message),
                             hash, sizeof(hash), &hash_length);
    
    if (status == PSA_SUCCESS) {
        printf("[PSA 해시 테스트] ✓ SHA-256 계산됨 (%d 바이트)\\n", hash_length);
        
        /* 해시 출력 (처음 8바이트) */
        printf("[PSA 해시 테스트] 해시: ");
        for (int i = 0; i < 8; i++) {
            printf("%02x", hash[i]);
        }
        printf("...\\n");
    } else {
        printf("[PSA 해시 테스트] ✗ 해시 실패: %d\\n", status);
    }
}
```

### 3. TinyMaix 추론 테스트

#### 프로덕션 모드 테스트
```c
void test_tinymaix_basic_functionality(void)
{
    printf("[TinyMaix 테스트] 암호화된 모델 기능 테스트 중...\\n");
    
    tfm_tinymaix_status_t status;
    int predicted_class = -1;
    
    /* 테스트 1: 암호화된 모델 로드 */
    printf("[TinyMaix 테스트] 1. 내장 암호화 MNIST 모델 로딩 중...\\n");
    status = tfm_tinymaix_load_encrypted_model();
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix 테스트] ✓ 암호화된 모델 로드 성공\\n");
    } else {
        printf("[TinyMaix 테스트] ✗ 모델 로드 실패: %d\\n", status);
        return;
    }
    
    /* 테스트 2: 내장 이미지 추론 */
    printf("[TinyMaix 테스트] 2. 내장 이미지로 추론 실행 중...\\n");
    status = tfm_tinymaix_run_inference(&predicted_class);
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix 테스트] ✓ 내장 추론 성공\\n");
        printf("[TinyMaix 테스트] ✓ 예측된 숫자: %d\\n", predicted_class);
    } else {
        printf("[TinyMaix 테스트] ✗ 내장 추론 실패: %d\\n", status);
        return;
    }
    
    /* 테스트 3: 커스텀 이미지 추론 */
    printf("[TinyMaix 테스트] 3. 커스텀 이미지로 추론 실행 중...\\n");
    
    uint8_t custom_image[28*28];
    create_test_digit_pattern(custom_image, 7); /* 숫자 \"7\" 생성 */
    
    status = tfm_tinymaix_run_inference_with_data(custom_image, sizeof(custom_image), 
                                                  &predicted_class);
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix 테스트] ✓ 커스텀 추론 성공\\n");
        printf("[TinyMaix 테스트] ✓ 예측된 숫자: %d\\n", predicted_class);
    } else {
        printf("[TinyMaix 테스트] ✗ 커스텀 추론 실패: %d\\n", status);
    }
}
```

#### DEV_MODE 테스트
```c
#ifdef DEV_MODE
void test_tinymaix_get_model_key(void)
{
    printf("[TinyMaix 테스트] HUK 파생 모델 키 테스트 (DEV_MODE)...\\n");
    
    tfm_tinymaix_status_t status;
    uint8_t key_buffer[16]; /* 128비트 키 */
    
    /* 테스트 1: 모델 키 가져오기 */
    printf("[TinyMaix 테스트] 1. HUK 파생 모델 키 가져오는 중...\\n");
    status = tfm_tinymaix_get_model_key(key_buffer, sizeof(key_buffer));
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix 테스트] ✓ 모델 키 검색 성공\\n");
        printf("[TinyMaix 테스트] ✓ 키 (16진수): ");
        for (int i = 0; i < 16; i++) {
            printf("%02x", key_buffer[i]);
        }
        printf("\\n");
    } else {
        printf("[TinyMaix 테스트] ✗ 모델 키 가져오기 실패: %d\\n", status);
        return;
    }
    
    /* 테스트 2: 키 일관성 검증 */
    printf("[TinyMaix 테스트] 2. 키 일관성 검증 중...\\n");
    uint8_t key_buffer2[16];
    status = tfm_tinymaix_get_model_key(key_buffer2, sizeof(key_buffer2));
    if (status == TINYMAIX_STATUS_SUCCESS) {
        int keys_match = (memcmp(key_buffer, key_buffer2, 16) == 0);
        if (keys_match) {
            printf("[TinyMaix 테스트] ✓ 키 파생이 일관됨\\n");
        } else {
            printf("[TinyMaix 테스트] ✗ 키 파생이 일관되지 않음\\n");
        }
    }
    
    /* 테스트 3: 잘못된 매개변수 */
    printf("[TinyMaix 테스트] 3. 잘못된 매개변수 테스트 중...\\n");
    status = tfm_tinymaix_get_model_key(NULL, 16);
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix 테스트] ✓ NULL 버퍼 올바르게 거부됨\\n");
    }
    
    status = tfm_tinymaix_get_model_key(key_buffer, 8); /* 너무 작음 */
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("[TinyMaix 테스트] ✓ 작은 버퍼 올바르게 거부됨\\n");
    }
}
#endif
```

## 테스트 유틸리티

### MNIST 패턴 생성
```c
static void create_test_digit_pattern(uint8_t* image, int digit)
{
    memset(image, 0, 28*28);
    
    switch (digit) {
        case 0:
            /* \"0\"을 위한 원 패턴 그리기 */
            for (int y = 8; y < 20; y++) {
                for (int x = 10; x < 18; x++) {
                    if ((y == 8 || y == 19) || (x == 10 || x == 17)) {
                        image[y * 28 + x] = 255;
                    }
                }
            }
            break;
            
        case 1:
            /* \"1\"을 위한 수직선 그리기 */
            for (int y = 5; y < 23; y++) {
                image[y * 28 + 14] = 255;
            }
            break;
            
        case 7:
            /* \"7\" 패턴 그리기 */
            for (int x = 10; x < 18; x++) {
                image[8 * 28 + x] = 255; /* 상단 선 */
            }
            for (int y = 8; y < 20; y++) {
                image[y * 28 + 17] = 255; /* 수직선 */
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

### 테스트 결과 검증
```c
static bool validate_inference_result(int predicted_class, int expected_class)
{
    if (predicted_class < 0 || predicted_class > 9) {
        printf("[검증] ✗ 잘못된 예측: %d (범위 초과)\\n", predicted_class);
        return false;
    }
    
    if (predicted_class == expected_class) {
        printf("[검증] ✓ 예측이 예상과 일치: %d\\n", predicted_class);
        return true;
    } else {
        printf("[검증] ⚠ 예측 불일치: %d를 얻었지만 %d가 예상됨\\n", 
               predicted_class, expected_class);
        return false; /* 일부 테스트 패턴에서는 허용 가능할 수 있음 */
    }
}
```

## 테스트 출력 및 로깅

### 시리얼 출력 형식
테스트는 UART/USB를 통해 구조화된 로그를 출력합니다:
```
[컴포넌트 테스트] 작업 설명...
[컴포넌트 테스트] ✓ 성공 메시지
[컴포넌트 테스트] ✗ 오류 메시지: error_code
[컴포넌트 테스트] ⚠ 경고 메시지
```

### 예제 테스트 출력
```
TF-M 테스트 시작 (프로덕션 모드)...

[Echo 테스트] ===========================================
[Echo 테스트] Echo 서비스 테스트
[Echo 테스트] ===========================================
[Echo 테스트] ✓ 에코 성공: 안녕하세요, 보안 세계!

[PSA 암호화 테스트] ===========================================
[PSA 암호화 테스트] PSA 암호화 연산 테스트
[PSA 암호화 테스트] ===========================================
[PSA 암호화 테스트] ✓ AES-128 키 생성됨
[PSA 암호화 테스트] ✓ 암호화 성공 (16 바이트)
[PSA 암호화 테스트] ✓ 복호화 성공 (16 바이트)
[PSA 암호화 테스트] ✓ 데이터 무결성 검증됨

[PSA 해시 테스트] ===========================================
[PSA 해시 테스트] SHA-256 테스트
[PSA 해시 테스트] ===========================================
[PSA 해시 테스트] ✓ SHA-256 계산됨 (32 바이트)
[PSA 해시 테스트] 해시: 2c26b46b...

[TinyMaix 테스트] ###########################################
[TinyMaix 테스트] #     TinyMaix 암호화 모델 테스트 도구     #
[TinyMaix 테스트] #   암호화를 통한 MNIST 분류              #
[TinyMaix 테스트] ###########################################

[TinyMaix 테스트] 1. 내장 암호화 MNIST 모델 로딩 중...
[TinyMaix 테스트] ✓ 암호화된 모델 로드 성공
[TinyMaix 테스트] 2. 내장 이미지로 추론 실행 중...
[TinyMaix 테스트] ✓ 내장 추론 성공
[TinyMaix 테스트] ✓ 예측된 숫자: 2
[TinyMaix 테스트] 3. 커스텀 이미지로 추론 실행 중...
[TinyMaix 테스트] ✓ 커스텀 추론 성공
[TinyMaix 테스트] ✓ 예측된 숫자: 7

모든 프로덕션 테스트 완료!
```

## 새로운 테스트 추가

### 1단계: 테스트 함수 생성
```c
/* 적절한 테스트 파일에서 */
void test_my_new_feature(void)
{
    printf("[내 기능 테스트] 새 기능 테스트 중...\\n");
    
    /* 설정 */
    setup_test_environment();
    
    /* 테스트 실행 */
    int result = call_feature_under_test();
    
    /* 검증 */
    if (result == EXPECTED_VALUE) {
        printf("[내 기능 테스트] ✓ 테스트 통과\\n");
    } else {
        printf("[내 기능 테스트] ✗ 테스트 실패: %d를 얻었지만 %d가 예상됨\\n", 
               result, EXPECTED_VALUE);
    }
    
    /* 정리 */
    cleanup_test_environment();
}
```

### 2단계: 테스트 실행기에 추가
```c
/* test_runner.c에서 */
void run_all_tests(void* arg)
{
    /* ... 기존 테스트들 ... */
    
    /* 새 테스트 추가 */
    test_my_new_feature();
    
    /* ... */
}
```

### 3단계: 선언 추가
```c
/* app.h에서 */
void test_my_new_feature(void);
```

### 4단계: 빌드 업데이트
```cmake
# app_broker/CMakeLists.txt에서
target_sources(tfm_tflm_broker
    PRIVATE
        # ... 기존 소스들 ...
        ../nspe/my_new_feature_test.c
)
```

## 성능 테스트

### 타이밍 측정
```c
void test_performance_timing(void)
{
    printf("[성능 테스트] 작업 타이밍 측정 중...\\n");
    
    /* 작업 전 시스템 틱 가져오기 */
    uint32_t start_tick = osKernelGetTickCount();
    
    /* 테스트 대상 작업 실행 */
    perform_operation();
    
    /* 작업 후 시스템 틱 가져오기 */
    uint32_t end_tick = osKernelGetTickCount();
    
    /* 경과 시간 계산 */
    uint32_t elapsed_ms = end_tick - start_tick;
    printf("[성능 테스트] 작업에 걸린 시간: %d ms\\n", elapsed_ms);
    
    /* 타이밍 요구사항 검증 */
    if (elapsed_ms <= MAX_ALLOWED_TIME_MS) {
        printf("[성능 테스트] ✓ 타이밍 요구사항 충족\\n");
    } else {
        printf("[성능 테스트] ✗ 타이밍 요구사항 초과\\n");
    }
}
```

### 메모리 사용 테스트
```c
void test_memory_usage(void)
{
    printf("[메모리 테스트] 메모리 사용량 확인 중...\\n");
    
    /* 초기 메모리 통계 가져오기 */
    size_t initial_free = get_free_memory();
    
    /* 리소스 할당 */
    void* resource = allocate_test_resource();
    
    /* 메모리 소비 확인 */
    size_t after_alloc = get_free_memory();
    size_t consumed = initial_free - after_alloc;
    
    printf("[메모리 테스트] 소비된 메모리: %d 바이트\\n", consumed);
    
    /* 리소스 해제 */
    free_test_resource(resource);
    
    /* 정리 검증 */
    size_t final_free = get_free_memory();
    if (final_free == initial_free) {
        printf("[메모리 테스트] ✓ 메모리 누수 감지되지 않음\\n");
    } else {
        printf("[메모리 테스트] ✗ 메모리 누수: %d 바이트\\n", 
               initial_free - final_free);
    }
}
```

## 오류 테스트

### 잘못된 매개변수 테스트
```c
void test_error_handling(void)
{
    printf("[오류 테스트] 오류 처리 테스트 중...\\n");
    
    /* NULL 매개변수 테스트 */
    tfm_service_status_t status = tfm_service_call(NULL, 0, NULL, 0);
    if (status == SERVICE_ERROR_INVALID_PARAM) {
        printf("[오류 테스트] ✓ NULL 매개변수 거부됨\\n");
    } else {
        printf("[오류 테스트] ✗ NULL 매개변수 처리되지 않음\\n");
    }
    
    /* 크기 초과 입력 테스트 */
    uint8_t oversized_data[MAX_SIZE + 1];
    status = tfm_service_call(oversized_data, sizeof(oversized_data), NULL, 0);
    if (status == SERVICE_ERROR_BUFFER_TOO_LARGE) {
        printf("[오류 테스트] ✓ 크기 초과 입력 거부됨\\n");
    } else {
        printf("[오류 테스트] ✗ 크기 초과 입력 처리되지 않음\\n");
    }
}
```

### 스트레스 테스트
```c
void test_stress_operations(void)
{
    printf("[스트레스 테스트] 스트레스 테스트 실행 중...\\n");
    
    const int num_iterations = 100;
    int success_count = 0;
    
    for (int i = 0; i < num_iterations; i++) {
        tfm_service_status_t status = tfm_service_call(...);
        if (status == SERVICE_SUCCESS) {
            success_count++;
        }
        
        /* 작업 간 짧은 지연 */
        osDelay(10);
    }
    
    printf("[스트레스 테스트] 성공률: %d/%d (%.1f%%)\\n", 
           success_count, num_iterations, 
           (float)success_count * 100.0f / num_iterations);
    
    if (success_count == num_iterations) {
        printf("[스트레스 테스트] ✓ 모든 작업 성공\\n");
    } else {
        printf("[스트레스 테스트] ⚠ 일부 작업 실패\\n");
    }
}
```

## 테스트 구성

### 컴파일 시점 테스트 제어
```c
/* 특정 테스트 카테고리 활성화/비활성화 */
#define ENABLE_ECHO_TESTS        1
#define ENABLE_CRYPTO_TESTS      1
#define ENABLE_TINYMAIX_TESTS    1
#define ENABLE_PERFORMANCE_TESTS 0
#define ENABLE_STRESS_TESTS      0

#ifdef DEV_MODE
#define ENABLE_DEBUG_TESTS       1
#else
#define ENABLE_DEBUG_TESTS       0
#endif
```

### 런타임 테스트 선택
```c
void run_selected_tests(uint32_t test_mask)
{
    if (test_mask & TEST_ECHO) {
        test_echo_service();
    }
    
    if (test_mask & TEST_CRYPTO) {
        test_psa_encryption();
        test_psa_hash();
    }
    
    if (test_mask & TEST_TINYMAIX) {
        test_tinymaix_comprehensive_suite();
    }
}
```

## 테스트 디버깅

### 디버그 출력
```c
#ifdef DEBUG_TESTS
#define TEST_DEBUG(fmt, ...) printf(\"[테스트 디버그] \" fmt, ##__VA_ARGS__)
#else
#define TEST_DEBUG(fmt, ...)
#endif

void debug_test_function(void)
{
    TEST_DEBUG(\"테스트 함수 진입\\n\");
    TEST_DEBUG(\"매개변수 값: %d\\n\", param);
    /* ... 테스트 로직 ... */
    TEST_DEBUG(\"테스트 결과: %d\\n\", result);
}
```

### 테스트 격리
```c
void isolated_test(void)
{
    /* 시스템 상태 저장 */
    save_system_state();
    
    /* 테스트 실행 */
    run_test_logic();
    
    /* 시스템 상태 복원 */
    restore_system_state();
}
```

## 고급 테스트 패턴

### 테스트 도구 (Test Fixtures)
```c
/* 테스트 도구 구조체 */
typedef struct test_fixture {
    void* test_data;
    size_t data_size;
    bool initialized;
    uint32_t setup_time;
} test_fixture_t;

static test_fixture_t g_fixture;

/* 테스트 설정 */
static void setup_test_fixture(void)
{
    g_fixture.test_data = malloc(TEST_DATA_SIZE);
    g_fixture.data_size = TEST_DATA_SIZE;
    g_fixture.initialized = true;
    g_fixture.setup_time = osKernelGetTickCount();
    
    /* 테스트 데이터 초기화 */
    initialize_test_data(g_fixture.test_data, g_fixture.data_size);
}

/* 테스트 정리 */
static void teardown_test_fixture(void)
{
    if (g_fixture.test_data) {
        free(g_fixture.test_data);
        g_fixture.test_data = NULL;
    }
    g_fixture.initialized = false;
}

/* 도구를 사용한 테스트 */
void test_with_fixture(void)
{
    setup_test_fixture();
    
    /* 실제 테스트 실행 */
    if (g_fixture.initialized) {
        perform_test_with_data(g_fixture.test_data, g_fixture.data_size);
    }
    
    teardown_test_fixture();
}
```

### 매개변수화된 테스트
```c
/* 테스트 매개변수 구조체 */
typedef struct test_case {
    const char* name;
    int input_value;
    int expected_output;
    bool should_pass;
} test_case_t;

/* 테스트 케이스 배열 */
static const test_case_t arithmetic_test_cases[] = {
    {\"양수 더하기\", 5, 10, true},
    {\"0 더하기\", 0, 5, true},
    {\"음수 더하기\", -3, 2, true},
    {\"오버플로 케이스\", INT_MAX, INT_MAX, false},
};

/* 매개변수화된 테스트 실행 */
void test_arithmetic_operations(void)
{
    printf(\"[산술 테스트] 매개변수화된 테스트 실행 중...\\n\");
    
    size_t num_cases = sizeof(arithmetic_test_cases) / sizeof(arithmetic_test_cases[0]);
    int passed = 0, failed = 0;
    
    for (size_t i = 0; i < num_cases; i++) {
        const test_case_t* test_case = &arithmetic_test_cases[i];
        
        printf(\"[산술 테스트] 테스트 케이스: %s\\n\", test_case->name);
        
        int result = arithmetic_function(test_case->input_value);
        bool test_passed = (result == test_case->expected_output);
        
        if (test_passed == test_case->should_pass) {
            printf(\"[산술 테스트] ✓ %s 통과\\n\", test_case->name);
            passed++;
        } else {
            printf(\"[산술 테스트] ✗ %s 실패\\n\", test_case->name);
            failed++;
        }
    }
    
    printf(\"[산술 테스트] 결과: %d 통과, %d 실패\\n\", passed, failed);
}
```

### 테스트 도구 체인
```c
/* 테스트 결과 수집기 */
typedef struct test_results {
    uint32_t total_tests;
    uint32_t passed_tests;
    uint32_t failed_tests;
    uint32_t skipped_tests;
    uint32_t total_time_ms;
} test_results_t;

static test_results_t g_test_results = {0};

/* 테스트 시작 매크로 */
#define TEST_BEGIN(name) \\
    do { \\
        printf(\"[테스트] 시작: %s\\n\", name); \\
        uint32_t test_start_time = osKernelGetTickCount(); \\
        g_test_results.total_tests++;

/* 테스트 종료 매크로 */
#define TEST_END(name, passed) \\
        uint32_t test_end_time = osKernelGetTickCount(); \\
        uint32_t test_duration = test_end_time - test_start_time; \\
        g_test_results.total_time_ms += test_duration; \\
        if (passed) { \\
            printf(\"[테스트] ✓ %s 완료 (%d ms)\\n\", name, test_duration); \\
            g_test_results.passed_tests++; \\
        } else { \\
            printf(\"[테스트] ✗ %s 실패 (%d ms)\\n\", name, test_duration); \\
            g_test_results.failed_tests++; \\
        } \\
    } while(0)

/* 사용 예 */
void example_test_with_macros(void)
{
    TEST_BEGIN(\"예제 테스트\");
    
    bool test_passed = perform_some_test();
    
    TEST_END(\"예제 테스트\", test_passed);
}

/* 최종 테스트 결과 보고 */
void report_final_test_results(void)
{
    printf(\"\\n=== 최종 테스트 결과 ===\\n\");
    printf(\"총 테스트: %d\\n\", g_test_results.total_tests);
    printf(\"통과: %d\\n\", g_test_results.passed_tests);
    printf(\"실패: %d\\n\", g_test_results.failed_tests);
    printf(\"건너뜀: %d\\n\", g_test_results.skipped_tests);
    printf(\"총 시간: %d ms\\n\", g_test_results.total_time_ms);
    
    float pass_rate = (float)g_test_results.passed_tests * 100.0f / g_test_results.total_tests;
    printf(\"통과율: %.1f%%\\n\", pass_rate);
}
```

## 다음 단계

테스트 프레임워크를 마스터했다면 다음 문서를 참조하세요:

- **[보안 파티션](./secure-partitions.md)** - 커스텀 보안 서비스 테스트
- **[PSA API](./psa-api.md)** - PSA 클라이언트-서버 통신 테스트
- **[문제 해결](./troubleshooting.md)** - 실패한 테스트 디버깅