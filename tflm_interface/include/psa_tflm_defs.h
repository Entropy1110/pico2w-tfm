/*
 * Copyright (c) 2024, TZTZEN Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PSA_TFLM_DEFS_H__
#define __PSA_TFLM_DEFS_H__

#include <stdint.h>
#include <stddef.h>

#ifndef TFM_PARTITION_TFLM_SECURE_SERVICE
#include "psa/client.h"
#else
#include "psa/service.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* TFLM Secure Service ID */
#define TFLM_SECURE_SERVICE_SID     (0x00000100U)
#define TFLM_SECURE_SERVICE_SIGNAL  (1U << (TFLM_SECURE_SERVICE_SID & 0x1FU))
/* TFLM Secure Service Version */
#define TFLM_SECURE_SERVICE_VERSION (1U)

/* PSA Service IDs for TFLM Operations */
#define PSA_TFLM_LOAD_MODEL_SID     (0x00000101U)
#define PSA_TFLM_RUN_INFERENCE_SID  (0x00000102U)
#define PSA_TFLM_GET_MODEL_INFO_SID (0x00000103U)
#define PSA_TFLM_UNLOAD_MODEL_SID   (0x00000104U)

/* PSA Service Versions */
#define PSA_TFLM_LOAD_MODEL_VERSION     (1U)
#define PSA_TFLM_RUN_INFERENCE_VERSION  (1U)
#define PSA_TFLM_GET_MODEL_INFO_VERSION (1U)
#define PSA_TFLM_UNLOAD_MODEL_VERSION   (1U)

/* Request Types for TFLM Operations */
#define TFLM_REQUEST_TYPE_LOAD_MODEL        0x01
#define TFLM_REQUEST_TYPE_RUN_INFERENCE     0x02
#define TFLM_REQUEST_TYPE_GET_MODEL_INFO    0x03
#define TFLM_REQUEST_TYPE_UNLOAD_MODEL      0x04
#define TFLM_REQUEST_TYPE_ECHO              0x05

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

/* PSA Request Type Definitions */
typedef enum {
    TFLM_PSA_REQUEST_INFER = 1,
    TFLM_PSA_REQUEST_ECHO  = 2
} psa_tflm_service_req_type_t;

#ifdef __cplusplus
}
#endif

#endif /* __PSA_TFLM_DEFS_H__ */
