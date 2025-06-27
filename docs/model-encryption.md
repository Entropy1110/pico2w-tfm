# Model Encryption Guide

## Overview

This guide explains how to securely encrypt TinyMaix neural network models using Hardware Unique Key (HUK) based encryption for use in secure partitions. It employs AES-128-CBC encryption to ensure model confidentiality and integrity.

## HUK-based Security Architecture

### Hardware Unique Key (HUK)
```
HUK Security Scheme:
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
        │    HKDF-SHA256       │  ← Key Derivation Function
        │  Key Derivation      │
        └───────────┬───────────┘
                    │
    ┌───────────────▼───────────────┐
    │     Model Encryption Key      │  ← Unique key per model
    │        AES-128 Key            │
    │   (Label: model-aes128-v1.0)  │
    └───────────────────────────────┘
```

### Key Derivation Process
```c
// Derive model encryption key from HUK
psa_status_t derive_key_from_huk(const char* label, 
                                uint8_t* derived_key, 
                                size_t key_length)
{
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t status;
    
    // 1. Setup HKDF-SHA256
    status = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (status != PSA_SUCCESS) return status;
    
    // 2. Use HUK as input key
    status = psa_key_derivation_input_key(&op, PSA_KEY_DERIVATION_INPUT_SECRET,
                                         TFM_BUILTIN_KEY_ID_HUK);
    if (status != PSA_SUCCESS) goto cleanup;
    
    // 3. Use label as Info - context binding
    status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO,
                                           (const uint8_t*)label, strlen(label));
    if (status != PSA_SUCCESS) goto cleanup;
    
    // 4. Generate derived key
    status = psa_key_derivation_output_bytes(&op, derived_key, key_length);
    
cleanup:
    psa_key_derivation_abort(&op);
    return status;
}

// Example usage:
// const char* label = "pico2w-tinymaix-model-aes128-v1.0";
// derive_key_from_huk(label, key_buffer, 16);
```

## HUK Key Extraction via DEV_MODE

### Step 1: Build with DEV_MODE
```bash
# Build with DEV_MODE to enable HUK key debugging
./build.sh DEV_MODE

# Or manual build
cmake -S ./spe -B build/spe \
  -DTFM_PLATFORM=rpi/rp2350 \
  -DPICO_BOARD=pico2_w \
  -DTFM_PROFILE=profile_medium \
  -DTFM_PARTITION_TINYMAIX_INFERENCE=ON \
  -DDEV_MODE=ON

cmake --build build/spe -- -j8 install
cmake --build build/nspe -- -j8
```

### Step 2: Extract HUK Key
```bash
# Connect device and monitor serial output
# Linux/Mac:
screen /dev/ttyACM0 115200

# Windows:
# Use PuTTY or Tera Term

# Expected output:
# [TinyMaix Test] Testing HUK-derived model key (DEV_MODE)...
# [TinyMaix] Getting HUK-derived model key (DEV_MODE)
# HUK-derived key: 40c962d66a1fa40346cac8b7e612741e
# [TinyMaix Test] ✓ Model key retrieved successfully
```

### Step 3: Convert Key to Binary File
```bash
# Copy hex key from serial output
# Example: HUK-derived key: 40c962d66a1fa40346cac8b7e612741e

# Use xxd to convert hex to binary
echo "40c962d66a1fa40346cac8b7e612741e" | xxd -r -p > models/model_key_psa.bin

# Verify key file creation
ls -la models/model_key_psa.bin
# Output: -rw-r--r-- 1 user user 16 Jun 14 10:30 models/model_key_psa.bin

# Verify key file content (hex)
xxd models/model_key_psa.bin
# Output: 00000000: 40c9 62d6 6a1f a403 46ca c8b7 e612 741e  @.b.j...F...t.
```

### Step 4: Encrypt Model with Extracted Key
```bash
# Encrypt model with HUK-derived key
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header

# Example output:
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

## Encryption Tool Deep Dive

### Encryption Tool: `tinymaix_model_encryptor.py`

#### Key Features
```python
class TinyMAIXModelEncryptor:
    """TinyMAIX model encryption utility"""
    
    def parse_tinymaix_header(self, header_content: str):
        """Extract model data from C header file"""
        # Extract array name: const uint8_t array_name[]
        array_match = re.search(r'const\\s+uint8_t\\s+(\\w+)\\[\\d*\\]\\s*=', header_content)
        
        # Extract MDL_BUF_LEN, LBUF_LEN metadata
        mdl_buf_match = re.search(r'#define\\s+MDL_BUF_LEN\\s+\\((\\d+)\\)', header_content)
        
        # Parse hex data
        hex_values = re.findall(r'0x([0-9a-fA-F]{2})', hex_data)
        model_data = bytes(int(h, 16) for h in hex_values)
        
        return model_data, array_name, mdl_buf_len, lbuf_len
    
    def encrypt_model(self, model_data: bytes, key: bytes):
        """AES-CBC-PKCS7 encryption (PSA compatible)"""
        # Generate random IV
        iv = secrets.token_bytes(16)
        
        # Apply PKCS7 padding
        padded_data = self._pkcs7_pad(model_data, 16)
        
        # AES-CBC encryption
        cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
        encryptor = cipher.encryptor()
        encrypted_data = encryptor.update(padded_data) + encryptor.finalize()
        
        return encrypted_data, iv
    
    def create_encrypted_package(self, model_data: bytes, key: bytes, model_name: str):
        """Create PSA-compatible encrypted package"""
        encrypted_data, iv = self.encrypt_model(model_data, key)
        
        # PSA CBC package header
        header = struct.pack('<4sII16s',
                            b"TMAX",           # Magic header
                            3,                # Version (CBC)
                            len(model_data),  # Original size
                            iv)               # 16-byte IV
        
        return header + encrypted_data
```

#### Package Format
```
PSA CBC Encrypted Package Structure:
┌─────────────────────────────────────────┐ ← Start
│  Magic Header: "TMAX" (4 bytes)         │
├─────────────────────────────────────────┤
│  Version: 3 (4 bytes, little endian)   │  ← CBC Version
├─────────────────────────────────────────┤
│  Original Size (4 bytes, little endian)│  ← Size before compression
├─────────────────────────────────────────┤
│  IV: Random 128-bit (16 bytes)         │  ← Initialization Vector for CBC
├─────────────────────────────────────────┤
│                                         │
│  Encrypted Model Data (Variable)       │  ← AES-CBC encrypted data
│  + PKCS7 Padding                       │    (Includes PKCS7 padding)
│                                         │
└─────────────────────────────────────────┘ ← End
```

### Encryption Options

#### 1. Using HUK-Derived Key (Recommended)
```bash
# Encrypt with device-specific unique key
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

#### 2. Using PSA Test Key
```bash
# Fixed key for development/testing
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --use-psa-key \
    --generate-c-header

# PSA Test Key: 40c962d66a1fa40346cac8b7e612741e
```

#### 3. Generating New Key
```bash
# Generate random key
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --generate-key \
    --output-key models/new_key.bin \
    --generate-c-header

# Generate key from password
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --generate-key \
    --password "my_secure_password" \
    --output-key models/pwd_key.bin \
    --generate-c-header
```

## Decryption in Secure Partition

### AES-CBC Decryption Implementation
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
    
    /* Validate package header */
    if (encrypted_size < 28) {
        LOG_ERRSIG("[TinyMaix] Encrypted data too small\\n");
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* Parse header */
    const uint8_t* magic = encrypted_data;
    const uint32_t* version = (const uint32_t*)(encrypted_data + 4);
    const uint32_t* original_size = (const uint32_t*)(encrypted_data + 8);
    const uint8_t* iv = encrypted_data + 12;
    const uint8_t* ciphertext = encrypted_data + 28;
    size_t ciphertext_size = encrypted_size - 28;
    
    /* Validate magic header */
    if (memcmp(magic, "TMAX", 4) != 0) {
        LOG_ERRSIG("[TinyMaix] Invalid model magic\\n");
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* Validate version (CBC = 3) */
    if (*version != 3) {
        LOG_ERRSIG("[TinyMaix] Unsupported model version: %d\\n", *version);
        return PSA_ERROR_NOT_SUPPORTED;
    }
    
    LOG_INFSIG("[TinyMaix] Decrypting: v%d, original_size=%d\\n", 
               *version, *original_size);
    
    /* Set AES-128 key attributes */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    
    /* Import key */
    status = psa_import_key(&attributes, key, 16, &key_id);
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix] Key import failed: %d\\n", status);
        return status;
    }
    
    /* AES-CBC decryption */
    size_t output_length;
    status = psa_cipher_decrypt(key_id, PSA_ALG_CBC_NO_PADDING,
                               ciphertext, ciphertext_size,
                               decrypted_data, *decrypted_size, &output_length);
    
    /* Securely destroy key */
    psa_destroy_key(key_id);
    
    if (status != PSA_SUCCESS) {
        LOG_ERRSIG("[TinyMaix] Decryption failed: %d\\n", status);
        return status;
    }
    
    /* Validate and remove PKCS7 padding */
    if (!validate_pkcs7_padding(decrypted_data, output_length)) {
        LOG_ERRSIG("[TinyMaix] Invalid PKCS7 padding\\n");
        return PSA_ERROR_INVALID_SIGNATURE;
    }
    
    /* Remove padding */
    uint8_t padding_size = decrypted_data[output_length - 1];
    *decrypted_size = output_length - padding_size;
    
    /* Validate original size */
    if (*decrypted_size != *original_size) {
        LOG_ERRSIG("[TinyMaix] Size mismatch: %d != %d\\n", 
                   *decrypted_size, *original_size);
        return PSA_ERROR_CORRUPTION_DETECTED;
    }
    
    LOG_INFSIG("[TinyMaix] Model decrypted: %d bytes\\n", *decrypted_size);
    
    return PSA_SUCCESS;
}

/* Validate PKCS7 padding */
static bool validate_pkcs7_padding(const uint8_t* data, size_t size)
{
    if (size == 0) return false;
    
    uint8_t padding_size = data[size - 1];
    
    /* Validate padding size */
    if (padding_size == 0 || padding_size > 16 || padding_size > size) {
        LOG_ERRSIG("[TinyMaix] Invalid padding size: %d\\n", padding_size);
        return false;
    }
    
    /* Check all padding bytes are identical */
    for (size_t i = size - padding_size; i < size; i++) {
        if (data[i] != padding_size) {
            LOG_ERRSIG("[TinyMaix] Invalid padding byte at position %d\\n", i);
            return false;
        }
    }
    
    return true;
}
```

## Model Integrity Verification

### Python Decryption Test
```python
# test_cbc_decrypt.py - Encryption verification script
import struct
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

def test_model_decryption():
    """Test decryption of encrypted model"""
    
    # Read encrypted model file
    with open('models/encrypted_mnist_model_psa.bin', 'rb') as f:
        encrypted_package = f.read()
    
    # Read key file
    with open('models/model_key_psa.bin', 'rb') as f:
        key = f.read()
    
    print(f"Package size: {len(encrypted_package)} bytes")
    print(f"Key size: {len(key)} bytes")
    
    # Parse header
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
    
    # Validate magic header
    assert magic == b'TMAX', f"Invalid magic: {magic}"
    assert version == 3, f"Invalid version: {version}"
    
    # AES-CBC decryption
    cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
    decryptor = cipher.decryptor()
    padded_plaintext = decryptor.update(ciphertext) + decryptor.finalize()
    
    # Remove PKCS7 padding
    padding_size = padded_plaintext[-1]
    plaintext = padded_plaintext[:-padding_size]
    
    print(f"Decrypted size: {len(plaintext)} bytes")
    print(f"Padding size: {padding_size}")
    
    # Validate size
    assert len(plaintext) == original_size, f"Size mismatch: {len(plaintext)} != {original_size}"
    
    # Validate TinyMaix model header
    tm_magic = plaintext[0:4]
    assert tm_magic == b'TMDL', f"Invalid TinyMaix magic: {tm_magic}"
    
    print("✓ Model decryption and validation successful!")
    
    # Save decrypted model to file (for verification)
    with open('models/decrypted_model_test.bin', 'wb') as f:
        f.write(plaintext)
    
    return True

if __name__ == "__main__":
    test_model_decryption()
```

### Execution and Verification
```bash
# Run decryption test
python3 test_cbc_decrypt.py

# Expected output:
# Package size: 16412 bytes
# Key size: 16 bytes
# Magic: b'TMAX'
# Version: 3
# Original size: 16384
# IV: 1234567890abcdef1122334455667788
# Ciphertext size: 16384
# Decrypted size: 16384 bytes
# Padding size: 0
# ✓ Model decryption and validation successful!
```

## Security Considerations

### Secure Key Management
```c
/* Secure key handling principles */

// 1. Immediately clear keys from memory after use
void secure_key_cleanup(uint8_t* key_buffer, size_t key_size)
{
    /* Overwrite memory with zeros */
    memset(key_buffer, 0, key_size);
    
    /* Prevent compiler optimization */
    volatile uint8_t* volatile_ptr = (volatile uint8_t*)key_buffer;
    for (size_t i = 0; i < key_size; i++) {
        volatile_ptr[i] = 0;
    }
}

// 2. Clear sensitive data from stack
void secure_function_exit(void)
{
    uint8_t stack_cleanup[256];
    memset(stack_cleanup, 0, sizeof(stack_cleanup));
    
    /* Ensure stack variable is actually used */
    volatile uint8_t dummy = stack_cleanup[0];
    (void)dummy;
}

// 3. Prevent reuse in key derivation
static uint32_t g_key_derivation_counter = 0;

psa_status_t derive_unique_key(const char* base_label,
                              uint8_t* derived_key,
                              size_t key_length)
{
    char unique_label[128];
    
    /* Create unique label including counter */
    snprintf(unique_label, sizeof(unique_label), 
             "%s-counter-%u", base_label, g_key_derivation_counter++);
    
    return derive_key_from_huk(unique_label, derived_key, key_length);
}
```

### Timing Attack Prevention
```c
/* Constant-time comparison function */
static bool secure_compare(const uint8_t* a, const uint8_t* b, size_t length)
{
    volatile uint8_t result = 0;
    
    for (size_t i = 0; i < length; i++) {
        result |= a[i] ^ b[i];
    }
    
    return (result == 0);
}

/* Timing attack resistant decryption */
static psa_status_t secure_decrypt_verify(const uint8_t* encrypted_data,
                                         size_t encrypted_size,
                                         const uint8_t* expected_magic,
                                         uint8_t* decrypted_data,
                                         size_t* decrypted_size)
{
    psa_status_t status;
    uint8_t temp_buffer[TEMP_BUFFER_SIZE];
    
    /* Perform decryption */
    status = decrypt_model_cbc(encrypted_data, encrypted_size, 
                              derived_key, temp_buffer, decrypted_size);
    
    /* Constant-time magic header validation */
    bool magic_valid = secure_compare(temp_buffer, expected_magic, 4);
    
    /* Conditional copy (prevent timing information leakage) */
    if (status == PSA_SUCCESS && magic_valid) {
        memcpy(decrypted_data, temp_buffer, *decrypted_size);
    } else {
        /* Ensure same time taken on failure */
        memset(decrypted_data, 0, *decrypted_size);
        status = PSA_ERROR_INVALID_SIGNATURE;
    }
    
    /* Clear temporary buffer */
    memset(temp_buffer, 0, sizeof(temp_buffer));
    
    return status;
}
```

### DEV_MODE Security Warning
```c
#ifdef DEV_MODE
/* DEV_MODE warning message */
#warning "DEV_MODE is active - HUK keys will be exposed!"
#warning "Never use DEV_MODE in production environments!"

static void dev_mode_security_warning(void)
{
    LOG_ERRSIG("================================================\\n");
    LOG_ERRSIG("WARNING: DEV_MODE is active!\\n");
    LOG_ERRSIG("HUK-derived keys will be exposed via debug output.\\n");
    LOG_ERRSIG("This build is NOT suitable for production use!\\n");
    LOG_ERRSIG("================================================\\n");
}

/* Allow key extraction only in DEV_MODE */
static psa_status_t handle_get_model_key(psa_msg_t *msg)
{
    dev_mode_security_warning();
    
    /* Actual key extraction logic */
    return extract_huk_key_for_debugging(msg);
}
#else
/* Disable key extraction in production mode */
static psa_status_t handle_get_model_key(psa_msg_t *msg)
{
    LOG_ERRSIG("[Security] Key extraction disabled in production\\n");
    return PSA_ERROR_NOT_SUPPORTED;
}
#endif
```

## Advanced Encryption Techniques

### Multi-Model Support
```c
/* Derive unique key per model */
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
    
    /* Create unique label including model name and version */
    snprintf(full_label, sizeof(full_label), 
             "pico2w-tinymaix-%s-aes128-%s", model_name, version);
    
    return derive_key_from_huk(full_label, derived_key, 16);
}

/* Example usage */
// derive_model_specific_key("mnist", "v1.0", key_buffer);
// derive_model_specific_key("cifar10", "v2.1", key_buffer);
```

### Key Rotation
```c
/* Version management for periodic key rotation */
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

/* Key version management */
static uint32_t get_current_key_version(void)
{
    /* Return current active key version */
    /* Can be implemented using RTC or secure counter */
    return 1; /* Default version */
}
```

## Troubleshooting

### Common Encryption Issues

#### 1. Key File Errors
```bash
# Symptom: Key file read failure
Error: Invalid key file format. Expected 16 bytes, got 32

# Cause: Incorrect xxd command
echo "40c962d66a1fa40346cac8b7e612741e" | xxd -r -p > key.bin  # Correct
echo "40c962d66a1fa40346cac8b7e612741e" > key.bin             # Incorrect (saves as text)

# Solution: Regenerate with correct method
rm models/model_key_psa.bin
echo "40c962d66a1fa40346cac8b7e612741e" | xxd -r -p > models/model_key_psa.bin
```

#### 2. Decryption Failure
```bash
# Symptom: Padding error during decryption
[TinyMaix] Invalid PKCS7 padding

# Cause: Incorrect key used
# Solution: Re-extract HUK key
./build.sh DEV_MODE
# Copy new key from serial and re-encrypt
```

#### 3. Model Load Failure
```bash
# Symptom: TinyMaix model load error
[TinyMaix] Model load failed: -1

# Cause: Decrypted data is not a valid TinyMaix format
# Solution: Verify original model file
python3 test_cbc_decrypt.py  # Decryption test
hexdump -C models/decrypted_model_test.bin | head  # Check first few bytes
# First 4 bytes should be "TMDL" (0x544D444C)
```

## Performance Considerations

### Encryption Performance Measurement
```python
# Encryption performance benchmark
import time

def benchmark_encryption():
    """Measure model encryption performance"""
    
    # Test data of various sizes
    test_sizes = [1024, 4096, 16384, 65536]  # 1KB ~ 64KB
    
    for size in test_sizes:
        test_data = b'\\x00' * size
        key = b'\\x40\\xc9\\x62\\xd6\\x6a\\x1f\\xa4\\x03\\x46\\xca\\xc8\\xb7\\xe6\\x12\\x74\\xe1'
        
        # Measure encryption time
        start_time = time.time()
        
        for _ in range(100):  # Repeat 100 times
            encryptor = TinyMAIXModelEncryptor()
            encrypted_data, iv = encryptor.encrypt_model(test_data, key)
        
        end_time = time.time()
        
        avg_time = (end_time - start_time) / 100 * 1000  # ms
        throughput = size / (avg_time / 1000) / 1024  # KB/s
        
        print(f"Size: {size:5d} bytes, Avg time: {avg_time:.2f} ms, Throughput: {throughput:.1f} KB/s")

# Example results:
# Size:  1024 bytes, Avg time: 0.15 ms, Throughput: 6826.7 KB/s
# Size:  4096 bytes, Avg time: 0.32 ms, Throughput: 12800.0 KB/s
# Size: 16384 bytes, Avg time: 1.05 ms, Throughput: 15603.8 KB/s
# Size: 65536 bytes, Avg time: 3.89 ms, Throughput: 16845.0 KB/s
```

### Secure Partition Decryption Performance
```c
/* Decryption performance measurement */
static void benchmark_decryption(void)
{
    uint32_t start_time, end_time;
    uint8_t test_key[16] = {0x40, 0xc9, 0x62, 0xd6, /* ... */};
    uint8_t decrypted_buffer[MODEL_BUFFER_SIZE];
    size_t decrypted_size = sizeof(decrypted_buffer);
    
    LOG_INFSIG("[Benchmark] Starting decryption performance test...\\n");
    
    start_time = osKernelGetTickCount();
    
    /* Decrypt 10 times */
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

/* Typical performance (RP2350 @150MHz):
 * - 16KB model: ~15-25ms decryption
 * - Throughput: ~800KB/s
 * - HUK key derivation: ~5-10ms
 */
```

## Next Steps

Once model encryption is complete, refer to the following documents:

- **[TinyMaix Integration](./tinymaix-integration.md)** - Using encrypted models
- **[Secure Partitions](./secure-partitions.md)** - Implementing secure services
- **[Testing Framework](./testing-framework.md)** - Writing encryption tests
- **[Troubleshooting](./troubleshooting.md)** - Resolving encryption-related issues