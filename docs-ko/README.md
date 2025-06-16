# 개발자 문서 (한국어)

이 폴더는 Pico 2W TF-M TinyMaix 프로젝트의 한국어 개발자 문서를 포함합니다.

## 문서 목록

### 🏗️ 시스템 아키텍처
- **[TF-M 아키텍처](./tfm-architecture.md)** - ARM TrustZone 기반 이중 세계 아키텍처
- **[보안 파티션 개발](./secure-partitions.md)** - PSA 서비스 구현 가이드

### 🤖 머신러닝 통합
- **[TinyMaix 통합](./tinymaix-integration.md)** - 보안 파티션에서 ML 추론 구현
- **[모델 암호화](./model-encryption.md)** - HUK 기반 모델 보안

### 🔧 개발 도구
- **[PSA API](./psa-api.md)** - 보안/비보안 세계 간 통신
- **[빌드 시스템](./build-system.md)** - CMake 구성 및 빌드 과정
- **[테스트 프레임워크](./testing-framework.md)** - 자동화된 테스트 시스템

### 🛠️ 문제 해결
- **[문제 해결 가이드](./troubleshooting.md)** - 일반적인 문제와 해결책

## 시작하기

프로젝트를 처음 접하는 경우 다음 순서로 문서를 읽는 것을 권장합니다:

1. **[TF-M 아키텍처](./tfm-architecture.md)** - 전체 시스템 이해
2. **[빌드 시스템](./build-system.md)** - 프로젝트 빌드 방법
3. **[보안 파티션 개발](./secure-partitions.md)** - 보안 서비스 개발
4. **[TinyMaix 통합](./tinymaix-integration.md)** - ML 기능 구현
5. **[테스트 프레임워크](./testing-framework.md)** - 테스트 작성 및 실행

## 주요 개념

### DEV_MODE
- **목적**: HUK 파생 키 추출 및 디버깅
- **사용법**: `./build.sh DEV_MODE`
- **보안 주의**: 프로덕션 환경에서 절대 사용 금지

### HUK 키 추출 워크플로우
```bash
# 1. DEV_MODE로 빌드
./build.sh DEV_MODE

# 2. 시리얼 출력에서 키 복사
# "HUK-derived key: 40c962d66a1fa40346cac8b7e612741e"

# 3. 16진수를 바이너리로 변환
echo "40c962d66a1fa40346cac8b7e612741e" | xxd -r -p > models/model_key_psa.bin

# 4. 추출한 키로 모델 암호화
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

### 보안 고려사항
- HUK 키는 디바이스별로 고유하며 변경 불가
- DEV_MODE는 개발 단계에서만 사용
- 모든 모델은 배포 전 암호화 필수
- PSA API를 통한 보안 경계 유지

## 기여하기

문서 개선을 위한 기여를 환영합니다:

1. 명확하지 않은 부분이나 오류 발견 시 이슈 등록
2. 새로운 기능이나 사용 사례에 대한 문서 추가
3. 예제 코드나 실용적인 가이드 제공

## 추가 자료

- **[영어 문서](../docs/)** - 영어 버전 개발자 문서
- **[프로젝트 README](../README.md)** - 프로젝트 개요 및 빠른 시작
- **[TF-M 공식 문서](https://trustedfirmware-m.readthedocs.io/)** - TF-M 프레임워크 상세 가이드
- **[PSA 사양](https://developer.arm.com/architectures/security-architectures/platform-security-architecture)** - ARM PSA 아키텍처 명세
