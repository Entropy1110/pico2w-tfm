/*
 * Copyright (c) 2024, Your Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "psa/service.h"
#include "tflm_secure_service_sp.h"
#include "tfm_log_unpriv.h"
#include "tflm_crypto_ops.h"
#include "tflm_inference_engine.h"
#include "encrypted_audio_model.h"

/* Include the generated interface header */
#include "psa_tflm_defs.h"

/* Maximum number of models that can be loaded */
#define MAX_LOADED_MODELS 4

/* Model management structure */
typedef struct {
    uint32_t model_id;
    uint8_t *model_data;
    size_t model_size;
    bool is_loaded;
    tflm_model_info_t info;
} loaded_model_t;

/* Global model storage */
static loaded_model_t loaded_models[MAX_LOADED_MODELS];
static uint32_t next_model_id = 1;

/* Initialize model storage */
static void init_model_storage(void)
{
    memset(loaded_models, 0, sizeof(loaded_models));
}

/* Find a free model slot */
static loaded_model_t* find_free_slot(void)
{
    for (int i = 0; i < MAX_LOADED_MODELS; i++) {
        if (!loaded_models[i].is_loaded) {
            return &loaded_models[i];
        }
    }
    return NULL;
}

/* Find a loaded model by ID */
static loaded_model_t* find_model_by_id(uint32_t model_id)
{
    for (int i = 0; i < MAX_LOADED_MODELS; i++) {
        if (loaded_models[i].is_loaded && loaded_models[i].model_id == model_id) {
            return &loaded_models[i];
        }
    }
    return NULL;
}

/* Handle load model request */
static psa_status_t handle_load_model(const psa_msg_t *msg)
{
    loaded_model_t *slot;
    psa_status_t status;
    uint32_t model_id;

    INFO_UNPRIV_RAW("[TFLM SP] Handling load model request");

    /* Find free slot */
    slot = find_free_slot();
    if (!slot) {
        INFO_UNPRIV_RAW("[TFLM SP] No free model slots available");
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }

    /* Decrypt the embedded encrypted model */
    status = tflm_decrypt_model(encrypted_audio_preprocessor_int8_data, 
                                encrypted_audio_preprocessor_int8_size,
                                &slot->model_data, &slot->model_size);
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM SP] Failed to decrypt model: %d", status);
        return status;
    }

    /* Assign model ID first */
    slot->model_id = next_model_id++;
    model_id = slot->model_id;
    
    /* Initialize model in inference engine */
    uint32_t returned_model_id;
    status = tflm_init_model(slot->model_data, slot->model_size, &returned_model_id);
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM SP] Failed to initialize model: %d", status);
        /* Clean up allocated memory */
        if (slot->model_data) {
            free(slot->model_data);
            slot->model_data = NULL;
        }
        return status;
    }

    /* Get model info using the new function */
    uint32_t input_size, output_size, model_version;
    status = tflm_get_model_info(returned_model_id, &input_size, &output_size, &model_version);
    if (status == PSA_SUCCESS) {
        slot->info.model_id = returned_model_id;
        slot->info.input_size = input_size;
        slot->info.output_size = output_size;
        slot->info.model_version = model_version;
    }

    /* Mark as loaded */
    slot->is_loaded = true;

    /* Write model ID back to client */
    psa_write(msg->handle, 0, &model_id, sizeof(model_id));

    INFO_UNPRIV_RAW("[TFLM SP] Model loaded successfully with ID: %d", model_id);
    return PSA_SUCCESS;
}

/* Handle run inference request */
static psa_status_t handle_run_inference(const psa_msg_t *msg)
{
    tflm_inference_request_t request;
    uint8_t input_data[256];  /* Temporary buffer */
    uint8_t output_data[256]; /* Temporary buffer */
    size_t actual_output_size;
    loaded_model_t *model;
    psa_status_t status;

    INFO_UNPRIV_RAW("[TFLM SP] Handling run inference request");

    /* Read inference request */
    if (psa_read(msg->handle, 0, &request, sizeof(request)) != sizeof(request)) {
        INFO_UNPRIV_RAW("[TFLM SP] Failed to read inference request");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* Find the model */
    model = find_model_by_id(request.model_id);
    if (!model) {
        INFO_UNPRIV_RAW("[TFLM SP] Model not found: %d", request.model_id);
        return PSA_ERROR_DOES_NOT_EXIST;
    }

    /* Read input data */
    if (request.input_size > sizeof(input_data)) {
        INFO_UNPRIV_RAW("[TFLM SP] Input data too large");
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }
    psa_read(msg->handle, 1, input_data, request.input_size);

    /* Run inference */
    status = tflm_run_inference(model->model_id, input_data, request.input_size,
                                output_data, sizeof(output_data), &actual_output_size);
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM SP] Inference failed");
        return status;
    }

    /* Write output data and size */
    psa_write(msg->handle, 0, output_data, actual_output_size);
    psa_write(msg->handle, 1, &actual_output_size, sizeof(actual_output_size));

    INFO_UNPRIV_RAW("[TFLM SP] Inference completed successfully");
    return PSA_SUCCESS;
}

/* Handle get model info request */
static psa_status_t handle_get_model_info(const psa_msg_t *msg)
{
    uint32_t model_id;
    loaded_model_t *model;

    INFO_UNPRIV_RAW("[TFLM SP] Handling get model info request");

    /* Read model ID */
    if (psa_read(msg->handle, 0, &model_id, sizeof(model_id)) != sizeof(model_id)) {
        INFO_UNPRIV_RAW("[TFLM SP] Failed to read model ID");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* Find the model */
    model = find_model_by_id(model_id);
    if (!model) {
        INFO_UNPRIV_RAW("[TFLM SP] Model not found: %d", model_id);
        return PSA_ERROR_DOES_NOT_EXIST;
    }

    /* Write model info back */
    psa_write(msg->handle, 0, &model->info, sizeof(model->info));

    INFO_UNPRIV_RAW("[TFLM SP] Model info retrieved successfully");
    return PSA_SUCCESS;
}

/* Handle unload model request */
static psa_status_t handle_unload_model(const psa_msg_t *msg)
{
    uint32_t model_id;
    loaded_model_t *model;

    INFO_UNPRIV_RAW("[TFLM SP] Handling unload model request");

    /* Read model ID */
    if (psa_read(msg->handle, 0, &model_id, sizeof(model_id)) != sizeof(model_id)) {
        INFO_UNPRIV_RAW("[TFLM SP] Failed to read model ID");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* Find the model */
    model = find_model_by_id(model_id);
    if (!model) {
        INFO_UNPRIV_RAW("[TFLM SP] Model not found: %d", model_id);
        return PSA_ERROR_DOES_NOT_EXIST;
    }

    /* Clean up model */
    if (model->model_data) {
        free(model->model_data);
        model->model_data = NULL;
    }
    
    /* Clean up TFLM resources */
    tflm_cleanup_model();
    
    model->is_loaded = false;
    model->model_size = 0;
    model->model_id = 0;

    INFO_UNPRIV_RAW("[TFLM SP] Model unloaded successfully");
    return PSA_SUCCESS;
}

static psa_status_t handle_echo(const psa_msg_t *msg)
{
    uint8_t buffer[256];
    size_t bytes_read;
    size_t output_size;
    
    INFO_UNPRIV_RAW("[TFLM SP] Handling echo request");
    
    if (msg->in_size[1] > sizeof(buffer)) {
        INFO_UNPRIV_RAW("[TFLM SP] Input data too large: %zu", msg->in_size[1]);
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    bytes_read = psa_read(msg->handle, 1, buffer, msg->in_size[1]);
    if (bytes_read != msg->in_size[1]) {
        INFO_UNPRIV_RAW("[TFLM SP] Failed to read input data");
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    
    INFO_UNPRIV_RAW("[TFLM SP] Echo: received %zu bytes", bytes_read);
    
    output_size = bytes_read;
    psa_write(msg->handle, 0, buffer, output_size);
    psa_write(msg->handle, 1, &output_size, sizeof(output_size));
    
    INFO_UNPRIV_RAW("[TFLM SP] Echo: sent %zu bytes back", output_size);
    return PSA_SUCCESS;
}

/* SFN model initialization entry point */
psa_status_t tflm_secure_service_sp_init(void)
{
    psa_status_t status = PSA_SUCCESS;

    INFO_UNPRIV_RAW("[TFLM SP] TFLM Secure Service Partition initializing");

    /* Initialize model storage */
    init_model_storage();

    /* Initialize crypto operations */
    status = tflm_crypto_init();
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM SP] Failed to initialize crypto operations");
        return status;
    }

    INFO_UNPRIV_RAW("[TFLM SP] TFLM Secure Service Partition initialized successfully");
    return PSA_SUCCESS;
}

/* SFN model service call handler */
psa_status_t tfm_tflm_secure_service_sfn(const psa_msg_t *msg)
{
    switch (msg->type) {
        case TFLM_REQUEST_TYPE_LOAD_MODEL:
            return handle_load_model(msg);
        case TFLM_REQUEST_TYPE_RUN_INFERENCE:
            return handle_run_inference(msg);
        case TFLM_REQUEST_TYPE_GET_MODEL_INFO:
            return handle_get_model_info(msg);
        case TFLM_REQUEST_TYPE_UNLOAD_MODEL:
            return handle_unload_model(msg);
        default:
            INFO_UNPRIV_RAW("[TFLM SP] Unknown request type: %d", msg->type);
            return PSA_ERROR_NOT_SUPPORTED;
    }
}

/* Main entry point for the TFLM secure service partition */
void tflm_secure_service_sp_main(void)
{
    psa_msg_t msg;
    psa_status_t status = PSA_SUCCESS;

    INFO_UNPRIV_RAW("[TFLM SP] TFLM Secure Service Partition started");

    /* Initialize model storage */
    init_model_storage();

    /* Initialize crypto operations */
    status = tflm_crypto_init();
    if (status != PSA_SUCCESS) {
        INFO_UNPRIV_RAW("[TFLM SP] Failed to initialize crypto operations");
        /* Continue anyway for now */
    }

    /* Main message loop */
    while (1) {
        uint32_t signals = psa_wait(TFLM_SECURE_SERVICE_SIGNAL, PSA_BLOCK);

        if (signals & TFLM_SECURE_SERVICE_SIGNAL) {
            status = psa_get(TFLM_SECURE_SERVICE_SIGNAL, &msg);
            if (status != PSA_SUCCESS) {
                continue;
            }

            switch (msg.type) {
            case PSA_IPC_CONNECT:
                INFO_UNPRIV_RAW("[TFLM SP] Connect request received");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;

            case PSA_IPC_CALL:
                {
                    uint32_t request_type;
                    
                    /* Read request type from first input vector */
                    if (psa_read(msg.handle, 0, &request_type, sizeof(request_type)) 
                        != sizeof(request_type)) {
                        INFO_UNPRIV_RAW("[TFLM SP] Failed to read request type");
                        psa_reply(msg.handle, PSA_ERROR_INVALID_ARGUMENT);
                        break;
                    }

                    INFO_UNPRIV_RAW("[TFLM SP] Call request received, type: %d", request_type);

                    /* Handle request based on type */
                    switch (request_type) {
                    case TFLM_REQUEST_TYPE_LOAD_MODEL:
                        status = handle_load_model(&msg);
                        break;
                    case TFLM_REQUEST_TYPE_RUN_INFERENCE:
                        status = handle_run_inference(&msg);
                        break;
                    case TFLM_REQUEST_TYPE_GET_MODEL_INFO:
                        status = handle_get_model_info(&msg);
                        break;
                    case TFLM_REQUEST_TYPE_UNLOAD_MODEL:
                        status = handle_unload_model(&msg);
                        break;
                    case TFLM_REQUEST_TYPE_ECHO:
                        status = handle_echo(&msg);
                        break;
                    default:
                        INFO_UNPRIV_RAW("[TFLM SP] Unknown request type: %d", request_type);
                        status = PSA_ERROR_NOT_SUPPORTED;
                        break;
                    }

                    psa_reply(msg.handle, status);
                }
                break;

            case PSA_IPC_DISCONNECT:
                INFO_UNPRIV_RAW("[TFLM SP] Disconnect request received");
                psa_reply(msg.handle, PSA_SUCCESS);
                break;

            default:
                INFO_UNPRIV_RAW("[TFLM SP] Unknown message type: %d", msg.type);
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
            }
        }
    }
}
