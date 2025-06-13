/*
 * Copyright (c) 2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "psa/service.h"
#include "psa/crypto.h"
#include "psa_manifest/tinymaix_inference_manifest.h"
#include "tfm_log_unpriv.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "tm_port.h"
#include "tinymaix.h"
#include "../../models/encrypted_mnist_model_psa.h"  /* Changed to match PSA encrypted model header */

/* Maximum model size */
#define TFM_TINYMAIX_MAX_MODEL_SIZE 4096

/* TinyMaix IPC Message Types - matching original implementation */
#define TINYMAIX_IPC_LOAD_MODEL          (0x1001U)  /* Using built-in encrypted model */
#define TINYMAIX_IPC_LOAD_ENCRYPTED_MODEL (0x1002U)  /* For future custom encrypted models */
#define TINYMAIX_IPC_RUN_INFERENCE       (0x1003U)

/* Encrypted TinyMAIX model header structure for CBC */
typedef struct {
    uint32_t magic;              // "TMAX" (0x54 0x4D 0x41 0x58)
    uint32_t version;            // Format version = 3 for CBC
    uint32_t original_size;      // Size of decrypted model data
    uint8_t iv[16];              // AES-CBC IV (128 bits)
} __attribute__((packed)) encrypted_tinymaix_header_cbc_t;

#define ENCRYPTED_HEADER_MAGIC 0x58414D54  // "TMAX" in little endian
#define ENCRYPTED_HEADER_CBC_SIZE 28       // 4+4+4+16

/* Global TinyMaix objects */
static tm_mdl_t g_mdl;
static tm_mat_t g_in_uint8;
static tm_mat_t g_in;
static tm_mat_t g_outs[1];
static int g_model_loaded = 0;
#define MDL_BUF_LEN (1464)
#define LBUF_LEN (1424)
static uint8_t static_main_buf[MDL_BUF_LEN];
static uint8_t static_sub_buf[512];

/* Shared buffer for model processing */
static uint8_t shared_model_buffer[TFM_TINYMAIX_MAX_MODEL_SIZE];

/* Global decrypted model buffer */
static uint8_t g_decrypted_model[TFM_TINYMAIX_MAX_MODEL_SIZE];
static size_t g_decrypted_size = 0;

/* AES-128 encryption key (16 bytes) - PSA crypto test key */
static const uint8_t encryption_key[16] = {
    0x40, 0xc9, 0x62, 0xd6, 0x6a, 0x1f, 0xa4, 0x03,
    0x46, 0xca, 0xc8, 0xb7, 0xe6, 0x12, 0x74, 0xe1
};

/* MNIST test image - digit "2" */
static uint8_t mnist_pic[28*28]={
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,116,125,171,255,255,150, 93,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,169,253,253,253,253,253,253,218, 30,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,169,253,253,253,213,142,176,253,253,122,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0, 52,250,253,210, 32, 12,  0,  6,206,253,140,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0, 77,251,210, 25,  0,  0,  0,122,248,253, 65,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0, 31, 18,  0,  0,  0,  0,209,253,253, 65,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,117,247,253,198, 10,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 76,247,253,231, 63,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,128,253,253,144,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,176,246,253,159, 12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 25,234,253,233, 35,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,198,253,253,141,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0, 78,248,253,189, 12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0, 19,200,253,253,141,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,134,253,253,173, 12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,248,253,253, 25,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,248,253,253, 43, 20, 20, 20, 20,  5,  0,  5, 20, 20, 37,150,150,150,147, 10,  0,
  0,  0,  0,  0,  0,  0,  0,  0,248,253,253,253,253,253,253,253,168,143,166,253,253,253,253,253,253,253,123,  0,
  0,  0,  0,  0,  0,  0,  0,  0,174,253,253,253,253,253,253,253,253,253,253,253,249,247,247,169,117,117, 57,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,118,123,123,123,166,253,253,253,155,123,123, 41,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

/* Layer callback function */
static tm_err_t layer_cb(tm_mdl_t* mdl, tml_head_t* lh)
{
    return TM_OK;
}

/* Parse output function */
static int parse_output(tm_mat_t* outs)
{
    tm_mat_t out = outs[0];
    float* data = out.dataf;
    float maxp = 0;
    int maxi = -1;
    
    for(int i = 0; i < 10; i++){
        if(data[i] > maxp) {
            maxi = i;
            maxp = data[i];
        }
    }
    return maxi;
}

static psa_status_t decrypt_model_to_global(const uint8_t* encrypted_data, size_t encrypted_size)
{
    INFO_UNPRIV("=== PSA CBC DECRYPTION WITH MANUAL PADDING REMOVAL ===\n");
    
    /* Basic validation */
    if (!encrypted_data) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    INFO_UNPRIV("Input package size: %d bytes\n", encrypted_size);
    
    /* Parse header */
    if (encrypted_size < ENCRYPTED_HEADER_CBC_SIZE) {
        INFO_UNPRIV("Package too small for CBC header\n");
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    const encrypted_tinymaix_header_cbc_t* header = (const encrypted_tinymaix_header_cbc_t*)encrypted_data;
    
    INFO_UNPRIV("PSA CBC Header:\n");
    INFO_UNPRIV("  - Magic: 0x%08x\n", header->magic);
    INFO_UNPRIV("  - Version: %u\n", header->version);
    INFO_UNPRIV("  - Original size: %u\n", header->original_size);
    
    /* Validate header */
    if (header->magic != ENCRYPTED_HEADER_MAGIC || header->version != 3) {
        INFO_UNPRIV("Invalid header\n");
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    if (header->original_size > TFM_TINYMAIX_MAX_MODEL_SIZE) {
        INFO_UNPRIV("Output size too large\n");
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }
    
    /* Get ciphertext */
    const size_t ciphertext_size = encrypted_size - ENCRYPTED_HEADER_CBC_SIZE;
    const uint8_t* ciphertext = encrypted_data + ENCRYPTED_HEADER_CBC_SIZE;
    
    if (ciphertext_size == 0) {
        INFO_UNPRIV("No ciphertext found!\n");
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    /* Initialize PSA Crypto */
    psa_status_t crypto_status = psa_crypto_init();
    if (crypto_status != PSA_SUCCESS && crypto_status != PSA_ERROR_ALREADY_EXISTS) {
        INFO_UNPRIV("PSA crypto init failed: %d\n", crypto_status);
        return crypto_status;
    }
    
    /* Set up key for CBC without automatic padding removal */
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = 0;
    
    /* Use basic CBC mode, handle padding manually */
    psa_algorithm_t cbc_alg = PSA_ALG_CBC_NO_PADDING;
    
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, cbc_alg);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    
    INFO_UNPRIV("Using CBC with manual padding, algorithm: 0x%08x\n", cbc_alg);
    
    psa_status_t status = psa_import_key(&attributes, encryption_key, 16, &key_id);
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV("Key import failed: %d\n", status);
        return status;
    }
    
    /* Create cipher operation */
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
    
    status = psa_cipher_decrypt_setup(&operation, key_id, cbc_alg);
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV("Cipher setup failed: %d\n", status);
        psa_destroy_key(key_id);
        return status;
    }
    
    /* Set IV */
    status = psa_cipher_set_iv(&operation, header->iv, 16);
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV("Set IV failed: %d\n", status);
        psa_cipher_abort(&operation);
        psa_destroy_key(key_id);
        return status;
    }
    
    /* Decrypt */
    size_t output_length = 0;
    status = psa_cipher_update(&operation, ciphertext, ciphertext_size, 
                              g_decrypted_model, TFM_TINYMAIX_MAX_MODEL_SIZE, &output_length);
    
    if (status == PSA_SUCCESS) {
        size_t final_length = 0;
        status = psa_cipher_finish(&operation, 
                                  g_decrypted_model + output_length, 
                                  TFM_TINYMAIX_MAX_MODEL_SIZE - output_length, 
                                  &final_length);
        output_length += final_length;
    }
    
    psa_cipher_abort(&operation);
    psa_destroy_key(key_id);
    
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV("Decryption failed: %d\n", status);
        return status;
    }
    
    INFO_UNPRIV("Raw decryption successful: %d bytes\n", output_length);
    
    /* Manual PKCS7 padding removal */
    if (output_length == 0) {
        INFO_UNPRIV("No decrypted data!\n");
        return PSA_ERROR_GENERIC_ERROR;
    }
    
    uint8_t padding_length = g_decrypted_model[output_length - 1];
    INFO_UNPRIV("PKCS7 padding length: %d\n", padding_length);
    
    if (padding_length == 0 || padding_length > 16 || padding_length > output_length) {
        INFO_UNPRIV("Invalid padding length: %d\n", padding_length);
        return PSA_ERROR_INVALID_PADDING;
    }
    
    /* Verify padding bytes */
    for (int i = 0; i < padding_length; i++) {
        if (g_decrypted_model[output_length - 1 - i] != padding_length) {
            INFO_UNPRIV("Invalid padding at offset %d\n", i);
            return PSA_ERROR_INVALID_PADDING;
        }
    }
    
    /* Remove padding */
    g_decrypted_size = output_length - padding_length;
    
    INFO_UNPRIV("=== CBC DECRYPTION SUCCESS ===\n");
    INFO_UNPRIV("Decrypted %d bytes (after removing %d padding bytes)\n", g_decrypted_size, padding_length);
    INFO_UNPRIV("Expected size: %d bytes\n", header->original_size);
    
    /* Verify size matches expected */
    if (g_decrypted_size != header->original_size) {
        INFO_UNPRIV("Size mismatch: got %d, expected %d\n", g_decrypted_size, header->original_size);
        return PSA_ERROR_GENERIC_ERROR;
    }
    
    /* Debug: Show first 16 bytes */
    INFO_UNPRIV("First 16 bytes: ");
    for (int i = 0; i < 16 && i < g_decrypted_size; i++) {
        INFO_UNPRIV("%02x ", g_decrypted_model[i]);
    }
    INFO_UNPRIV("\n");
    
    /* Verify TinyMaix model magic header */
    if (g_decrypted_size >= 4) {
        uint32_t model_magic = *(uint32_t*)g_decrypted_model;
        INFO_UNPRIV("Model magic: 0x%08x\n", model_magic);
        if (model_magic == 0x5849414D) { // "MAIX"
            INFO_UNPRIV("✅ Valid TinyMaix model detected!\n");
        } else {
            INFO_UNPRIV("❌ Invalid TinyMaix magic\n");
            return PSA_ERROR_GENERIC_ERROR;
        }
    }
    
    return PSA_SUCCESS;
}
/* Initialization function for the TinyMaix inference service */
psa_status_t tinymaix_inference_init(void)
{
    /* Initialize global state */
    g_model_loaded = 0;
    memset(&g_mdl, 0, sizeof(g_mdl));
    return PSA_SUCCESS;
}

/* Main entry point for the partition */
void tinymaix_inference_entry(void)
{
    psa_msg_t msg;
    psa_status_t status;
    size_t bytes_read;
    tm_err_t tm_res;
    int result;

    /* Service loop: continuously wait for and process messages */
    while (1) {
        /* Wait for a message from a client */
        psa_wait(TFM_TINYMAIX_INFERENCE_SIGNAL, PSA_BLOCK);
        
        /* Get the message */
        if (psa_get(TFM_TINYMAIX_INFERENCE_SIGNAL, &msg) != PSA_SUCCESS) {
            continue;
        }
        
        /* Process the message */
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                /* Connection request */
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case TINYMAIX_IPC_LOAD_ENCRYPTED_MODEL:  /* Changed from LOAD_ENCRYPTED_MODEL to match NS API */
                /* Process encrypted model load request using builtin encrypted model */
                INFO_UNPRIV("TINYMAIX_IPC_LOAD_ENCRYPTED_MODEL called (builtin encrypted)\n");
                
                /* Use builtin encrypted model data */
                INFO_UNPRIV("Using builtin model: size=%d bytes\n", encrypted_mdl_data_size);
                
                /* Validate model size before processing */
                if (encrypted_mdl_data_size > TFM_TINYMAIX_MAX_MODEL_SIZE) {
                    INFO_UNPRIV("Builtin model too large: %d > %d\n", encrypted_mdl_data_size, TFM_TINYMAIX_MAX_MODEL_SIZE);
                    status = PSA_ERROR_INSUFFICIENT_MEMORY;
                } else {
                    status = decrypt_model_to_global(encrypted_mdl_data_data, encrypted_mdl_data_size);
                }
                
                if (status == PSA_SUCCESS) {
                    /* Load decrypted model into TinyMaix */
                    INFO_UNPRIV("=== LOADING DECRYPTED MODEL INTO TINYMAIX ===\n");
                    INFO_UNPRIV("Decrypted model size: %d bytes\n", g_decrypted_size);
                    INFO_UNPRIV("Calling tm_load...\n");
                    
                    tm_res = tm_load(&g_mdl, g_decrypted_model, static_main_buf, layer_cb, &g_in);
                    
                    INFO_UNPRIV("tm_load returned: %d\n", tm_res);
                    if (tm_res != TM_OK) {
                        INFO_UNPRIV("TinyMaix model load failed: %d\n", tm_res);
                        if (tm_res == TM_ERR_MAGIC) {
                            INFO_UNPRIV("ERROR: Invalid model magic\n");
                        } else if (tm_res == TM_ERR_MDLTYPE) {
                            INFO_UNPRIV("ERROR: Wrong model type\n");
                        } else if (tm_res == TM_ERR_OOM) {
                            INFO_UNPRIV("ERROR: Out of memory\n");
                        }
                        status = PSA_ERROR_GENERIC_ERROR;
                        g_model_loaded = 0;
                    } else {
                        g_model_loaded = 1;
                        status = PSA_SUCCESS;
                        INFO_UNPRIV("=== TINYMAIX MODEL LOADED SUCCESSFULLY ===\n");
                        INFO_UNPRIV("Model info:\n");
                        INFO_UNPRIV("  - Input dims: %dx%dx%d\n", g_mdl.b->in_dims[1], g_mdl.b->in_dims[2], g_mdl.b->in_dims[3]);
                        INFO_UNPRIV("  - Output dims: %dx%dx%d\n", g_mdl.b->out_dims[1], g_mdl.b->out_dims[2], g_mdl.b->out_dims[3]);
                        INFO_UNPRIV("  - Layer count: %d\n", g_mdl.b->layer_cnt);
                        INFO_UNPRIV("  - Buffer size: %d\n", g_mdl.b->buf_size);
                    }
                } else {
                    INFO_UNPRIV("Builtin model decryption failed: %d\n", status);
                    g_model_loaded = 0;
                }
                
                /* Write success result if there's output space */
                if (status == PSA_SUCCESS && msg.out_size[0] >= sizeof(uint32_t)) {
                    uint32_t success_result = 0;
                    psa_write(msg.handle, 0, &success_result, sizeof(success_result));
                }
                
                psa_reply(msg.handle, status);
                break;
                
            case TINYMAIX_IPC_RUN_INFERENCE:
                /* Process inference request */
                INFO_UNPRIV("=== TINYMAIX_IPC_RUN_INFERENCE called ===\n");
                INFO_UNPRIV("Model loaded status: %d\n", g_model_loaded);
                
                if (!g_model_loaded) {
                    INFO_UNPRIV("ERROR: Model not loaded, cannot run inference\n");
                    status = PSA_ERROR_BAD_STATE;
                } else {
                    INFO_UNPRIV("Input data size: %d bytes\n", msg.in_size[0]);
                    /* Check if input data provided */
                    if (msg.in_size[0] == 28*28) {
                        /* Read custom input image data (28x28 = 784 bytes) */
                        bytes_read = psa_read(msg.handle, 0, mnist_pic, msg.in_size[0]);
                        if (bytes_read != msg.in_size[0]) {
                            status = PSA_ERROR_COMMUNICATION_FAILURE;
                        } else {
                            /* Prepare input data with custom image */
                            g_in_uint8.dims = 3;
                            g_in_uint8.h = 28;
                            g_in_uint8.w = 28; 
                            g_in_uint8.c = 1;
                            g_in_uint8.data = (mtype_t*)mnist_pic;
                            
                            /* Preprocess input */
                            #if (TM_MDL_TYPE == TM_MDL_INT8) || (TM_MDL_TYPE == TM_MDL_INT16) 
                                tm_res = tm_preprocess(&g_mdl, TMPP_UINT2INT, &g_in_uint8, &g_in); 
                            #else
                                tm_res = tm_preprocess(&g_mdl, TMPP_UINT2FP01, &g_in_uint8, &g_in); 
                            #endif
                            
                            if (tm_res != TM_OK) {
                                status = PSA_ERROR_GENERIC_ERROR;
                            } else {
                                /* Run inference */
                                tm_res = tm_run(&g_mdl, &g_in, g_outs);
                                if (tm_res != TM_OK) {
                                    status = PSA_ERROR_GENERIC_ERROR;
                                } else {
                                    /* Parse output */
                                    result = parse_output(g_outs);
                                    status = PSA_SUCCESS;
                                    
                                    /* Write result if there's output space */
                                    if (msg.out_size[0] >= sizeof(int)) {
                                        psa_write(msg.handle, 0, &result, sizeof(result));
                                    }
                                }
                            }
                        }
                    } else if (msg.in_size[0] == 0) {
                        INFO_UNPRIV("Using built-in test image for inference\n");
                        /* Use built-in test image */
                        g_in_uint8.dims = 3;
                        g_in_uint8.h = 28;
                        g_in_uint8.w = 28; 
                        g_in_uint8.c = 1;
                        g_in_uint8.data = (mtype_t*)mnist_pic;
                        
                        INFO_UNPRIV("Preprocessing built-in input...\n");
                        /* Preprocess input */
                        #if (TM_MDL_TYPE == TM_MDL_INT8) || (TM_MDL_TYPE == TM_MDL_INT16) 
                            tm_res = tm_preprocess(&g_mdl, TMPP_UINT2INT, &g_in_uint8, &g_in); 
                        #else
                            tm_res = tm_preprocess(&g_mdl, TMPP_UINT2FP01, &g_in_uint8, &g_in); 
                        #endif
                        
                        INFO_UNPRIV("Built-in preprocessing result: %d\n", tm_res);
                        if (tm_res != TM_OK) {
                            INFO_UNPRIV("ERROR: Built-in preprocessing failed\n");
                            status = PSA_ERROR_GENERIC_ERROR;
                        } else {
                            INFO_UNPRIV("Running built-in inference...\n");
                            /* Run inference */
                            tm_res = tm_run(&g_mdl, &g_in, g_outs);
                            INFO_UNPRIV("Built-in inference result: %d\n", tm_res);
                            if (tm_res != TM_OK) {
                                INFO_UNPRIV("ERROR: Built-in inference failed\n");
                                status = PSA_ERROR_GENERIC_ERROR;
                            } else {
                                INFO_UNPRIV("Parsing built-in output...\n");
                                /* Parse output */
                                result = parse_output(g_outs);
                                INFO_UNPRIV("Built-in predicted class: %d\n", result);
                                status = PSA_SUCCESS;
                            }
                        }
                    } else {
                        /* Invalid input size */
                        INFO_UNPRIV("ERROR: Invalid input size: %d (expected 0 or 784)\n", msg.in_size[0]);
                        status = PSA_ERROR_INVALID_ARGUMENT;
                    }
                }
                
                INFO_UNPRIV("=== INFERENCE COMPLETE ===\n");
                INFO_UNPRIV("Final status: %d\n", status);
                if (status == PSA_SUCCESS) {
                    INFO_UNPRIV("Final predicted class: %d\n", result);
                }
                
                psa_reply(msg.handle, status);
                break;
                
            case PSA_IPC_DISCONNECT:
                /* Client disconnected */
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            default:
                /* Unsupported message type */
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}