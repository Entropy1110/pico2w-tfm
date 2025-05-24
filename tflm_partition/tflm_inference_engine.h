/*
 * Copyright (c) 2024, Your Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TFLM_INFERENCE_ENGINE_H__
#define __TFLM_INFERENCE_ENGINE_H__

#include <stdint.h>
#include <stddef.h>
#include "psa/error.h"
#include "psa_tflm_service_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize a model and get its information */
psa_status_t tflm_init_model(const uint8_t *model_data,
                              size_t model_size,
                              tflm_model_info_t *model_info);

/* Run inference on the model */
psa_status_t tflm_run_inference(const uint8_t *model_data,
                                const uint8_t *input_data,
                                size_t input_size,
                                uint8_t *output_data,
                                size_t output_buffer_size,
                                size_t *actual_output_size);

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
