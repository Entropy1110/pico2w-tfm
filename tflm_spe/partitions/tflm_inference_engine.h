/*
 * Copyright (c) 2024, Your Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TFLM_INFERENCE_ENGINE_H__
#define __TFLM_INFERENCE_ENGINE_H__

#include <stdint.h>
#include <stddef.h>
#include "psa/error.h"
#include "psa_tflm_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize a model and get its information */
psa_status_t tflm_init_model(const uint8_t *model_data,
                              size_t model_size,
                              uint32_t *model_id);

/* Get model information */
psa_status_t tflm_get_model_info(uint32_t model_id,
                                 uint32_t *input_size,
                                 uint32_t *output_size,
                                 uint32_t *model_version);

/* Run inference on the model */
psa_status_t tflm_run_inference(uint32_t model_id,
                                const uint8_t *input_data,
                                size_t input_size,
                                uint8_t *output_data,
                                size_t output_size,
                                size_t *actual_output_size);

/* Cleanup model resources */
void tflm_cleanup_model(void);

// usage
//status = tflm_decrypt_model(encrypted_model_data, model_size, 
//                                 &slot->model_data, &slot->model_size);
psa_status_t tflm_decrypt_model(const uint8_t *encrypted_model_data,
                                size_t encrypted_model_size,
                                uint8_t **decrypted_model_data,
                                size_t *decrypted_model_size);

#ifdef __cplusplus
}
#endif

#endif /* __TFLM_INFERENCE_ENGINE_H__ */
