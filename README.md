# Pico W (RP2350) TF-M Project - TinyMaix Integration

[한국어](#한국어) | [English](#english)

## DEV DOCS
- 개발을 위한 문서는 [docs-ko](./docs-ko) 를 참조하세요.
- If you're looking for development documents, refer to [docs](./docs).

---

## 한국어

### 프로젝트 개요

이 프로젝트는 Raspberry Pi Pico 2W (RP2350)를 위한 **TinyMaix와 TF-M (Trusted Firmware-M) 통합** 시스템입니다. ARM TrustZone 기술을 사용하여 보안 분할 프로그래밍을 구현하며, 이중 세계 아키텍처(보안 및 비보안 처리 환경)를 통해 안전한 머신러닝 추론을 제공합니다. TinyMaix는 임베디드 시스템에 최적화된 경량 신경망 추론 라이브러리입니다.

### 주요 특징

- **보안 머신러닝**: ARM TrustZone을 사용한 Secure Partition에서 신경망 추론 실행
- **모델 암호화**: AES-256-CBC를 사용한 모델 데이터 보호
- **PSA API**: 표준화된 PSA (Platform Security Architecture) 프레임워크
- **이중 세계 아키텍처**: 보안 환경(SPE)과 비보안 환경(NSPE) 분리
- **실시간 테스트**: 장치 부팅 시 자동 테스트 실행

### 빌드 및 테스트 방법

#### 0. 프로젝트 클론 및 폴더 이동

```bash
git clone https://github.com/TZTZEN/pico2w-tfm-tinyml --recursive && cd pico2w-tfm-tinyml
```

#### 1. 개발 모드 빌드 (DEV_MODE) - HUK 키 추출

```bash
# 첫 번째 단계: DEV_MODE로 빌드하여 HUK 파생 키 추출
./build.sh DEV_MODE

# 시리얼 출력에서 "✓ Key (hex):" 줄을 찾아 16진수 키를 복사
# 예: ✓ Key (hex): 40c962d66a1fa40346cac8b7e612741e

# 추출한 키를 바이너리 파일로 저장 (16진수 → 바이너리 변환)
echo "40c962d66a1fa40346cac8b7e612741e" | xxd -r -p > models/model_key_psa.bin

# 또는 사용자 정의 이름으로 저장
echo "YOUR_HUK_KEY_HERE" | xxd -r -p > models/my_device_key.bin
```

#### 2. HUK 키를 사용한 모델 암호화

DEV_MODE에서 추출한 HUK 파생 키를 사용하여 모델을 암호화합니다:

```bash
# 추출한 HUK 키로 모델 암호화 (권장 방법)
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header

# 사용자 정의 키 파일 사용
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/my_device_key.bin \
    --generate-c-header
```

**중요**: 각 디바이스마다 고유한 HUK 키가 생성됩니다. 디바이스를 변경하면 새로운 HUK 키를 추출해야 합니다.

#### 3. 클린 빌드

```bash
./build.sh clean
```

#### 4. 빌드 실행 (캐시 포함)

```bash
./build.sh
```


개발 모드에서는 다음 디버그/키 추출 기능이 활성화됩니다:
- **HUK 파생 키 출력**: `tfm_tinymaix_get_model_key()` 함수를 통해 HUK에서 파생된 모델 암호화 키를 출력
- **추가 로깅**: 키 파생 과정에 대한 상세한 디버그 정보

### 아키텍처

#### 디렉터리 구조
- **`spe/`** - 보안 처리 환경 구성
- **`nspe/`** - 비보안 처리 환경 및 테스트 애플리케이션
- **`partitions/`** - 커스텀 Secure Partition (현재: echo_service, tinymaix_inference)
- **`app_broker/`** - 비보안 애플리케이션 브로커
- **`interface/`** - 보안/비보안 환경 간 API
- **`models/`** - 암호화된 TinyMaix 모델 파일
- **`tools/`** - 모델 암호화 도구

#### Secure Partition 서비스
- **Echo Service** (PID: 444, SID: 0x00000105) - 연결 기반 에코 기능
- **TinyMaix Inference Service** (PID: 445, SID: 0x00000107) - TinyMaix 신경망 추론 엔진

### 모델 관리

#### 모델 암호화
모델은 보안 배포를 위해 AES-128-CBC로 암호화됩니다. `./build.sh` 실행 시 자동으로 HUK(Hardware Unique Key) 파생 키를 사용하여 암호화됩니다.

**수동 암호화 방법:**
```bash
# 기존 키 파일 사용
python3 tools/tinymaix_model_encryptor.py --input models/mnist_valid_q.h --output models/encrypted_mnist_model_psa.bin --key-file models/model_key_psa.bin --generate-c-header

# PSA 테스트 키 사용
python3 tools/tinymaix_model_encryptor.py --input models/mnist_valid_q.h --output models/encrypted_model.bin --use-psa-key --generate-c-header

# 새 암호화 키 생성
python3 tools/tinymaix_model_encryptor.py --input models/mnist_valid_q.h --output models/encrypted_model.bin --generate-key --output-key models/new_key.bin --generate-c-header

# 패스워드 기반 키 생성
python3 tools/tinymaix_model_encryptor.py --input models/mnist_valid_q.h --output models/encrypted_model.bin --generate-key --password "your_password" --output-key models/pwd_key.bin --generate-c-header
```

#### 복호화 테스트
암호화된 모델의 정확성을 Python으로 검증할 수 있습니다:
```bash
python3 test_cbc_decrypt.py
```

#### 모델 형식
- **입력**: 모델 데이터 배열이 포함된 TinyMaix C 헤더 파일 (.h)
- **출력**: CBC 암호화된 바이너리 패키지 (.bin)
- **키**: 별도 키 파일에 저장된 128비트 키
- **통합**: 임베디드 배포를 위한 C 헤더/소스 생성

### 테스트

테스트 애플리케이션은 `nspe/`에 위치하며 Secure Partition 사용법을 보여줍니다:
- **`echo_test_app.c`** - 에코 서비스 기능 테스트
- **`psa_crypto_test.c`** - PSA 암호화 연산 테스트 (AES-128, SHA-256)
- **`tinymaix_inference_test.c`** - TinyMaix 추론 서비스 기능 테스트
- **`test_runner.c`** - RTOS 컨텍스트에서 모든 테스트 조율

테스트는 장치 부팅 시 자동으로 실행되며 UART/USB 시리얼을 통해 결과를 출력합니다.

### 수동 빌드 단계

고급 사용자를 위한 수동 빌드 방법:

```bash
# SPE 빌드
cmake -S ./spe -B build/spe \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DTFM_PROFILE=profile_medium \
  -DTFM_PARTITION_ECHO_SERVICE=ON \
  -DTFM_PARTITION_TINYMAIX_INFERENCE=ON

cmake --build build/spe -- -j8 install

# NSPE 빌드  
cmake -S ./nspe -B build/nspe \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DCONFIG_SPE_PATH=build/spe/api_ns

cmake --build build/nspe -- -j8

# UF2 파일 생성
./pico_uf2.sh . ./build

# Pico에 배포
picotool erase && picotool load build/spe/bin/bl2.uf2 && picotool load build/spe/bin/tfm_s_ns_signed.uf2 && picotool reboot
```

### 개발 환경 요구사항

#### 하드웨어
- **개발 보드**: RP2350 마이크로컨트롤러를 탑재한 Raspberry Pi Pico 2W
- **툴체인**: GNU ARM (`GNUARM`)
- **배포**: picotool을 통한 UF2 형식

#### 소프트웨어 의존성
- **Python 패키지**: [pico2w-trusted-firmware-m/tools/requirements.txt](./pico2w-trusted-firmware-m/tools/requirements.txt) 참조
- **picotool**: [https://github.com/raspberrypi/picotool](https://github.com/raspberrypi/picotool) - Raspberry Pi Pico 배포 도구
- **Python 3.x**: 모델 암호화 및 테스트 스크립트용
- **CMake**: 빌드 시스템

---

## English

### Project Overview

This project is a **TinyMaix integration with TF-M (Trusted Firmware-M)** for the **Raspberry Pi Pico 2W (RP2350)**. It demonstrates secure partition programming using ARM TrustZone technology with a dual-world architecture (secure and non-secure processing environments) for safe machine learning inference. TinyMaix is a lightweight neural network inference library optimized for embedded systems.

### Key Features

- **Secure Machine Learning**: Neural network inference execution in secure partitions using ARM TrustZone
- **Model Encryption**: Model data protection using AES-256-CBC encryption
- **PSA API**: Standardized PSA (Platform Security Architecture) framework
- **Dual-World Architecture**: Separation of Secure (SPE) and Non-Secure (NSPE) Processing Environments
- **Real-time Testing**: Automatic test execution on device boot

### Build and Test Instructions

#### 0. Clone and Navigate to Project Folder

```bash
git clone https://github.com/TZTZEN/pico2w-tfm-tinyml --recursive && cd pico2w-tfm-tinyml
```

#### 1. Build Only (with caches)

```bash
./build.sh
```

#### 2. Clean Build

```bash
./build.sh clean
```

#### 3. Development Mode Build (DEV_MODE)

```bash
./build.sh DEV_MODE
```

Development mode enables the following debug features:
- **HUK-derived Key Output**: Output model encryption key derived from HUK via `tfm_tinymaix_get_model_key()` function
- **Additional Logging**: Detailed debug information about key derivation process

### Architecture

#### Directory Structure
- **`spe/`** - Secure Processing Environment configuration
- **`nspe/`** - Non-Secure Processing Environment with test applications
- **`partitions/`** - Custom secure partitions (current: echo_service, tinymaix_inference)
- **`app_broker/`** - Non-secure application broker
- **`interface/`** - APIs between secure/non-secure worlds
- **`models/`** - Encrypted TinyMaix model files
- **`tools/`** - Model encryption utilities

#### Secure Partition Services
- **Echo Service** (PID: 444, SID: 0x00000105) - Connection-based echo functionality
- **TinyMaix Inference Service** (PID: 445, SID: 0x00000107) - TinyMaix neural network inference engine

### Model Management

#### Model Encryption
Models are encrypted using AES-128-CBC for secure deployment. The `./build.sh` script automatically encrypts models using HUK (Hardware Unique Key) derived keys.

#### HUK Key Extraction and Model Encryption Workflow

First, extract HUK-derived keys in DEV_MODE, then use them for model encryption:

```bash
# Step 1: Build in DEV_MODE to extract HUK-derived keys
./build.sh DEV_MODE

# Step 2: Find "✓ Key (hex):" in serial output and copy the hex key
# Example: ✓ Key (hex): 40c962d66a1fa40346cac8b7e612741e

# Step 3: Convert hex to binary using xxd
echo "40c962d66a1fa40346cac8b7e612741e" | xxd -r -p > models/model_key_psa.bin

# Or save with custom name
echo "YOUR_HUK_KEY_HERE" | xxd -r -p > models/my_device_key.bin

# Step 4: Encrypt model using extracted HUK key (recommended)
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

**Important**: Each device generates unique HUK keys. If you change devices, you must extract new HUK keys.

**Manual encryption methods:**
```bash
# Use existing key file
python3 tools/tinymaix_model_encryptor.py --input models/mnist_valid_q.h --output models/encrypted_mnist_model_psa.bin --key-file models/model_key_psa.bin --generate-c-header

# Use PSA test key (same as C code)
python3 tools/tinymaix_model_encryptor.py --input models/mnist_valid_q.h --output models/encrypted_model.bin --use-psa-key --generate-c-header

# Generate new encryption key
python3 tools/tinymaix_model_encryptor.py --input models/mnist_valid_q.h --output models/encrypted_model.bin --generate-key --output-key models/new_key.bin --generate-c-header

# Password-based key generation
python3 tools/tinymaix_model_encryptor.py --input models/mnist_valid_q.h --output models/encrypted_model.bin --generate-key --password "your_password" --output-key models/pwd_key.bin --generate-c-header
```

#### Decryption Testing
You can verify the encrypted model accuracy using Python:
```bash
python3 test_cbc_decrypt.py
```

#### Model Format
- **Input**: TinyMaix C header files (.h) with model data arrays
- **Output**: Encrypted binary packages (.bin) with CBC encryption
- **Keys**: 128-bit keys stored in separate key files
- **Integration**: C headers/sources generated for embedded deployment

### Testing

Test applications in `nspe/` demonstrate secure partition usage:
- **`echo_test_app.c`** - Tests echo service functionality
- **`psa_crypto_test.c`** - Tests PSA crypto operations (AES-128, SHA-256)
- **`tinymaix_inference_test.c`** - Tests TinyMaix inference service functionality
- **`test_runner.c`** - Orchestrates all tests in RTOS context

Tests run automatically on device boot and output results via UART/USB serial.

### Development Requirements

#### Hardware
- **Device**: Raspberry Pi Pico 2W with RP2350 microcontroller
- **Toolchain**: GNU ARM (`GNUARM`)
- **Deployment**: UF2 format via picotool

#### Software Dependencies
- **Python packages**: See [pico2w-trusted-firmware-m/tools/requirements.txt](./pico2w-trusted-firmware-m/tools/requirements.txt)
- **picotool**: [https://github.com/raspberrypi/picotool](https://github.com/raspberrypi/picotool) - Raspberry Pi Pico deployment tool
- **Python 3.x**: For model encryption and test scripts
- **CMake**: Build system

### Manual Build Steps

For advanced users, manual build steps are available:

```bash
# SPE Build
cmake -S ./spe -B build/spe \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DTFM_PROFILE=profile_medium \
  -DTFM_PARTITION_ECHO_SERVICE=ON \
  -DTFM_PARTITION_TINYMAIX_INFERENCE=ON

cmake --build build/spe -- -j8 install

# NSPE Build  
cmake -S ./nspe -B build/nspe \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DCONFIG_SPE_PATH=build/spe/api_ns

cmake --build build/nspe -- -j8

# Generate UF2 files
./pico_uf2.sh . ./build

# Deploy to Pico
picotool erase && picotool load build/spe/bin/bl2.uf2 && picotool load build/spe/bin/tfm_s_ns_signed.uf2 && picotool reboot
```

### Development Notes

- **Security Isolation**: Secure and non-secure worlds are strictly separated via ARM TrustZone
- **Build Dependencies**: SPE must build before NSPE (NSPE links against SPE APIs)
- **Manifest Validation**: Partition manifests are validated during build for PSA compliance
- **Model Security**: All neural network models must be encrypted before deployment to secure partitions
