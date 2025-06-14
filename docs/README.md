# Developer Documentation

This directory contains comprehensive developer documentation for the Pico 2W TF-M TinyMaix project.

## 📚 Document Index

### 🏗️ System Architecture
- **[TF-M Architecture](./tfm-architecture.md)** - ARM TrustZone based dual-world architecture
- **[Secure Partition Development](./secure-partitions.md)** - PSA service implementation guide

### 🤖 Machine Learning Integration
- **[TinyMaix Integration](./tinymaix-integration.md)** - Implementing ML inference in secure partitions
- **[Model Encryption](./model-encryption.md)** - HUK-based model security

### 🔧 Development Tools
- **[PSA API](./psa-api.md)** - Communication between secure/non-secure worlds
- **[Build System](./build-system.md)** - CMake configuration and build process
- **[Testing Framework](./testing-framework.md)** - Automated testing system

### 🛠️ Troubleshooting
- **[Troubleshooting Guide](./troubleshooting.md)** - Common issues and solutions

## 🚀 Getting Started

If you are new to the project, it is recommended to read the documents in the following order:

1.  **[TF-M Architecture](./tfm-architecture.md)** - Understand the overall system
2.  **[Build System](./build-system.md)** - How to build the project
3.  **[Secure Partition Development](./secure-partitions.md)** - Develop secure services
4.  **[TinyMaix Integration](./tinymaix-integration.md)** - Implement ML features
5.  **[Testing Framework](./testing-framework.md)** - Write and run tests

## 🔑 Key Concepts

### DEV_MODE
- **Purpose**: HUK-derived key extraction and debugging
- **Usage**: `./build.sh DEV_MODE`
- **Security Warning**: Never use in a production environment

### HUK Key Extraction Workflow
```bash
# 1. Build with DEV_MODE
./build.sh DEV_MODE

# 2. Copy the key from serial output
# "HUK-derived key: 40c962d66a1fa40346cac8b7e612741e"

# 3. Convert hex to binary
echo "40c962d66a1fa40346cac8b7e612741e" | xxd -r -p > models/model_key_psa.bin

# 4. Encrypt the model with the extracted key
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

### 🔒 Security Considerations
- HUK keys are device-specific and cannot be changed.
- DEV_MODE should only be used during development.
- All models must be encrypted before deployment.
- Maintain security boundaries through PSA APIs.

## 🤝 Contributing

Contributions to improve documentation are welcome:

1.  Report issues for unclear sections or errors.
2.  Add documentation for new features or use cases.
3.  Provide example code or practical guides.

## 🔗 Additional Resources

- **[Korean Documentation](../docs-ko/)** - Korean version of developer documents
- **[Project README](../README.md)** - Project overview and quick start
- **[TF-M Official Documentation](https://tf-m-user-guide.trustedfirmware.org/)** - Detailed guide for the TF-M framework
- **[PSA Specification](https://developer.arm.com/architectures/security-architectures/platform-security-architecture)** - ARM PSA architecture specification