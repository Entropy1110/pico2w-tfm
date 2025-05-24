/*
 * Copyright (c) 2024, TZTZEN Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PSA_TFLM_SERVICE_API_H__
#define __PSA_TFLM_SERVICE_API_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TFLM Secure Service ID */
#define TFLM_SECURE_SERVICE_SID     (0x00000100U)
#define TFLM_SECURE_SERVICE_SIGNAL  (1U << (TFLM_SECURE_SERVICE_SID & 0x1FU))
/* TFLM Secure Service Version */
#define TFLM_SECURE_SERVICE_VERSION (1U)

/* Request Types for TFLM Operations */
#define TFLM_REQUEST_TYPE_LOAD_MODEL        0x01
#define TFLM_REQUEST_TYPE_RUN_INFERENCE     0x02
#define TFLM_REQUEST_TYPE_GET_MODEL_INFO    0x03
#define TFLM_REQUEST_TYPE_UNLOAD_MODEL      0x04

/* Model Information Structure */
typedef struct {
    uint32_t model_id;
    uint32_t input_size;
    uint32_t output_size;
    uint32_t model_version;
} tflm_model_info_t;

/* Inference Request Structure */
typedef struct {
    uint32_t model_id;
    size_t input_size;
    size_t output_size;
} tflm_inference_request_t;

#ifdef __cplusplus
}
#endif

#endif /* __PSA_TFLM_SERVICE_API_H__ */
