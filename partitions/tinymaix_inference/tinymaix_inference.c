/*
 * Copyright (c) 2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "psa/service.h"
#include "psa_manifest/tinymaix_inference_manifest.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "tm_port.h"     // Include our custom port configuration first
#include "tinymaix.h"    // Include TinyMaix header
#include "mnist_valid_q.h"  // Include MNIST model

/* Maximum model size */
#define TFM_TINYMAIX_MAX_MODEL_SIZE 4096

/* TinyMaix IPC Message Types - following echo_service pattern */
#define TINYMAIX_IPC_LOAD_MODEL          (0x1001U)
#define TINYMAIX_IPC_RUN_INFERENCE       (0x1003U)

/* Global TinyMaix objects - following MNIST example pattern */
static tm_mdl_t g_mdl;
static tm_mat_t g_in_uint8;
static tm_mat_t g_in;
static tm_mat_t g_outs[1];
static int g_model_loaded = 0;

/* Static memory buffers for TinyMaix - NO malloc in TF-M! */
static uint8_t static_main_buf[MDL_BUF_LEN];  // Use size from mnist_valid_q.h
static uint8_t static_sub_buf[512];           // Small sub buffer

/* MNIST test image - Using original MNIST example (digit "3") */
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

/* Layer callback function - from MNIST example */
static tm_err_t layer_cb(tm_mdl_t* mdl, tml_head_t* lh)
{
    return TM_OK;
}

/* Parse output function - from MNIST example */
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

/* Initialization function for the TinyMaix inference service */
psa_status_t tinymaix_inference_init(void)
{
    /* Initialize global state */
    g_model_loaded = 0;
    memset(&g_mdl, 0, sizeof(g_mdl));
    return PSA_SUCCESS;
}

/* Main entry point for the partition - following echo_service pattern exactly */
void tinymaix_inference_entry(void)
{
    psa_msg_t msg;
    psa_status_t status;
    uint8_t input_buffer[TFM_TINYMAIX_MAX_MODEL_SIZE];
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
                
            case TINYMAIX_IPC_LOAD_MODEL:
                /* Process load model request */
                /* Validate input parameters */
                if (msg.in_size[0] > TFM_TINYMAIX_MAX_MODEL_SIZE) {
                    status = PSA_ERROR_INVALID_ARGUMENT;
                } else if (msg.in_size[0] == 0) {
                    /* Use built-in MNIST model with static buffer */
                    tm_res = tm_load(&g_mdl, mdl_data, static_main_buf, layer_cb, &g_in);
                    if (tm_res != TM_OK) {
                        status = PSA_ERROR_GENERIC_ERROR;
                        g_model_loaded = 0;
                    } else {
                        g_model_loaded = 1;
                        status = PSA_SUCCESS;
                    }
                } else {
                    /* Read custom model data */
                    bytes_read = psa_read(msg.handle, 0, input_buffer, msg.in_size[0]);
                    if (bytes_read != msg.in_size[0]) {
                        status = PSA_ERROR_COMMUNICATION_FAILURE;
                    } else {
                        tm_res = tm_load(&g_mdl, input_buffer, static_main_buf, layer_cb, &g_in);
                        if (tm_res != TM_OK) {
                            status = PSA_ERROR_GENERIC_ERROR;
                            g_model_loaded = 0;
                        } else {
                            g_model_loaded = 1;
                            status = PSA_SUCCESS;
                        }
                    }
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
                if (!g_model_loaded) {
                    status = PSA_ERROR_BAD_STATE;
                } else {
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
                        /* Use built-in test image */
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
                    } else {
                        /* Invalid input size */
                        status = PSA_ERROR_INVALID_ARGUMENT;
                    }
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