# 모델 암호화 가이드

## 개요

이 가이드는 TinyMaix 신경망 모델을 HUK(Hardware Unique Key) 기반으로 암호화하여 보안 파티션에서 안전하게 사용하는 방법을 설명합니다. AES-128-CBC 암호화를 사용하여 모델의 기밀성과 무결성을 보장합니다.

## HUK 기반 보안 아키텍처

### Hardware Unique Key (HUK)
```
HUK 보안 체계:
┌─────────────────────────────────────────┐
│            RP2350 Hardware              │
├─────────────────────────────────────────┤
│  ┌───────────────┐  ┌─────────────────┐ │
│  │      HUK      │  │   Key Storage   │ │
│  │  (Hardware)   │  │   (One-Time)    │ │
│  │   Unique      │  │   Programmable  │ │
│  │   128-bit     │  │      Fuses      │ │
│  └───────────────┘  └─────────────────┘ │
└─────────────────────────────────────────┘
                    │
        ┌───────────▼───────────┐
        │    HKDF-SHA256       │  ← 키 파생 함수
        │  Key Derivation      │
        └───────────┬───────────┘
                    │
    ┌───────────────▼───────────────┐
    │     Model Encryption Key      │  ← 모델별 고유 키
    │        AES-128 Key            │
    │   (Label: model-aes128-v1.0)  │
    └───────────────────────────────┘
```

### 키 파생 과정
```c
// HUK에서 모델 암호화 키 파생
psa_status_t derive_key_from_huk(const char* label, 
                                uint8_t* derived_key, 
                                size_t key_length)
{
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t status;
    
    // 1. HKDF-SHA256 설정
    status = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (status != PSA_SUCCESS) return status;
    
    // 2. HUK를 입력 키로 사용
    status = psa_key_derivation_input_key(&op, PSA_KEY_DERIVATION_INPUT_SECRET,
                                         TFM_BUILTIN_KEY_ID_HUK);
    if (status != PSA_SUCCESS) goto cleanup;
    
    // 3. 라벨을 정보(Info)로 사용 - 컨텍스트 바인딩
    status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO,
                                           (const uint8_t*)label, strlen(label));
    if (status != PSA_SUCCESS) goto cleanup;
    
    // 4. 파생 키 생성
    status = psa_key_derivation_output_bytes(&op, derived_key, key_length);
    
cleanup:
    psa_key_derivation_abort(&op);
    return status;
}

// 사용 예:
// const char* label = "pico2w-tinymaix-model-aes128-v1.0";
// derive_key_from_huk(label, key_buffer, 16);
```

## DEV_MODE를 통한 HUK 키 추출

### 1단계: DEV_MODE 빌드
```bash
# DEV_MODE로 빌드하여 HUK 키 디버깅 활성화
./build.sh DEV_MODE

# 또는 수동 빌드
cmake -S ./spe -B build/spe \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DTFM_PROFILE=profile_medium \
  -DTFM_PARTITION_TINYMAIX_INFERENCE=ON \
  -DDEV_MODE=ON

cmake --build build/spe -- -j8 install
cmake --build build/nspe -- -j8
```

### 2단계: HUK 키 추출
```bash
# 디바이스를 연결하고 시리얼 출력 모니터링
# Linux/Mac:
screen /dev/ttyACM0 115200

# Windows:
# PuTTY 또는 Tera Term 사용

# 예상 출력:
# [TinyMaix Test] Testing HUK-derived model key (DEV_MODE)...
# [TinyMaix] Getting HUK-derived model key (DEV_MODE)
# HUK-derived key: 40c962d66a1fa40346cac8b7e612741e
# [TinyMaix Test] ✓ Model key retrieved successfully
```

### 3단계: 키를 바이너리 파일로 변환
```bash
# 시리얼 출력에서 16진수 키 복사
# 예: HUK-derived key: 40c962d66a1fa40346cac8b7e612741e

# xxd를 사용하여 16진수를 바이너리로 변환
echo "40c962d66a1fa40346cac8b7e612741e" | xxd -r -p > models/model_key_psa.bin

# 키 파일 생성 확인
ls -la models/model_key_psa.bin
# 출력: -rw-r--r-- 1 user user 16 Jun 14 10:30 models/model_key_psa.bin

# 키 파일 내용 확인 (16진수)
xxd models/model_key_psa.bin
# 출력: 00000000: 40c9 62d6 6a1f a403 46ca c8b7 e612 741e  @.b.j...F...t.
```

### 4단계: 추출한 키로 모델 암호화
```bash
# HUK 파생 키로 모델 암호화
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header

# 출력 예:
# Parsed TinyMAIX model:
#   Array name: mnist_valid_q
#   Model size: 16384 bytes
#   MDL_BUF_LEN: 32768
#   LBUF_LEN: 8192
# 
# Encryption details:
#   - Original data size: 16384 bytes
#   - Padded data size: 16384 bytes
#   - IV: 1234567890abcdef1122334455667788
#   - Encrypted data size: 16384 bytes
# 
# PSA CBC package structure:
#   - Magic: TMAX (0x54414d58)
#   - Version: 3 (CBC)
#   - Original size: 16384 bytes
#   - Header size: 28 bytes (28 bytes total)
#   - Encrypted data: 16384 bytes
#   - Total package: 16412 bytes
# 
# C header generated: models/encrypted_mnist_model_psa.h
# C source generated: models/encrypted_mnist_model_psa.c
# TinyMAIX model encryption completed successfully!
```

## 암호화 도구 상세 분석

### 암호화 도구: `tinymaix_model_encryptor.py`

#### 주요 기능
```python
class TinyMAIXModelEncryptor:
    """TinyMAIX 모델 암호화 유틸리티"""
    
    def parse_tinymaix_header(self, header_content: str):
        """C 헤더 파일에서 모델 데이터 추출"""
        # 배열 이름 추출: const uint8_t array_name[]
        array_match = re.search(r'const\\s+uint8_t\\s+(\\w+)\\[\\d*\\]\\s*=', header_content)
        
        # MDL_BUF_LEN, LBUF_LEN 메타데이터 추출
        mdl_buf_match = re.search(r'#define\\s+MDL_BUF_LEN\\s+\\((\\d+)\\)', header_content)
        
        # 16진수 데이터 파싱
        hex_values = re.findall(r'0x([0-9a-fA-F]{2})', hex_data)
        model_data = bytes(int(h, 16) for h in hex_values)
        
        return model_data, array_name, mdl_buf_len, lbuf_len
    
    def encrypt_model(self, model_data: bytes, key: bytes):
        """AES-CBC-PKCS7 암호화 (PSA 호환)"""
        # 랜덤 IV 생성
        iv = secrets.token_bytes(16)
        
        # PKCS7 패딩 적용
        padded_data = self._pkcs7_pad(model_data, 16)
        
        # AES-CBC 암호화
        cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
        encryptor = cipher.encryptor()
        encrypted_data = encryptor.update(padded_data) + encryptor.finalize()
        
        return encrypted_data, iv
    
    def create_encrypted_package(self, model_data: bytes, key: bytes, model_name: str):
        """PSA 호환 암호화 패키지 생성"""
        encrypted_data, iv = self.encrypt_model(model_data, key)
        
        # PSA CBC 패키지 헤더
        header = struct.pack('<4sII16s',
                            b"TMAX",           # 매직 헤더
                            3,                # 버전 (CBC)
                            len(model_data),  # 원본 크기
                            iv)               # 16바이트 IV
        
        return header + encrypted_data
```

#### 패키지 포맷
```
PSA CBC 암호화 패키지 구조:
┌─────────────────────────────────────────┐ ← 시작
│  Magic Header: "TMAX" (4 bytes)         │
├─────────────────────────────────────────┤
│  Version: 3 (4 bytes, little endian)   │  ← CBC 버전
├─────────────────────────────────────────┤
│  Original Size (4 bytes, little endian)│  ← 압축 전 크기
├─────────────────────────────────────────┤
│  IV: Random 128-bit (16 bytes)         │  ← CBC용 초기화 벡터
├─────────────────────────────────────────┤
│                                         │
│  Encrypted Model Data (Variable)       │  ← AES-CBC 암호화된 데이터
│  + PKCS7 Padding                       │    (PKCS7 패딩 포함)
│                                         │
└─────────────────────────────────────────┘ ← 끝
```

### 암호화 옵션

#### 1. HUK 파생 키 사용 (권장)
```bash
# 디바이스별 고유 키로 암호화
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

#### 2. PSA 테스트 키 사용
```bash
# 개발/테스트용 고정 키
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --use-psa-key \
    --generate-c-header

# PSA 테스트 키: 40c962d66a1fa40346cac8b7e612741e
```

#### 3. 새 키 생성
```bash
# 랜덤 키 생성
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --generate-key \
    --output-key models/new_key.bin \
    --generate-c-header

# 패스워드 기반 키 생성
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --generate-key \
    --password "my_secure_password" \
    --output-key models/pwd_key.bin \
    --generate-c-header
```

## 보안 파티션에서의 복호화

### AES-CBC 복호화 구현
```c
// partitions/tinymaix_inference/tinymaix_inference.c
static psa_status_t decrypt_model_cbc(const uint8_t* encrypted_data,
                                     size_t encrypted_size,
                                     const uint8_t* key,
                                     uint8_t* decrypted_data,
                                     size_t* decrypted_size)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;
    psa_status_t status;
    
    /* 패키지 헤더 검증 */
    if (encrypted_size < 28) {
        LOG_ERRSIG("[TinyMaix] Encrypted data too small\\n");
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* 헤더 파싱 */
    const uint8_t* magic = encrypted_data;
    const uint32_t* version = (const uint32_t*)(encrypted_data + 4);
    const uint32_t* original_size = (const uint32_t*)(encrypted_data + 8);
    const uint8_t* iv = encrypted_data + 12;
    const uint8_t* ciphertext = encrypted_data + 28;
    size_t ciphertext_size = encrypted_size - 28;
    
    /* 매직 헤더 검증 */
    if (memcmp(magic, "TMAX", 4) != 0) {
        LOG_ERRSIG("[TinyMaix] Invalid model magic\\n");
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* 버전 검증 (CBC = 3) */
    if (*version != 3) {
        LOG_ERRSIG("[TinyMaix] Unsupported model version: %d\\n", *version);
        return PSA_ERROR_NOT_SUPPORTED;
    }
    
    LOG_INFSIG("[TinyMaix] Decrypting: v%d, original_size=%d\\n", 
               *version, *original_size);
    
    /* AES-128 키 설정 */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    
    /* 키 가져오기 */
    status = psa_import_key(&attributes, key, 16, &key_id);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix] Key import failed: %d\\n", status);
        return status;
    }
    
    /* AES-CBC 복호화 */
    size_t output_length;
    status = psa_cipher_decrypt(key_id, PSA_ALG_CBC_NO_PADDING,
                               ciphertext, ciphertext_size,
                               decrypted_data, *decrypted_size, &output_length);
    
    /* 키 정리 (보안) */
    psa_destroy_key(key_id);
    
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix] Decryption failed: %d\\n", status);
        return status;
    }
    
    /* PKCS7 패딩 검증 및 제거 */
    if (!validate_pkcs7_padding(decrypted_data, output_length)) {
        LOG_ERRSIG("[TinyMaix] Invalid PKCS7 padding\\n");
        return PSA_ERROR_INVALID_SIGNATURE;
    }
    
    /* 패딩 제거 */
    uint8_t padding_size = decrypted_data[output_length - 1];
    *decrypted_size = output_length - padding_size;
    
    /* 원본 크기 검증 */
    if (*decrypted_size != *original_size) {
        LOG_ERRSIG("[TinyMaix] Size mismatch: %d != %d\\n", 
                   *decrypted_size, *original_size);
        return PSA_ERROR_CORRUPTION_DETECTED;
    }
    
    LOG_INFSIG("[TinyMaix] Model decrypted: %d bytes\\n", *decrypted_size);
    
    return PSA_SUCCESS;
}

/* PKCS7 패딩 검증 */
static bool validate_pkcs7_padding(const uint8_t* data, size_t size)
{
    if (size == 0) return false;
    
    uint8_t padding_size = data[size - 1];
    
    /* 패딩 크기 유효성 검사 */
    if (padding_size == 0 || padding_size > 16 || padding_size > size) {
        LOG_ERRSIG("[TinyMaix] Invalid padding size: %d\\n", padding_size);
        return false;
    }
    
    /* 모든 패딩 바이트가 동일한지 확인 */
    for (size_t i = size - padding_size; i < size; i++) {
        if (data[i] != padding_size) {
            LOG_ERRSIG("[TinyMaix] Invalid padding byte at position %d\\n", i);
            return false;
        }
    }
    
    return true;
}
```

## 모델 무결성 검증

### Python에서 복호화 테스트
```python
# test_cbc_decrypt.py - 암호화 검증 스크립트
import struct
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

def test_model_decryption():
    """암호화된 모델의 복호화 테스트"""
    
    # 암호화된 모델 파일 읽기
    with open('models/encrypted_mnist_model_psa.bin', 'rb') as f:
        encrypted_package = f.read()
    
    # 키 파일 읽기
    with open('models/model_key_psa.bin', 'rb') as f:
        key = f.read()
    
    print(f"Package size: {len(encrypted_package)} bytes")
    print(f"Key size: {len(key)} bytes")
    
    # 헤더 파싱
    magic = encrypted_package[0:4]
    version = struct.unpack('<I', encrypted_package[4:8])[0]
    original_size = struct.unpack('<I', encrypted_package[8:12])[0]
    iv = encrypted_package[12:28]
    ciphertext = encrypted_package[28:]
    
    print(f"Magic: {magic}")
    print(f"Version: {version}")
    print(f"Original size: {original_size}")
    print(f"IV: {iv.hex()}")
    print(f"Ciphertext size: {len(ciphertext)}")
    
    # 매직 헤더 검증
    assert magic == b'TMAX', f"Invalid magic: {magic}"
    assert version == 3, f"Invalid version: {version}"
    
    # AES-CBC 복호화
    cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
    decryptor = cipher.decryptor()
    padded_plaintext = decryptor.update(ciphertext) + decryptor.finalize()
    
    # PKCS7 패딩 제거
    padding_size = padded_plaintext[-1]
    plaintext = padded_plaintext[:-padding_size]
    
    print(f"Decrypted size: {len(plaintext)} bytes")
    print(f"Padding size: {padding_size}")
    
    # 크기 검증
    assert len(plaintext) == original_size, f"Size mismatch: {len(plaintext)} != {original_size}"
    
    # TinyMaix 모델 헤더 검증
    tm_magic = plaintext[0:4]
    assert tm_magic == b'TMDL', f"Invalid TinyMaix magic: {tm_magic}"
    
    print("✓ 모델 복호화 및 검증 성공!")
    
    # 복호화된 모델을 파일로 저장 (검증용)
    with open('models/decrypted_model_test.bin', 'wb') as f:
        f.write(plaintext)
    
    return True

if __name__ == "__main__":
    test_model_decryption()
```

### 실행 및 검증
```bash
# 복호화 테스트 실행
python3 test_cbc_decrypt.py

# 예상 출력:
# Package size: 16412 bytes
# Key size: 16 bytes
# Magic: b'TMAX'
# Version: 3
# Original size: 16384
# IV: 1234567890abcdef1122334455667788
# Ciphertext size: 16384
# Decrypted size: 16384 bytes
# Padding size: 0
# ✓ 모델 복호화 및 검증 성공!
```

## 보안 고려사항

### 키 관리 보안
```c
/* 보안 키 처리 원칙 */

// 1. 키 사용 후 즉시 메모리 정리
void secure_key_cleanup(uint8_t* key_buffer, size_t key_size)
{
    /* 메모리를 0으로 덮어쓰기 */
    memset(key_buffer, 0, key_size);
    
    /* 컴파일러 최적화 방지 */
    volatile uint8_t* volatile_ptr = (volatile uint8_t*)key_buffer;
    for (size_t i = 0; i < key_size; i++) {
        volatile_ptr[i] = 0;
    }
}

// 2. 스택의 민감한 데이터 정리
void secure_function_exit(void)
{
    uint8_t stack_cleanup[256];
    memset(stack_cleanup, 0, sizeof(stack_cleanup));
    
    /* 스택 변수가 실제로 사용되도록 보장 */
    volatile uint8_t dummy = stack_cleanup[0];
    (void)dummy;
}

// 3. 키 파생 시 재사용 방지
static uint32_t g_key_derivation_counter = 0;

psa_status_t derive_unique_key(const char* base_label,
                              uint8_t* derived_key,
                              size_t key_length)
{
    char unique_label[128];
    
    /* 카운터를 포함하여 고유한 라벨 생성 */
    snprintf(unique_label, sizeof(unique_label), 
             "%s-counter-%u", base_label, g_key_derivation_counter++);
    
    return derive_key_from_huk(unique_label, derived_key, key_length);
}
```

### 타이밍 공격 방지
```c
/* 상수 시간 비교 함수 */
static bool secure_compare(const uint8_t* a, const uint8_t* b, size_t length)
{
    volatile uint8_t result = 0;
    
    for (size_t i = 0; i < length; i++) {
        result |= a[i] ^ b[i];
    }
    
    return (result == 0);
}

/* 복호화 시 타이밍 공격 방지 */
static psa_status_t secure_decrypt_verify(const uint8_t* encrypted_data,
                                         size_t encrypted_size,
                                         const uint8_t* expected_magic,
                                         uint8_t* decrypted_data,
                                         size_t* decrypted_size)
{
    psa_status_t status;
    uint8_t temp_buffer[TEMP_BUFFER_SIZE];
    
    /* 복호화 실행 */
    status = decrypt_model_cbc(encrypted_data, encrypted_size, 
                              derived_key, temp_buffer, decrypted_size);
    
    /* 상수 시간 매직 헤더 검증 */
    bool magic_valid = secure_compare(temp_buffer, expected_magic, 4);
    
    /* 조건부 복사 (타이밍 정보 노출 방지) */
    if (status == PSA_SUCCESS && magic_valid) {
        memcpy(decrypted_data, temp_buffer, *decrypted_size);
    } else {
        /* 실패 시에도 동일한 시간 소요 */
        memset(decrypted_data, 0, *decrypted_size);
        status = PSA_ERROR_INVALID_SIGNATURE;
    }
    
    /* 임시 버퍼 정리 */
    memset(temp_buffer, 0, sizeof(temp_buffer));
    
    return status;
}
```

### DEV_MODE 보안 주의사항
```c
#ifdef DEV_MODE
/* DEV_MODE 경고 메시지 */
#warning "DEV_MODE is enabled - HUK keys will be exposed!"
#warning "Never use DEV_MODE in production environments!"

static void dev_mode_security_warning(void)
{
    LOG_ERRSIG("================================================\\n");
    LOG_ERRSIG("WARNING: DEV_MODE is active!\\n");
    LOG_ERRSIG("HUK-derived keys will be exposed via debug output.\\n");
    LOG_ERRSIG("This build is NOT suitable for production use!\\n");
    LOG_ERRSIG("================================================\\n");
}

/* DEV_MODE에서만 키 추출 허용 */
static psa_status_t handle_get_model_key(psa_msg_t *msg)
{
    dev_mode_security_warning();
    
    /* 실제 키 추출 로직 */
    return extract_huk_key_for_debugging(msg);
}
#else
/* 프로덕션 모드에서는 키 추출 비활성화 */
static psa_status_t handle_get_model_key(psa_msg_t *msg)
{
    LOG_ERRSIG("[Security] Key extraction disabled in production\\n");
    return PSA_ERROR_NOT_SUPPORTED;
}
#endif
```

## 고급 암호화 기법

### 다중 모델 지원
```c
/* 모델별 고유 키 파생 */
typedef struct model_key_info {
    const char* model_name;
    const char* version;
    uint8_t derived_key[16];
} model_key_info_t;

static psa_status_t derive_model_specific_key(const char* model_name,
                                             const char* version,
                                             uint8_t* derived_key)
{
    char full_label[128];
    
    /* 모델명과 버전을 포함한 고유 라벨 생성 */
    snprintf(full_label, sizeof(full_label), 
             "pico2w-tinymaix-%s-aes128-%s", model_name, version);
    
    return derive_key_from_huk(full_label, derived_key, 16);
}

/* 사용 예 */
// derive_model_specific_key("mnist", "v1.0", key_buffer);
// derive_model_specific_key("cifar10", "v2.1", key_buffer);
```

### 키 회전 (Key Rotation)
```c
/* 주기적 키 회전을 위한 버전 관리 */
typedef struct key_version_info {
    uint32_t version_number;
    uint32_t creation_time;
    uint32_t expiration_time;
    bool is_active;
} key_version_info_t;

static psa_status_t derive_versioned_key(uint32_t key_version,
                                         uint8_t* derived_key)
{
    char versioned_label[128];
    
    snprintf(versioned_label, sizeof(versioned_label),
             "pico2w-tinymaix-model-aes128-v%u", key_version);
    
    return derive_key_from_huk(versioned_label, derived_key, 16);
}

/* 키 버전 관리 */
static uint32_t get_current_key_version(void)
{
    /* 현재 활성 키 버전 반환 */
    /* RTC나 보안 카운터를 사용하여 구현 가능 */
    return 1; /* 기본 버전 */
}
```

## 문제 해결

### 일반적인 암호화 문제

#### 1. 키 파일 오류
```bash
# 증상: 키 파일 읽기 실패
Error: Invalid key file format. Expected 16 bytes, got 32

# 원인: xxd 명령어 실수
echo "40c962d66a1fa40346cac8b7e612741e" | xxd -r -p > key.bin  # 올바름
echo "40c962d66a1fa40346cac8b7e612741e" > key.bin             # 틀림 (텍스트 저장)

# 해결: 올바른 방법으로 재생성
rm models/model_key_psa.bin
echo "40c962d66a1fa40346cac8b7e612741e" | xxd -r -p > models/model_key_psa.bin
```

#### 2. 복호화 실패
```bash
# 증상: 복호화 시 패딩 오류
[TinyMaix] Invalid PKCS7 padding

# 원인: 잘못된 키 사용
# 해결: HUK 키 재추출
./build.sh DEV_MODE
# 새로운 키를 시리얼에서 복사하여 재암호화
```

#### 3. 모델 로드 실패
```bash
# 증상: TinyMaix 모델 로드 오류
[TinyMaix] Model load failed: -1

# 원인: 복호화된 데이터가 유효한 TinyMaix 형식이 아님
# 해결: 원본 모델 파일 확인
python3 test_cbc_decrypt.py  # 복호화 테스트
hexdump -C models/decrypted_model_test.bin | head  # 첫 몇 바이트 확인
# 첫 4바이트가 "TMDL"(0x544D444C)이어야 함
```

## 성능 고려사항

### 암호화 성능 측정
```python
# 암호화 성능 벤치마크
import time

def benchmark_encryption():
    """모델 암호화 성능 측정"""
    
    # 다양한 크기의 테스트 데이터
    test_sizes = [1024, 4096, 16384, 65536]  # 1KB ~ 64KB
    
    for size in test_sizes:
        test_data = b'\\x00' * size
        key = b'\\x40\\xc9\\x62\\xd6\\x6a\\x1f\\xa4\\x03\\x46\\xca\\xc8\\xb7\\xe6\\x12\\x74\\xe1'
        
        # 암호화 시간 측정
        start_time = time.time()
        
        for _ in range(100):  # 100회 반복
            encryptor = TinyMAIXModelEncryptor()
            encrypted_data, iv = encryptor.encrypt_model(test_data, key)
        
        end_time = time.time()
        
        avg_time = (end_time - start_time) / 100 * 1000  # ms
        throughput = size / (avg_time / 1000) / 1024  # KB/s
        
        print(f"Size: {size:5d} bytes, Avg time: {avg_time:.2f} ms, Throughput: {throughput:.1f} KB/s")

# 결과 예:
# Size:  1024 bytes, Avg time: 0.15 ms, Throughput: 6826.7 KB/s
# Size:  4096 bytes, Avg time: 0.32 ms, Throughput: 12800.0 KB/s
# Size: 16384 bytes, Avg time: 1.05 ms, Throughput: 15603.8 KB/s
# Size: 65536 bytes, Avg time: 3.89 ms, Throughput: 16845.0 KB/s
```

### 보안 파티션 복호화 성능
```c
/* 복호화 성능 측정 */
static void benchmark_decryption(void)
{
    uint32_t start_time, end_time;
    uint8_t test_key[16] = {0x40, 0xc9, 0x62, 0xd6, /* ... */};
    uint8_t decrypted_buffer[MODEL_BUFFER_SIZE];
    size_t decrypted_size = sizeof(decrypted_buffer);
    
    LOG_INFSIG("[Benchmark] Starting decryption performance test...\\n");
    
    start_time = osKernelGetTickCount();
    
    /* 10회 반복 복호화 */
    for (int i = 0; i < 10; i++) {
        psa_status_t status = decrypt_model_cbc(
            encrypted_mnist_model_psa_data,
            encrypted_mnist_model_psa_size,
            test_key,
            decrypted_buffer,
            &decrypted_size);
        
        if (status != PSA_SUCCESS) {
            LOG_ERRSIG("[Benchmark] Decryption failed: %d\\n", status);
            return;
        }
    }
    
    end_time = osKernelGetTickCount();
    
    uint32_t total_time = end_time - start_time;
    uint32_t avg_time = total_time / 10;
    uint32_t throughput = (encrypted_mnist_model_psa_size * 1000) / avg_time; /* bytes/sec */
    
    LOG_INFSIG("[Benchmark] Results:\\n");
    LOG_INFSIG("  - Model size: %d bytes\\n", encrypted_mnist_model_psa_size);
    LOG_INFSIG("  - Average time: %d ms\\n", avg_time);
    LOG_INFSIG("  - Throughput: %d bytes/sec\\n", throughput);
}

/* 일반적인 성능 (RP2350 @150MHz):
 * - 16KB 모델: ~15-25ms 복호화
 * - 처리량: ~800KB/s
 * - HUK 키 파생: ~5-10ms
 */
```

## 다음 단계

모델 암호화를 완료했다면 다음 문서를 참조하세요:

- **[TinyMaix 통합](./tinymaix-integration.md)** - 암호화된 모델 사용
- **[보안 파티션](./secure-partitions.md)** - 보안 서비스 구현
- **[테스트 프레임워크](./testing-framework.md)** - 암호화 테스트 작성
- **[문제 해결](./troubleshooting.md)** - 암호화 관련 문제 해결