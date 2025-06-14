# Troubleshooting Guide

## Common Issues and Solutions

### Build Issues

#### 1. SPE Build Failures

**Error: TF-M platform not found**
```
CMake Error: Could not find platform definition for rpi/rp2350
```
**Cause**: TF-M platform configuration missing or incorrect path  
**Solution**:
```bash
# Check TF-M source path
ls pico2w-trusted-firmware-m/platform/ext/target/rpi/rp2350

# Verify build command
cmake -S ./tflm_spe -B build/spe -DTFM_PLATFORM=rpi/rp2350 ...
```

**Error: Manifest validation failed**
```
Error: Invalid manifest file: partitions/my_service/manifest.yaml
```
**Cause**: YAML syntax error or invalid partition configuration  
**Solution**:
```bash
# Validate YAML syntax
python3 -c "import yaml; yaml.safe_load(open('partitions/my_service/manifest.yaml'))"

# Check required fields
cat partitions/echo_service/echo_service_manifest.yaml  # Reference
```

**Error: Partition not found**
```
Error: TFM_PARTITION_MY_SERVICE is ON but partition not found
```
**Cause**: Partition not registered in manifest_list.yaml  
**Solution**:
```yaml
# Add to partitions/manifest_list.yaml
{
  "description": "My Service",
  "manifest": "partitions/my_service/my_service_manifest.yaml",
  "output_path": "partitions/my_service", 
  "conditional": "TFM_PARTITION_MY_SERVICE",
  "version_major": 0,
  "version_minor": 1,
  "pid": 446  # Choose unique PID
}
```

#### 2. NSPE Build Failures

**Error: CONFIG_SPE_PATH not found**
```
FATAL_ERROR CONFIG_SPE_PATH = build/spe/api_ns is not defined or incorrect
```
**Cause**: SPE build not completed or incorrect path  
**Solution**:
```bash
# Ensure SPE builds first
cmake --build build/spe -- -j8 install

# Check API files exist
ls build/spe/api_ns/interface/
```

**Error: Undefined reference to PSA functions**
```
undefined reference to `psa_connect'
```
**Cause**: Missing PSA API libraries in link  
**Solution**:
```cmake
# In CMakeLists.txt
target_link_libraries(my_target
    PRIVATE
        tfm_api_ns  # Add PSA API library
)
```

#### 3. Model Encryption Issues

**Error: Model encryption script failure**
```
FileNotFoundError: [Errno 2] No such file or directory: 'models/mnist_valid_q.h'
```
**Cause**: Missing model source file  
**Solution**:
```bash
# Check model file exists
ls models/mnist_valid_q.h

# Run encryption manually
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_model.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

### Runtime Issues

#### 1. Device Boot Problems

**Symptom: Device doesn't boot or no serial output**
**Debugging Steps**:
```bash
# Check UF2 files exist
ls build/spe/bin/bl2.uf2
ls build/spe/bin/tfm_s_ns_signed.uf2

# Try manual deployment
picotool info
picotool erase
picotool load build/spe/bin/bl2.uf2
picotool load build/spe/bin/tfm_s_ns_signed.uf2
picotool reboot

# Check serial connection
# Linux/Mac: screen /dev/ttyACM0 115200
# Windows: Use PuTTY or similar
```

#### 2. PSA Service Failures

**Error: PSA_ERROR_CONNECTION_REFUSED (-150)**
```
[Service Test] ✗ Connection failed: -150
```
**Cause**: Service not available or security policy rejection  
**Solution**:
```c
// Check service SID is correct
#define MY_SERVICE_SID 0x00000108U  // Must match manifest

// Verify partition is enabled
// In config_tflm.cmake:
set(TFM_PARTITION_MY_SERVICE ON CACHE BOOL "Enable My Service")

// Check non_secure_clients in manifest
"non_secure_clients": true
```

**Error: PSA_ERROR_INVALID_HANDLE (-136)**
```
[Service Test] ✗ Invalid handle: -136
```
**Cause**: Handle not properly obtained or already closed  
**Solution**:
```c
// Ensure handle is valid before use
psa_handle_t handle = psa_connect(SERVICE_SID, 1);
if (handle <= 0) {
    // Handle connection error
    return SERVICE_ERROR_CONNECTION_FAILED;
}

// Use handle for calls
psa_status_t status = psa_call(handle, operation, ...);

// Always close handle
psa_close(handle);
```

#### 3. TinyMaix Inference Issues

**Error: Model load failed**
```
[TinyMaix Test] ✗ Model load failed: -3
```
**Debugging Steps**:
```c
// Check in secure partition logs
INFO_UNPRIV("Model decryption status: %d\n", decrypt_status);
INFO_UNPRIV("TinyMaix load status: %d\n", tm_load_status);

// Common causes:
// 1. Wrong encryption key
// 2. Corrupted encrypted model
// 3. Insufficient memory
// 4. Invalid model format
```

**Error: Invalid model magic**
```
ERROR: Invalid model magic
```
**Cause**: Decryption failed or wrong key  
**Solution**:
```bash
# Verify model encryption
python3 test_cbc_decrypt.py

# Check key derivation in DEV_MODE
./build.sh DEV_MODE
# Look for "HUK-derived key:" in output

# Re-encrypt model with correct key
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

**Error: Inference failed**
```
[TinyMaix Test] ✗ Inference failed: -4
```
**Debugging Steps**:
```c
// Check model is loaded
if (!g_model_loaded) {
    return PSA_ERROR_BAD_STATE;
}

// Validate input dimensions
if (input_size != 28*28) {
    return PSA_ERROR_INVALID_ARGUMENT;
}

// Check TinyMaix error codes
if (tm_res == TM_ERR_OOM) {
    // Out of memory - increase stack size
}
```

### Memory Issues

#### 1. Stack Overflow

**Symptom: Random crashes or hard faults**
**Debugging**:
```yaml
# Increase stack size in manifest
stack_size: "0x4000"  # Increase from 0x2000
```

**Check Stack Usage**:
```c
// In secure partition
void check_stack_usage(void)
{
    extern uint32_t __stack_top__;
    extern uint32_t __stack_limit__;
    
    uint32_t stack_size = (uint32_t)&__stack_top__ - (uint32_t)&__stack_limit__;
    uint32_t current_sp;
    __asm volatile ("mov %0, sp" : "=r" (current_sp));
    uint32_t used = (uint32_t)&__stack_top__ - current_sp;
    
    INFO_UNPRIV("Stack usage: %d/%d bytes (%.1f%%)\n", 
                used, stack_size, (float)used * 100.0f / stack_size);
}
```

#### 2. Buffer Overflows

**Error: Secure fault or memory corruption**
**Prevention**:
```c
// Always validate buffer sizes
if (input_size > MAX_BUFFER_SIZE) {
    return PSA_ERROR_INVALID_ARGUMENT;
}

// Use safe string functions
size_t bytes_to_copy = MIN(src_size, dst_size - 1);
memcpy(dst, src, bytes_to_copy);
dst[bytes_to_copy] = '\0';

// Check psa_read return value
size_t bytes_read = psa_read(handle, 0, buffer, buffer_size);
if (bytes_read != expected_size) {
    return PSA_ERROR_COMMUNICATION_FAILURE;
}
```

### Crypto Issues

#### 1. Key Derivation Failures

**Error: psa_key_derivation_output_bytes failed: -135**
```
ERROR: psa_key_derivation_output_bytes failed: -135
```
**Cause**: HUK not available or wrong derivation setup  
**Solution**:
```c
// Check PSA crypto initialization
psa_status_t status = psa_crypto_init();
if (status != PSA_SUCCESS && status != PSA_ERROR_ALREADY_EXISTS) {
    return status;
}

// Verify HUK is available
// Check platform supports TFM_BUILTIN_KEY_ID_HUK

// Use correct derivation algorithm
status = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
```

#### 2. Decryption Failures

**Error: PKCS7 padding validation failed**
```
Invalid PKCS7 padding byte at position X
```
**Debugging**:
```c
// Check encryption/decryption key match
// Verify IV is correctly extracted from header
// Ensure CBC mode is used consistently

// Debug padding in DEV_MODE
#ifdef DEV_MODE
void debug_padding(const uint8_t* data, size_t size) {
    INFO_UNPRIV("Last 16 bytes: ");
    for (int i = 0; i < 16 && i < size; i++) {
        INFO_UNPRIV("%02x ", data[size - 16 + i]);
    }
    INFO_UNPRIV("\n");
}
#endif
```

### Communication Issues

#### 1. Serial Output Problems

**Symptom: No output or garbled text**
**Solutions**:
```bash
# Check baud rate (should be 115200)
# Try different serial terminal

# Linux/Mac
screen /dev/ttyACM0 115200
# or
minicom -D /dev/ttyACM0 -b 115200

# Check device enumeration
lsusb | grep Raspberry
```

#### 2. PSA Call Timeouts

**Symptom: Operations hang or timeout**
**Solutions**:
```c
// Implement timeout in client calls
// Check service is responding in main loop

// In service, ensure psa_reply is always called
switch (msg.type) {
    case MY_OPERATION:
        status = handle_operation(&msg);
        psa_reply(msg.handle, status);  // Always reply
        break;
    default:
        psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
        break;
}
```

## Debugging Techniques

### 1. Enable DEV_MODE

```bash
# Build with debug features
./build.sh DEV_MODE

# This enables:
# - HUK key exposure for debugging  
# - Additional debug logging
# - Key derivation validation
```

### 2. Increase Logging

```c
// In secure partitions
#define DEBUG_VERBOSE
#ifdef DEBUG_VERBOSE
#define DEBUG_LOG(fmt, ...) INFO_UNPRIV("[DEBUG] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...)
#endif

void debug_function(void) {
    DEBUG_LOG("Function entry\n");
    DEBUG_LOG("Parameter: %d\n", param);
    DEBUG_LOG("Function exit: %d\n", result);
}
```

### 3. Memory Inspection

```c
// Debug buffer contents
void debug_print_buffer(const char* name, const uint8_t* buf, size_t size) {
    INFO_UNPRIV("%s (%d bytes): ", name, size);
    for (size_t i = 0; i < size && i < 32; i++) {
        INFO_UNPRIV("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) INFO_UNPRIV("\n");
    }
    if (size > 32) INFO_UNPRIV("...");
    INFO_UNPRIV("\n");
}
```

### 4. Error Code Analysis

```c
// Decode PSA error codes
const char* psa_error_to_string(psa_status_t status) {
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
        default: return "UNKNOWN";
    }
}

// Usage
INFO_UNPRIV("PSA call failed: %s (%d)\n", psa_error_to_string(status), status);
```

## Performance Debugging

### 1. Timing Analysis

```c
// Measure operation timing
uint32_t start_time = osKernelGetTickCount();
psa_status_t status = psa_call(handle, operation, ...);
uint32_t end_time = osKernelGetTickCount();

INFO_UNPRIV("Operation took %d ms\n", end_time - start_time);
```

### 2. Memory Usage Tracking

```c
// Track memory allocation in partitions
static size_t total_allocated = 0;
static size_t peak_allocated = 0;

void* debug_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        total_allocated += size;
        if (total_allocated > peak_allocated) {
            peak_allocated = total_allocated;
        }
        INFO_UNPRIV("Allocated %d bytes, total: %d, peak: %d\n", 
                   size, total_allocated, peak_allocated);
    }
    return ptr;
}
```

### 3. Context Switch Analysis

```c
// Monitor TrustZone context switches
static uint32_t context_switch_count = 0;

// In NS side before PSA call
uint32_t before_switches = get_context_switch_count();
psa_status_t status = psa_call(...);
uint32_t after_switches = get_context_switch_count();

INFO_UNPRIV("PSA call required %d context switches\n", 
           after_switches - before_switches);
```

## Build Environment Issues

### 1. Toolchain Problems

**Error: arm-none-eabi-gcc not found**
```bash
# Install GNU ARM toolchain
# Ubuntu/Debian:
sudo apt-get install gcc-arm-none-eabi

# macOS:
brew install arm-none-eabi-gcc

# Or download from ARM website
```

**Error: CMake version too old**
```bash
# Update CMake (minimum 3.21 required)
pip3 install cmake --upgrade
```

### 2. Python Environment

**Error: Module not found**
```bash
# Install required Python packages
pip3 install pycryptodome pyyaml

# Or use requirements file if available
pip3 install -r requirements.txt
```

### 3. Git Submodule Issues

**Error: Submodule directories empty**
```bash
# Initialize and update submodules
git submodule update --init --recursive

# Or clone with submodules
git clone --recursive <repository_url>
```

## Hardware Debugging

### 1. Device Detection

```bash
# Check device is detected
lsusb | grep "Raspberry Pi"

# Check mount point
ls /media/*/RPI-RP2/ 2>/dev/null || echo "Device not mounted"

# Force device into BOOTSEL mode
# Hold BOOTSEL button while connecting USB
```

### 2. Picotool Issues

```bash
# Install picotool
sudo apt-get install picotool

# Check device in bootloader mode
picotool info

# If device not detected:
sudo picotool erase
sudo picotool load file.uf2
```

### 3. Serial Connection Problems

```bash
# Check device permissions
ls -l /dev/ttyACM*
# Add user to dialout group if needed:
sudo usermod -a -G dialout $USER

# Test serial connection
echo "test" > /dev/ttyACM0
cat /dev/ttyACM0
```

## Recovery Procedures

### 1. Corrupted Device Recovery

```bash
# Force into BOOTSEL mode
# 1. Disconnect USB
# 2. Hold BOOTSEL button
# 3. Connect USB while holding
# 4. Release BOOTSEL

# Erase everything
picotool erase

# Reload bootloader
picotool load build/spe/bin/bl2.uf2

# Reload main firmware
picotool load build/spe/bin/tfm_s_ns_signed.uf2

# Reboot
picotool reboot
```

### 2. Build Recovery

```bash
# Complete clean rebuild
rm -rf build/
./build.sh clean

# Rebuild just SPE
rm -rf build/spe
cmake -S ./tflm_spe -B build/spe ...
cmake --build build/spe -- -j8 install

# Rebuild just NSPE
rm -rf build/nspe  
cmake -S ./tflm_ns -B build/nspe ...
cmake --build build/nspe -- -j8
```

### 3. Git Recovery

```bash
# Reset to clean state
git clean -fdx
git reset --hard HEAD

# Update submodules
git submodule update --init --recursive
```

## Getting Help

### 1. Log Collection

```bash
# Collect build logs
./build.sh 2>&1 | tee build.log

# Collect runtime logs
# Connect serial terminal and save output

# Collect system info
uname -a
arm-none-eabi-gcc --version
cmake --version
python3 --version
```

### 2. Issue Reporting

When reporting issues, include:
- **Build logs**: Complete build output
- **Runtime logs**: Serial output from device
- **Environment**: OS, toolchain versions
- **Steps to reproduce**: Exact commands used
- **Expected vs actual behavior**

### 3. Common Resources

- **TF-M Documentation**: https://tf-m-user-guide.trustedfirmware.org/
- **PSA API Specification**: https://developer.arm.com/architectures/security-architectures/platform-security-architecture
- **Raspberry Pi Pico Documentation**: https://www.raspberrypi.org/documentation/microcontrollers/
- **Project Repository**: Issues and discussions

## Next Steps

After resolving issues:
- **[TF-M Architecture](./tfm-architecture.md)**: Understand the system better
- **[Secure Partitions](./secure-partitions.md)**: Implement robust services  
- **[Testing Framework](./testing-framework.md)**: Add comprehensive tests