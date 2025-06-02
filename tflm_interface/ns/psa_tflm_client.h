/*
 * Copyright (c) 2024, TZTZEN Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PSA_TFLM_CLIENT_H__
#define __PSA_TFLM_CLIENT_H__

#include "psa/client.h"
#include "../include/psa_tflm_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Client-side function declarations */
psa_status_t psa_tflm_echo(const uint8_t *input_data, size_t input_size,
                           uint8_t *output_data, size_t output_buffer_size,
                           size_t *actual_output_size);

psa_status_t psa_tflm_load_model(const uint8_t *encrypted_model_data,
                                 size_t model_size,
                                 uint32_t *model_id);

psa_status_t psa_tflm_run_inference(uint32_t model_id,
                                    const uint8_t *input_data,
                                    size_t input_size,
                                    uint8_t *output_data,
                                    size_t output_size,
                                    size_t *actual_output_size);

psa_status_t psa_tflm_get_model_info(uint32_t model_id,
                                     tflm_model_info_t *model_info);

psa_status_t psa_tflm_unload_model(uint32_t model_id);

#ifdef __cplusplus
}
#endif

#endif /* __PSA_TFLM_CLIENT_H__ */
