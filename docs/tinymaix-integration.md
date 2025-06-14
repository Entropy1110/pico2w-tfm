# TinyMaix Integration Guide

## Overview

TinyMaix is a lightweight neural network inference library optimized for embedded systems. This project integrates TinyMaix into a secure partition, providing hardware-protected machine learning inference using ARM TrustZone technology.

## Architecture

### Secure ML Inference Pipeline
```
Non-Secure Application
         │
         ▼
   PSA Client API
         │
         ▼
   TrustZone Boundary
         │
         ▼
  TinyMaix Secure Partition
         │
    ┌────▼────┐
    │ Decrypt │ ← HUK-derived AES key
    │  Model  │
    └────┬────┘
         │
    ┌────▼────┐
    │ TinyMaix│
    │ Inference│
    └────┬────┘
         │
         ▼
   Classification Result
```

### Security Features
- **Model Encryption**: AES-128-CBC with PKCS7 padding
- **HUK-based Key Derivation**: Hardware-unique encryption keys
- **Secure Processing**: ML inference isolated in secure partition
- **Memory Protection**: Model data never exposed to non-secure world

## TinyMaix Secure Partition

### Partition Configuration
- **PID**: 445
- **SID**: 0x00000107
- **Stack Size**: 8KB (0x2000)
- **Type**: Application Root of Trust (APP-ROT)
- **Connection**: Connection-based service

### Service Operations
The TinyMaix partition provides the following IPC operations:

#### 1. Load Encrypted Model
```c
#define TINYMAIX_IPC_LOAD_ENCRYPTED_MODEL (0x1002U)
```
- Loads built-in encrypted MNIST model
- Derives decryption key from HUK
- Validates model integrity
- Initializes TinyMaix inference engine

#### 2. Run Inference
```c
#define TINYMAIX_IPC_RUN_INFERENCE (0x1003U)
```
- Executes inference with built-in test image
- Supports custom 28x28 MNIST images
- Returns classification result (0-9)

#### 3. Get Model Key (DEV_MODE)
```c
#ifdef DEV_MODE
#define TINYMAIX_IPC_GET_MODEL_KEY (0x1004U)
#endif
```
- **DEBUG ONLY**: Exposes HUK-derived encryption key
- Used for debugging encryption/decryption
- **NEVER enable in production**

## Client API Usage

### Basic Inference Workflow

```c
#include "tfm_tinymaix_inference_defs.h"

void run_mnist_inference(void)
{
    tfm_tinymaix_status_t status;
    int predicted_class;
    
    /* Step 1: Load encrypted model */
    status = tfm_tinymaix_load_encrypted_model();
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("Model load failed: %d\n", status);
        return;
    }
    
    /* Step 2: Run inference with built-in image */
    status = tfm_tinymaix_run_inference(&predicted_class);
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("Predicted digit: %d\n", predicted_class);
    } else {
        printf("Inference failed: %d\n", status);
    }
}
```

### Custom Image Inference

```c
void run_custom_inference(void)
{
    tfm_tinymaix_status_t status;
    int predicted_class;
    
    /* Create custom 28x28 MNIST image */
    uint8_t custom_image[28*28];
    memset(custom_image, 0, sizeof(custom_image));
    
    /* Draw a simple digit pattern */
    for (int y = 10; y < 18; y++) {
        for (int x = 10; x < 18; x++) {
            custom_image[y * 28 + x] = 255; /* White pixel */
        }
    }
    
    /* Load model first */
    status = tfm_tinymaix_load_encrypted_model();
    if (status != TINYMAIX_STATUS_SUCCESS) {
        printf("Model load failed: %d\n", status);
        return;
    }
    
    /* Run inference with custom image */
    status = tfm_tinymaix_run_inference_with_data(custom_image, 
                                                  sizeof(custom_image), 
                                                  &predicted_class);
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("Custom image predicted digit: %d\n", predicted_class);
    } else {
        printf("Custom inference failed: %d\n", status);
    }
}
```

### DEV_MODE Key Debugging

```c
#ifdef DEV_MODE
void debug_model_key(void)
{
    tfm_tinymaix_status_t status;
    uint8_t key_buffer[16]; /* 128-bit key */
    
    /* Get HUK-derived model key */
    status = tfm_tinymaix_get_model_key(key_buffer, sizeof(key_buffer));
    if (status == TINYMAIX_STATUS_SUCCESS) {
        printf("HUK-derived key: ");
        for (int i = 0; i < 16; i++) {
            printf("%02x", key_buffer[i]);
        }
        printf("\n");
    } else {
        printf("Key retrieval failed: %d\n", status);
    }
}
#endif
```

## Model Format and Encryption

### TinyMaix Model Structure
The current implementation uses an MNIST classification model:
- **Input**: 28x28 grayscale images (784 bytes)
- **Output**: 10-class probability distribution (digits 0-9)
- **Format**: Quantized neural network optimized for embedded inference

### Encryption Workflow
```bash
# Automatic encryption during build
python3 tools/tinymaix_model_encryptor.py \
    --input models/mnist_valid_q.h \
    --output models/encrypted_mnist_model_psa.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

### Encrypted Model Header Structure
```c
typedef struct {
    uint32_t magic;              // "TMAX" (0x54 0x4D 0x41 0x58)
    uint32_t version;            // Format version = 3 for CBC
    uint32_t original_size;      // Size of decrypted model data
    uint8_t iv[16];              // AES-CBC IV (128 bits)
    /* Encrypted model data follows */
} encrypted_tinymaix_header_cbc_t;
```

## Key Derivation and Decryption

### HUK-based Key Derivation
The secure partition derives encryption keys from the Hardware Unique Key (HUK):

```c
/* Key derivation using PSA crypto API */
static psa_status_t derive_key_from_huk(const char *label, 
                                        uint8_t *derived, 
                                        size_t derived_len)
{
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t status;
    
    /* Setup HKDF with SHA-256 */
    status = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (status != PSA_SUCCESS) return status;
    
    /* Empty salt */
    status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT,
                                            NULL, 0);
    if (status != PSA_SUCCESS) goto cleanup;
    
    /* Use HUK as secret input */
    status = psa_key_derivation_input_key(&op, PSA_KEY_DERIVATION_INPUT_SECRET,
                                          TFM_BUILTIN_KEY_ID_HUK);
    if (status != PSA_SUCCESS) goto cleanup;
    
    /* Use label as info input */
    status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO,
                                            (const uint8_t*)label, strlen(label));
    if (status != PSA_SUCCESS) goto cleanup;
    
    /* Derive key material */
    status = psa_key_derivation_output_bytes(&op, derived, derived_len);
    
cleanup:
    psa_key_derivation_abort(&op);
    return status;
}
```

### Decryption Process
```c
static psa_status_t decrypt_model(const uint8_t* encrypted_data, 
                                  size_t encrypted_size)
{
    /* Parse encryption header */
    const encrypted_tinymaix_header_cbc_t* header = 
        (const encrypted_tinymaix_header_cbc_t*)encrypted_data;
    
    /* Validate header */
    if (header->magic != ENCRYPTED_HEADER_MAGIC || header->version != 3) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* Derive decryption key */
    const char *huk_label = "pico2w-tinymaix-model-aes128-v1.0";
    uint8_t encryption_key[16];
    if (derive_key_from_huk(huk_label, encryption_key, 16) != PSA_SUCCESS) {
        return PSA_ERROR_GENERIC_ERROR;
    }
    
    /* Setup PSA crypto for CBC decryption */
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = 0;
    
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    
    /* Import key */
    psa_status_t status = psa_import_key(&attributes, encryption_key, 16, &key_id);
    if (status != PSA_SUCCESS) return status;
    
    /* Decrypt with CBC */
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
    status = psa_cipher_decrypt_setup(&operation, key_id, PSA_ALG_CBC_NO_PADDING);
    if (status != PSA_SUCCESS) goto cleanup;
    
    /* Set IV from header */
    status = psa_cipher_set_iv(&operation, header->iv, 16);
    if (status != PSA_SUCCESS) goto cleanup;
    
    /* Decrypt model data */
    const uint8_t* ciphertext = encrypted_data + ENCRYPTED_HEADER_CBC_SIZE;
    size_t ciphertext_size = encrypted_size - ENCRYPTED_HEADER_CBC_SIZE;
    
    size_t output_length = 0;
    status = psa_cipher_update(&operation, ciphertext, ciphertext_size,
                              g_decrypted_model, TFM_TINYMAIX_MAX_MODEL_SIZE, 
                              &output_length);
    
    if (status == PSA_SUCCESS) {
        size_t final_length = 0;
        status = psa_cipher_finish(&operation, 
                                  g_decrypted_model + output_length,
                                  TFM_TINYMAIX_MAX_MODEL_SIZE - output_length,
                                  &final_length);
        output_length += final_length;
    }
    
    /* Remove PKCS7 padding manually */
    if (status == PSA_SUCCESS) {
        uint8_t padding_length = g_decrypted_model[output_length - 1];
        g_decrypted_size = output_length - padding_length;
    }
    
cleanup:
    psa_cipher_abort(&operation);
    psa_destroy_key(key_id);
    return status;
}
```

## TinyMaix Integration Details

### Model Loading
```c
static tm_err_t load_tinymaix_model(void)
{
    /* TinyMaix model loading */
    tm_err_t tm_res = tm_load(&g_mdl, g_decrypted_model, static_main_buf, 
                              layer_cb, &g_in);
    
    if (tm_res != TM_OK) {
        if (tm_res == TM_ERR_MAGIC) {
            INFO_UNPRIV("ERROR: Invalid model magic\n");
        } else if (tm_res == TM_ERR_MDLTYPE) {
            INFO_UNPRIV("ERROR: Wrong model type\n");
        } else if (tm_res == TM_ERR_OOM) {
            INFO_UNPRIV("ERROR: Out of memory\n");
        }
        return tm_res;
    }
    
    /* Log model information */
    INFO_UNPRIV("Model loaded successfully:\n");
    INFO_UNPRIV("  - Input dims: %dx%dx%d\n", 
                g_mdl.b->in_dims[1], g_mdl.b->in_dims[2], g_mdl.b->in_dims[3]);
    INFO_UNPRIV("  - Output dims: %dx%dx%d\n", 
                g_mdl.b->out_dims[1], g_mdl.b->out_dims[2], g_mdl.b->out_dims[3]);
    INFO_UNPRIV("  - Layer count: %d\n", g_mdl.b->layer_cnt);
    
    return TM_OK;
}
```

### Inference Execution
```c
static int run_tinymaix_inference(const uint8_t* image_data)
{
    /* Prepare input tensor */
    g_in_uint8.dims = 3;
    g_in_uint8.h = 28;
    g_in_uint8.w = 28;
    g_in_uint8.c = 1;
    g_in_uint8.data = (mtype_t*)image_data;
    
    /* Preprocess input (quantization) */
    tm_err_t tm_res;
    #if (TM_MDL_TYPE == TM_MDL_INT8) || (TM_MDL_TYPE == TM_MDL_INT16)
        tm_res = tm_preprocess(&g_mdl, TMPP_UINT2INT, &g_in_uint8, &g_in);
    #else
        tm_res = tm_preprocess(&g_mdl, TMPP_UINT2FP01, &g_in_uint8, &g_in);
    #endif
    
    if (tm_res != TM_OK) {
        return -1;
    }
    
    /* Run inference */
    tm_res = tm_run(&g_mdl, &g_in, g_outs);
    if (tm_res != TM_OK) {
        return -1;
    }
    
    /* Parse output to get classification result */
    return parse_output(g_outs);
}

static int parse_output(tm_mat_t* outs)
{
    tm_mat_t out = outs[0];
    float* data = out.dataf;
    float maxp = 0;
    int maxi = -1;
    
    /* Find class with highest probability */
    for(int i = 0; i < 10; i++){
        if(data[i] > maxp) {
            maxi = i;
            maxp = data[i];
        }
    }
    return maxi;
}
```

## Performance Considerations

### Memory Usage
- **Model Size**: ~1.4KB encrypted MNIST model
- **Stack Usage**: 8KB partition stack
- **Static Buffers**: 
  - Main buffer: 1464 bytes
  - Sub buffer: 512 bytes
  - Decrypted model: 4KB maximum

### Inference Performance
- **Latency**: Typically <100ms for 28x28 MNIST inference
- **Throughput**: Limited by TrustZone context switching overhead
- **Memory Efficiency**: Static allocation, no dynamic memory

### Optimization Tips
1. **Batch Processing**: Process multiple images in single PSA call
2. **Model Caching**: Keep model loaded between inferences
3. **Buffer Reuse**: Reuse static buffers for multiple inferences
4. **Quantization**: Use INT8 quantized models for faster inference

## Security Considerations

### Threat Model
- **Model Protection**: Encrypted models protect IP and prevent tampering
- **Key Security**: HUK-derived keys provide hardware-backed protection
- **Isolation**: TrustZone ensures inference runs in secure environment
- **Side Channel Protection**: Secure partition mitigates some timing attacks

### Best Practices
1. **Model Validation**: Always validate decrypted model magic and checksums
2. **Input Sanitization**: Validate image data dimensions and ranges
3. **Memory Clearing**: Clear sensitive data after use
4. **Error Handling**: Fail securely without leaking information

### DEV_MODE Security
- **Production Risk**: DEV_MODE exposes encryption keys
- **Debug Only**: Use only for development and debugging
- **Build Separation**: Separate production and development build pipelines

## Testing and Validation

### Unit Tests
```c
void test_tinymaix_basic_functionality(void)
{
    /* Test model loading */
    tfm_tinymaix_status_t status = tfm_tinymaix_load_encrypted_model();
    assert(status == TINYMAIX_STATUS_SUCCESS);
    
    /* Test built-in inference */
    int predicted_class;
    status = tfm_tinymaix_run_inference(&predicted_class);
    assert(status == TINYMAIX_STATUS_SUCCESS);
    assert(predicted_class >= 0 && predicted_class <= 9);
    
    /* Test custom image inference */
    uint8_t custom_image[28*28];
    /* ... create test pattern ... */
    status = tfm_tinymaix_run_inference_with_data(custom_image, 
                                                  sizeof(custom_image),
                                                  &predicted_class);
    assert(status == TINYMAIX_STATUS_SUCCESS);
}
```

### Integration Tests
- **End-to-End**: Full encrypt → decrypt → inference pipeline
- **Performance**: Measure inference latency and memory usage
- **Security**: Verify key derivation and model protection
- **Error Handling**: Test with corrupted models and invalid inputs

## Adding New Models

### Step 1: Model Preparation
1. **Convert to TinyMaix format**: Use TinyMaix tools to convert your model
2. **Optimize for embedded**: Quantize and optimize for target hardware
3. **Generate header**: Create C header file with model data array

### Step 2: Model Encryption
```bash
# Encrypt new model
python3 tools/tinymaix_model_encryptor.py \
    --input models/my_new_model.h \
    --output models/encrypted_my_model.bin \
    --key-file models/model_key_psa.bin \
    --generate-c-header
```

### Step 3: Integration
1. **Update includes**: Replace model header include in partition code
2. **Adjust buffer sizes**: Update buffer sizes if needed for larger models
3. **Modify parsing**: Update output parsing for different model outputs
4. **Test thoroughly**: Validate encryption, decryption, and inference

## Troubleshooting

### Common Issues

#### Model Loading Failures
```
ERROR: Invalid model magic
```
- **Cause**: Decryption failed or wrong encryption key
- **Solution**: Verify key derivation label matches encryption key

#### Memory Issues
```
ERROR: Out of memory
```
- **Cause**: Model too large for available buffers
- **Solution**: Increase buffer sizes in partition manifest and code

#### Inference Failures
```
Inference failed: -4
```
- **Cause**: Invalid input data or preprocessing failure
- **Solution**: Validate input dimensions and data ranges

### Debug Techniques
1. **Enable DEV_MODE**: Use `./build.sh DEV_MODE` to access key debugging
2. **Increase Logging**: Add INFO_UNPRIV messages for debugging
3. **Test Decryption**: Use Python scripts to validate decryption offline
4. **Memory Inspection**: Check stack usage and buffer overflow

## Next Steps

- **[Model Encryption Guide](./model-encryption.md)**: Detailed encryption workflow
- **[PSA API Reference](./psa-api.md)**: PSA client-service communication
- **[Troubleshooting Guide](./troubleshooting.md)**: Common issues and solutions