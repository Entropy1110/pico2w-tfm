# Model Encryption Guide

## Overview

This guide covers the complete workflow for encrypting TinyMaix neural network models for secure deployment. The encryption system uses AES-128-CBC with PKCS7 padding and integrates with Hardware Unique Key (HUK) derivation for maximum security.

## Encryption Architecture

### Security Model
```
Model Development → Encryption → Secure Deployment → HUK Decryption → Inference
     (Host)           (Host)        (Device)          (Secure)       (Secure)
```

### Key Management
- **Development Keys**: Test keys for development and validation
- **Production Keys**: HUK-derived keys unique to each device
- **Key Derivation**: HKDF-SHA256 with device-specific labels

## Model Encryption Tool

### Tool Location
- **Script**: `tools/tinymaix_model_encryptor.py`
- **Purpose**: Encrypt TinyMaix model files with various key sources
- **Output**: Encrypted binary + C header files

### Basic Usage
```bash
# Automatic encryption during build (recommended)
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

### Command Line Options

#### Input/Output Options
```bash
--input <file>           # Input TinyMaix model (.h file)
--output <file>          # Output encrypted model (.bin file)  
--generate-c-header      # Generate C header for embedding
```

#### Key Source Options
```bash
--key-file <file>        # Use existing key file
--generate-key           # Generate new random key
--use-psa-key           # Use predefined PSA test key
--password <string>      # Generate key from password
```

#### Key Output Options
```bash
--output-key <file>      # Save generated key to file
```

### Encryption Examples

#### 1. Using Existing Key File
```bash
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --key-file models/my_encryption_key.bin \
    --generate-c-header
```

#### 2. Generate New Random Key
```bash
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --generate-key \
    --output-key models/new_key.bin \
    --generate-c-header
```

#### 3. Password-Based Key
```bash
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --password "my_secure_password_123" \
    --output-key models/password_key.bin \
    --generate-c-header
```

#### 4. PSA Test Key
```bash
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --use-psa-key \
    --generate-c-header
```

## Input Model Format

### TinyMaix Model Structure
TinyMaix models are provided as C header files with embedded data arrays:

```c
// Example: models/mnist_valid_q.h
#ifndef _MNIST_VALID_Q_H
#define _MNIST_VALID_Q_H

#include "tinymaix.h"

// Model magic identifier
#define MNIST_VALID_Q_MAGIC 0x58414D54  // "MAIX"

// Model data array
const uint8_t mnist_valid_q_model[] = {
    0x58, 0x41, 0x4D, 0x54,  // Magic: "MAIX"
    0x03, 0x00, 0x00, 0x00,  // Version
    // ... model weights and structure ...
};

const uint32_t mnist_valid_q_model_size = sizeof(mnist_valid_q_model);

#endif
```

### Supported Model Types
- **Quantized Models**: INT8/INT16 quantized networks (recommended)
- **Floating Point**: FP32 models (larger size)
- **Model Size**: Current limit 4KB (configurable)

## Encryption Process

### AES-128-CBC Encryption
The tool uses industry-standard AES-128-CBC encryption:

1. **Key Generation/Loading**: 128-bit encryption key
2. **IV Generation**: Random 128-bit initialization vector
3. **PKCS7 Padding**: Pad data to 16-byte blocks
4. **CBC Encryption**: Encrypt with AES-128-CBC
5. **Header Creation**: Add metadata header

### Encrypted File Format
```c
typedef struct {
    uint32_t magic;              // "TMAX" (0x54 0x4D 0x41 0x58)
    uint32_t version;            // Format version = 3 for CBC
    uint32_t original_size;      // Size of original model data
    uint8_t iv[16];              // AES-CBC initialization vector
    // Encrypted model data follows (with PKCS7 padding)
} encrypted_tinymaix_header_cbc_t;
```

### Generated Files
After encryption, the tool generates:

1. **Binary File**: `encrypted_model.bin` - Raw encrypted data
2. **C Header**: `encrypted_model.h` - Embeddable C arrays
3. **C Source**: `encrypted_model.c` - Implementation file

### Generated C Header Example
```c
// Generated: encrypted_mnist_model_psa.h
#ifndef _ENCRYPTED_MNIST_MODEL_PSA_H
#define _ENCRYPTED_MNIST_MODEL_PSA_H

#include <stdint.h>
#include <stddef.h>

// Encrypted model data
extern const uint8_t encrypted_mdl_data_data[];
extern const size_t encrypted_mdl_data_size;

#endif
```

### Generated C Source Example  
```c
// Generated: encrypted_mnist_model_psa.c
#include "encrypted_mnist_model_psa.h"

const uint8_t encrypted_mdl_data_data[] = {
    0x54, 0x4D, 0x41, 0x58,  // Magic: "TMAX"
    0x03, 0x00, 0x00, 0x00,  // Version: 3 (CBC)
    0xBC, 0x05, 0x00, 0x00,  // Original size: 1468 bytes
    0x12, 0x34, 0x56, 0x78,  // IV (16 bytes)
    0x9A, 0xBC, 0xDE, 0xF0,
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
    // ... encrypted model data ...
};

const size_t encrypted_mdl_data_size = sizeof(encrypted_mdl_data_data);
```

## Key Management

### Development Keys vs Production Keys

#### Development Keys
- **Purpose**: Testing and development
- **Source**: Predefined or randomly generated
- **Security**: Lower security, reusable across devices
- **Usage**: Development builds, testing

#### Production Keys  
- **Purpose**: Production deployment
- **Source**: Hardware Unique Key (HUK) derivation
- **Security**: Device-unique, hardware-backed
- **Usage**: Production builds only

### HUK Key Derivation
In production, encryption keys are derived from the device's Hardware Unique Key:

```c
/* Key derivation label - must match encryption tool */
const char *huk_label = "pico2w-tinymaix-model-aes128-v1.0";

/* Derive 128-bit key using HKDF-SHA256 */
psa_status_t derive_key_from_huk(const char *label, 
                                 uint8_t *derived, 
                                 size_t derived_len)
{
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    
    /* Setup HKDF with SHA-256 */
    psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    
    /* Empty salt */
    psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT, NULL, 0);
    
    /* Use HUK as secret */
    psa_key_derivation_input_key(&op, PSA_KEY_DERIVATION_INPUT_SECRET,
                                 TFM_BUILTIN_KEY_ID_HUK);
    
    /* Use label as info */
    psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO,
                                   (const uint8_t*)label, strlen(label));
    
    /* Derive key */
    return psa_key_derivation_output_bytes(&op, derived, derived_len);
}
```

### Key Security Best Practices

#### Development Phase
1. **Use Test Keys**: Use `--use-psa-key` for consistent development
2. **Version Control**: Never commit real encryption keys to git
3. **Key Rotation**: Regularly rotate development keys
4. **Access Control**: Limit access to key generation tools

#### Production Phase
1. **HUK Derivation**: Always use HUK-derived keys
2. **Unique Labels**: Use unique derivation labels per application
3. **Key Validation**: Verify key derivation in secure partition
4. **Audit Trail**: Log key derivation (without exposing keys)

## Validation and Testing

### Decryption Testing
Test encrypted models with Python validation:

```bash
# Test decryption accuracy
python3 test_cbc_decrypt.py
```

### Test Script Example
```python
#!/usr/bin/env python3
import struct
from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad

def test_decryption():
    # Load encrypted model
    with open('models/encrypted_mnist_model_psa.bin', 'rb') as f:
        encrypted_data = f.read()
    
    # Parse header
    magic, version, original_size = struct.unpack('<III', encrypted_data[:12])
    iv = encrypted_data[12:28]
    ciphertext = encrypted_data[28:]
    
    print(f"Magic: 0x{magic:08x}")
    print(f"Version: {version}")
    print(f"Original size: {original_size}")
    print(f"Ciphertext size: {len(ciphertext)}")
    
    # Load key (for testing - use PSA test key)
    with open('models/model_key_psa.bin', 'rb') as f:
        key = f.read()
    
    # Decrypt
    cipher = AES.new(key, AES.MODE_CBC, iv)
    decrypted = cipher.decrypt(ciphertext)
    
    # Remove PKCS7 padding
    try:
        unpadded = unpad(decrypted, 16)
        print(f"Decrypted size: {len(unpadded)}")
        print(f"Size match: {len(unpadded) == original_size}")
        
        # Verify TinyMaix magic
        if len(unpadded) >= 4:
            model_magic = struct.unpack('<I', unpadded[:4])[0]
            print(f"Model magic: 0x{model_magic:08x}")
            print(f"Valid TinyMaix model: {model_magic == 0x5849414D}")
            
    except ValueError as e:
        print(f"Padding error: {e}")

if __name__ == "__main__":
    test_decryption()
```

### Integration Testing
Validate end-to-end encryption workflow:

1. **Encrypt Model**: Use encryption tool with test key
2. **Deploy to Device**: Build and deploy to Pico 2W
3. **Verify Decryption**: Check secure partition logs
4. **Test Inference**: Verify inference results match expectations

## Build Integration

### Automatic Encryption
The build script automatically encrypts models:

```bash
# In build.sh
echo "TinyMaix Model encryption using Test Key..."
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

### CMake Integration
Generated files are included in the build:

```cmake
# In partitions/tinymaix_inference/CMakeLists.txt
target_sources(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        tinymaix_inference.c
        # Include encrypted model
        ../../models/encrypted_mnist_model_psa.c
)

target_include_directories(tfm_app_rot_partition_tinymaix_inference
    PRIVATE
        # Include model headers
        ../../models
)
```

### Build Dependencies
```cmake
# Ensure model encryption runs before compilation
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/models/encrypted_mnist_model_psa.c
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/tinymaix_model_encryptor.py
            --input ${CMAKE_SOURCE_DIR}/models/mnist_valid_q.h
            --output ${CMAKE_SOURCE_DIR}/models/encrypted_mnist_model_psa.bin
            --key-file ${CMAKE_SOURCE_DIR}/models/model_key_psa.bin
            --generate-c-header
    DEPENDS ${CMAKE_SOURCE_DIR}/models/mnist_valid_q.h
            ${CMAKE_SOURCE_DIR}/tools/tinymaix_model_encryptor.py
    COMMENT "Encrypting TinyMaix model..."
)
```

## Advanced Usage

### Multiple Models
Encrypt multiple models with different keys:

```bash
# Model 1: MNIST classification
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_model.h \
    --output models/encrypted_mnist.bin \
    --generate-key \
    --output-key models/mnist_key.bin \
    --generate-c-header

# Model 2: Custom detection model  
python3 tools/tinymaix_model_encryptor.py \
    --input models/detection_model.h \
    --output models/encrypted_detection.bin \
    --generate-key \
    --output-key models/detection_key.bin \
    --generate-c-header
```

### Custom Key Derivation
Implement custom key derivation for specific use cases:

```python
# Custom key derivation example
import hashlib
import hmac

def derive_custom_key(master_key, device_id, model_name):
    """Derive model-specific key from master key and device info"""
    
    # Create derivation context
    context = f"{device_id}:{model_name}:v1.0"
    
    # Use HMAC-SHA256 for key derivation
    derived = hmac.new(
        master_key,
        context.encode('utf-8'),
        hashlib.sha256
    ).digest()
    
    # Use first 16 bytes for AES-128
    return derived[:16]
```

### Batch Processing
Process multiple models in batch:

```bash
#!/bin/bash
# Batch encrypt all models

MODELS_DIR="models"
OUTPUT_DIR="encrypted_models"
KEY_DIR="keys"

mkdir -p $OUTPUT_DIR $KEY_DIR

for model in $MODELS_DIR/*.h; do
    basename=$(basename "$model" .h)
    echo "Encrypting $basename..."
    
    python3 tools/tinymaix_model_encryptor.py \
        --input "$model" \
        --output "$OUTPUT_DIR/encrypted_${basename}.bin" \
        --generate-key \
        --output-key "$KEY_DIR/${basename}_key.bin" \
        --generate-c-header
done
```

## Security Considerations

### Threat Model
- **Model IP Protection**: Prevent model reverse engineering
- **Model Integrity**: Detect model tampering
- **Key Security**: Protect encryption keys
- **Side Channel Attacks**: Mitigate timing and power analysis

### Security Best Practices

#### Key Management
1. **Never Hardcode Keys**: Use dynamic key derivation
2. **Secure Key Storage**: Use hardware-backed storage when available
3. **Key Rotation**: Implement key rotation for long-term deployments
4. **Access Control**: Limit access to key generation tools

#### Model Protection
1. **Validate Decryption**: Always verify decrypted model integrity
2. **Error Handling**: Fail securely on decryption errors
3. **Memory Protection**: Clear keys from memory after use
4. **Audit Logging**: Log security events (without exposing keys)

#### Development Security
1. **Separate Builds**: Use different keys for development vs production
2. **Secure Development**: Protect development keys and environments
3. **Code Review**: Review encryption/decryption code carefully
4. **Testing**: Test with corrupted models and invalid keys

### Common Vulnerabilities

#### Weak Key Management
```c
// BAD: Hardcoded key
uint8_t encryption_key[] = {0x01, 0x02, 0x03, ...};

// GOOD: HUK-derived key
derive_key_from_huk("model-key-label", encryption_key, 16);
```

#### Insufficient Validation
```c
// BAD: No validation
decrypt_model(encrypted_data, size);

// GOOD: Validate header and magic
if (header->magic != EXPECTED_MAGIC || header->version != 3) {
    return PSA_ERROR_INVALID_ARGUMENT;
}
```

#### Key Exposure
```c
// BAD: Key logging
INFO_UNPRIV("Using key: %02x%02x%02x...", key[0], key[1], key[2]);

// GOOD: No key exposure (except in DEV_MODE)
#ifdef DEV_MODE
INFO_UNPRIV("Debug: derived key for debugging");
#endif
```

## Troubleshooting

### Common Issues

#### "Invalid magic" Errors
```
ERROR: Invalid model magic
```
**Causes**:
- Wrong encryption key
- Corrupted encrypted file
- Header parsing issues

**Solutions**:
- Verify key derivation matches encryption
- Re-encrypt with correct key
- Check file integrity

#### Size Mismatch Errors
```
Size mismatch: got 1460, expected 1468
```
**Causes**:
- PKCS7 padding issues
- Incomplete decryption
- Buffer size problems

**Solutions**:
- Verify PKCS7 padding removal
- Check buffer sizes
- Validate encryption format

#### Key Derivation Failures
```
ERROR: psa_key_derivation_output_bytes failed: -135
```
**Causes**:
- HUK not available
- Wrong derivation algorithm
- Insufficient privileges

**Solutions**:
- Ensure HUK support in platform
- Verify PSA crypto initialization
- Check secure partition permissions

### Debug Techniques

#### Enable DEV_MODE
```bash
./build.sh DEV_MODE
```
This enables key exposure for debugging.

#### Python Validation
```bash
python3 test_cbc_decrypt.py
```
Validate encryption/decryption offline.

#### Increase Logging
```c
#define ENABLE_DEBUG_LOGGING
#ifdef ENABLE_DEBUG_LOGGING
INFO_UNPRIV("Encryption header: magic=0x%08x, size=%d\n", 
            header->magic, header->original_size);
#endif
```

#### Memory Inspection
```c
void debug_print_buffer(const uint8_t* buf, size_t size) {
    INFO_UNPRIV("Buffer (%d bytes): ", size);
    for (size_t i = 0; i < size && i < 16; i++) {
        INFO_UNPRIV("%02x ", buf[i]);
    }
    INFO_UNPRIV("\n");
}
```

## Next Steps

- **[TinyMaix Integration](./tinymaix-integration.md)**: Use encrypted models in secure partitions
- **[PSA API Reference](./psa-api.md)**: PSA crypto API usage patterns
- **[Troubleshooting Guide](./troubleshooting.md)**: Detailed troubleshooting procedures