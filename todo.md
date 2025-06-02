# TFLM (TensorFlow Lite Micro) 보안 서비스 구현 TODO 리스트

## 프로젝트 개요
Raspberry Pi Pico 2W에서 TF-M(Trusted Firmware-M)을 사용하여 TFLM 추론을 보안 파티션에서 실행하는 시스템 구현

---

## 1. TFLM 서비스 인터페이스 정의

**담당자:** 남유찬  
**상태:** - [ ] 시작 전 / - [x] 진행 중 / - [ ] 완료

### 대상 파일
- `tflm_interface/include/psa_tflm_defs.h`
- `tflm_interface/ns/psa_tflm_client.h`

### 주요 작업
- [x] **psa_tflm_defs.h 내용 검토 및 수정**
  - PSA_TFLM_SERVICE_SID 정의 확인
  - PSA_TFLM_SERVICE_SIGNAL 정의 확인
  - 요청 타입 상수 정의 (PSA_TFLM_REQUEST_TYPE_ECHO, LOAD_MODEL, RUN_INFERENCE 등)
  - 구조체 정의 (tflm_model_info_t, tflm_inference_request_t, tflm_inference_response_t)

- [x] **psa_tflm_client.h 클라이언트 API 검토**
  - psa_tflm_echo, psa_tflm_load_model, psa_tflm_run_inference 함수 선언 확인
  - 필요한 추가 API 정의

**사용 API:** `psa/client.h`, `psa/error.h`

---

## 2. TFLM 보안 파티션 기본 구조 생성

**담당자:** 남유찬  
**상태:** - [ ] 시작 전 / - [x] 진행 중 / - [ ] 완료

### 대상 디렉토리
- `tflm_spe/partitions/` (파티션 구현)
- `tflm_spe/` (매니페스트)

### 주요 작업
- [x] **매니페스트 파일 검토 및 수정**
  - `tflm_spe/tflm_secure_service_sp.yaml`
  - `tflm_spe/tflm_manifest_list.yaml`

- [x] **CMakeLists.txt 빌드 설정 검토**
  - `tflm_spe/partitions/CMakeLists.txt`
  - include 경로 확인 (../../tflm_interface/include) - ✅ 경로 수정 완료
  - 소스 파일 목록 확인

- [x] **파티션 소스 파일 검토 및 수정**
  - `tflm_spe/partitions/tflm_secure_service_sp.h`
  - `tflm_spe/partitions/tflm_secure_service_sp.c`
  - PSA 메시지 루프 구현 확인
  - 요청 타입별 분기 처리 확인

**사용 API:** `psa/service.h`, `tfm_log_unpriv.h`

---

## 3. 비보안 애플리케이션 기본 클라이언트 작성

**담당자:** 남유찬  
**상태:** - [ ] 시작 전 / - [x] 진행 중 / - [ ] 완료

### 대상 디렉토리
- `tflm_ns/app/` (독립 실행형 NS 앱)
- `app_broker/` (브로커 통합)

### 주요 작업
- [x] **NS 애플리케이션 main 함수 검토**
  - `tflm_ns/app/tflm_main.c`
  - 클라이언트 API 호출 로직 확인
  - 로그 출력 기능 확인

- [x] **CMakeLists.txt 설정 검토**
  - `tflm_ns/app/CMakeLists.txt`
  - `app_broker/CMakeLists.txt`
  - include 경로 확인 (../../tflm_interface/include, ../../tflm_interface/ns) - ✅ 경로 수정 완료
  - 소스 파일 목록 확인 (tflm_main.c, psa_tflm_client.c) - ✅ 링크 오류 해결 완료

- [x] **클라이언트 구현 검토**
  - `tflm_interface/ns/psa_tflm_client.c`
  - PSA API 호출 구현 확인

**사용 API:** `psa/client.h`, `tflm_interface/ns/psa_tflm_client.h`

---

## 4. 모델 암호화 스크립트 작성 + 프로비저닝

**담당자:** 송현준  
**상태:** - [ ] 시작 전 / - [ ] 진행 중 / - [ ] 완료

### 대상 파일
- `tools/model_provisioning/encrypt_model.py` (신규 생성)
- `tflm_spe/partitions/encrypted_model_data.h` (스크립트로 생성)

### 주요 작업
- [ ] **tools/model_provisioning/ 디렉토리 생성**

- [ ] **encrypt_model.py 스크립트 작성**
  - TFLite 모델 파일(.tflite) 입력 처리
  - AES-GCM 암호화 구현
  - 키 관리 (HUK 기반 또는 개발용 고정 키)
  - C 헤더 파일 생성 (nonce, 암호문, 원본 길이)

- [ ] **encrypted_model_data.h 자동 생성 확인**
  - C 배열 변수 선언 형식
  - tflm_spe/partitions/ 디렉토리에 저장

- [ ] **테스트용 샘플 TFLite 모델 준비**

**사용 라이브러리:** `cryptography` (Python), `argparse`, `os`, `struct`

---

## 5. 보안 파티션 내 모델 복호화 기능 구현

**담당자:** 송현준 (암호/키), 남유찬 (PSA API)  
**상태:** - [ ] 시작 전 / - [ ] 진행 중 / - [ ] 완료

### 대상 디렉토리
`tflm_spe/partitions/`

### 주요 작업
- [ ] **암호화 모듈 헤더 검토 및 수정**
  - `tflm_spe/partitions/tflm_crypto_ops.h`
  - tflm_crypto_init(), tflm_decrypt_model() 함수 선언 확인

- [ ] **암호화 모듈 구현**
  - `tflm_spe/partitions/tflm_crypto_ops.c`
  - PSA Crypto API를 사용한 키 준비 구현
  - AES-GCM 복호화 및 인증 태그 검증 구현
  - encrypted_model_data.h 데이터 활용

- [ ] **보안 파티션 main에 복호화 연동**
  - `tflm_spe/partitions/tflm_secure_service_sp.c`
  - 초기화 시 tflm_crypto_init() 호출
  - LOAD_MODEL 요청 처리 시 tflm_decrypt_model() 호출

**사용 API:** `psa/crypto.h` (psa_crypto_init, psa_import_key, psa_aead_decrypt)

---

## 6. TFLM 라이브러리 통합 및 SP CMake 수정

**담당자:** 박수진, 송현준  
**상태:** - [ ] 시작 전 / - [ ] 진행 중 / - [ ] 완료

### 대상 파일
- `tflm_spe/partitions/CMakeLists.txt` (수정)
- `lib/tflite-micro/` (신규 추가)

### 주요 작업
- [ ] **TFLM 라이브러리 소스 다운로드 및 배치**
  - lib/tflite-micro/ 또는 적절한 외부 경로에 배치

- [ ] **CMakeLists.txt 수정**
  - TFLM 라이브러리를 tflm_secure_service_sp 타겟에 링크
  - TFLM 헤더 경로를 target_include_directories에 추가
  - 필요시 TFLM 빌드 설정 추가

- [ ] **빌드 테스트**
  - TFLM 라이브러리가 정상적으로 링크되는지 확인

**사용 API:** `target_link_libraries()`, `target_include_directories()`

---

## 7. 보안 파티션 내 TFLM 추론 엔진 구현

**담당자:** 박수진, 송현준  
**상태:** - [ ] 시작 전 / - [ ] 진행 중 / - [ ] 완료

### 대상 디렉토리
`tflm_spe/partitions/`

### 주요 작업
- [ ] **추론 엔진 헤더 검토 및 수정**
  - `tflm_spe/partitions/tflm_inference_engine.h`
  - TFLM API에 맞는 함수 선언으로 수정

- [ ] **추론 엔진 구현 (전체 재작성)**
  - `tflm_spe/partitions/tflm_inference_engine.c`
  - TFLM 헤더 include 추가
  - MicroErrorReporter, MicroMutableOpResolver, MicroInterpreter 구현
  - 텐서 아레나 정적 할당
  - tflm_init_model(): GetModel, 연산자 등록, MicroInterpreter 생성
  - tflm_run_inference(): 입력 설정, Invoke, 출력 가져오기

- [ ] **보안 파티션에 추론 엔진 연동**
  - `tflm_spe/partitions/tflm_secure_service_sp.c`
  - LOAD_MODEL 요청에 TFLM 인터프리터 초기화 연동
  - RUN_INFERENCE 요청에 TFLM 추론 수행 연동

**사용 API:** TFLM C++ API (MicroErrorReporter, MicroMutableOpResolver, MicroInterpreter)

---

## 8. NSPE 애플리케이션에 실제 모델 데이터 추론 요청 구현

**담당자:** 박수진 (TFLM 연동), 남유찬 (PSA API 사용)  
**상태:** - [ ] 시작 전 / - [ ] 진행 중 / - [ ] 완료

### 대상 파일
`tflm_ns/app/tflm_main.c`

### 주요 작업
- [ ] **실제 모델에 맞는 입력 데이터 준비**
  - 현재 더미 데이터를 실제 모델 입력 형식에 맞게 수정
  - 입력 데이터 크기 및 타입 확인

- [ ] **클라이언트 API 호출 로직 개선**
  - psa_tflm_load_model() 호출 확인
  - psa_tflm_run_inference() 호출 확인
  - 에러 처리 로직 추가

- [ ] **추론 결과 해석 및 출력**
  - SP로부터 받은 결과 데이터 파싱
  - UART를 통한 결과 출력
  - 결과 검증 로직 추가

**사용 API:** `tflm_interface/ns/psa_tflm_client.h`, `psa/client.h`

---

## 9. 전체 시스템 통합 테스트 및 디버깅

**담당자:** 전체 팀  
**상태:** - [ ] 시작 전 / - [ ] 진행 중 / - [ ] 완료

### 대상 파일
모든 관련 파일

### 주요 작업
- [x] **빌드 시스템 검증**
  - build_*.sh 스크립트 실행 확인 - ✅ 정상 실행 확인
  - CMake 빌드 오류 해결 - ✅ 주요 경로 및 링크 오류 해결
  - 링크 오류 해결 - ✅ psa_tflm_client.c 추가로 해결

- [ ] **엔드투엔드 기능 테스트**
  - NSPE에서 모델 입력 제공
  - SPE에서 모델 로드 및 복호화 확인
  - SPE에서 TFLM 추론 수행 확인
  - NSPE에서 결과 수신 및 검증

- [ ] **디버깅 및 최적화**
  - 로그 분석을 통한 오류 수정
  - 메모리 사용량 점검 (스택, 텐서 아레나)
  - 성능 테스트 및 최적화

- [ ] **보드 플래싱 및 실제 하드웨어 테스트**
  - UF2 파일 생성 확인
  - Raspberry Pi Pico 2W에 플래싱
  - UART 로그 확인

---

## 우선순위 및 의존성

### 우선순위 1 (기반 구조)
- **Task 1:** TFLM 서비스 인터페이스 정의
- **Task 2:** TFLM 보안 파티션 기본 구조 생성
- **Task 3:** 비보안 애플리케이션 기본 클라이언트 작성

### 우선순위 2 (핵심 기능)
- **Task 4:** 모델 암호화 스크립트 작성
- **Task 5:** 보안 파티션 내 모델 복호화 기능 구현
- **Task 6:** TFLM 라이브러리 통합

### 우선순위 3 (고급 기능)
- **Task 7:** TFLM 추론 엔진 구현
- **Task 8:** 실제 모델 데이터 추론 요청 구현

### 우선순위 4 (통합 및 검증)
- **Task 9:** 전체 시스템 통합 테스트 및 디버깅

### 의존성
- **Task 2** → **Task 1** (인터페이스 정의 필요)
- **Task 3** → **Task 1** (인터페이스 정의 필요)
- **Task 5** → **Task 4** (암호화 스크립트 필요)
- **Task 7** → **Task 6** (TFLM 라이브러리 필요)
- **Task 8** → **Task 7** (추론 엔진 필요)
- **Task 9** → **Task 1~8** (모든 구성 요소 필요)

---

## 참고 사항

### 현재 프로젝트 상태
- ✅ 기본 TF-M 구조는 구축됨
- ✅ TFLM 인터페이스 파일들 존재
- ✅ 보안 파티션 기본 구조 존재
- ✅ NS 애플리케이션 기본 구조 존재
- ✅ 빌드 시스템 설정됨

### 해결된 빌드 문제
- ✅ `psa_tflm_defs.h` 경로 문제 해결
- ✅ `psa_tflm_client.h` 경로 문제 해결
- ✅ 링크 오류 (psa_tflm_client.c 추가) 해결

### 다음 단계
1. 각 Task의 현재 구현 상태 검토
2. 누락된 기능 식별 및 구현
3. 단계별 테스트 및 검증
4. 통합 테스트 수행

---

## 진행 상황 업데이트

| Task | 상태 | 완료일 | 비고 |
|------|------|--------|------|
| Task 1 | 🟡 진행 중 | - | 인터페이스 정의 검토 중 |
| Task 2 | 🟡 진행 중 | - | 빌드 오류 일부 해결 |
| Task 3 | 🟡 진행 중 | - | 링크 오류 해결 완료 |
| Task 4 | ⚪ 시작 전 | - | - |
| Task 5 | ⚪ 시작 전 | - | - |
| Task 6 | ⚪ 시작 전 | - | - |
| Task 7 | ⚪ 시작 전 | - | - |
| Task 8 | ⚪ 시작 전 | - | - |
| Task 9 | ⚪ 시작 전 | - | - |

**범례:**
- ⚪ 시작 전
- 🟡 진행 중  
- 🟢 완료
- 🔴 중단/문제
