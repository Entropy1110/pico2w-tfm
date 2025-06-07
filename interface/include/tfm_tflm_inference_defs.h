#ifndef TFM_TFLM_INFERENCE_DEFS_H
#define TFM_TFLM_INFERENCE_DEFS_H

#include <stdint.h>
#include <stddef.h>
#include "psa/client.h"

#ifdef __cplusplus
extern "C" {
#endif

// TFLM Inference Service SID
#define TFM_TFLM_INFERENCE_SERVICE_SID                             (0x00000106U)
#define TFM_TFLM_INFERENCE_SERVICE_VERSION                         (1U)
// TFLM Inference Service Function IDs
#define TFM_TFLM_LOAD_MODEL                   (1)
#define TFM_TFLM_RUN_INFERENCE                (2)
#define TFM_TFLM_GET_INPUT_SIZE               (3)
#define TFM_TFLM_GET_OUTPUT_SIZE              (4)
#define TFM_TFLM_SET_INPUT_DATA               (5)
#define TFM_TFLM_GET_OUTPUT_DATA              (6)

// Return codes
#define TFM_TFLM_SUCCESS                      (0)
#define TFM_TFLM_ERROR_INVALID_PARAMETER      (-1)
#define TFM_TFLM_ERROR_MODEL_NOT_LOADED       (-2)
#define TFM_TFLM_ERROR_INFERENCE_FAILED       (-3)
#define TFM_TFLM_ERROR_MEMORY_ALLOCATION      (-4)
#define TFM_TFLM_ERROR_MODEL_LOADING_FAILED   (-5)

// Maximum sizes
#define TFM_TFLM_MAX_MODEL_SIZE               (32768)  // 32KB max model size
#define TFM_TFLM_MAX_INPUT_SIZE               (2048)   // 2KB max input size
#define TFM_TFLM_MAX_OUTPUT_SIZE              (1024)   // 1KB max output size

// Service entry points
void tflm_inference_entry(void);
psa_status_t tflm_inference_init(void);

#ifdef __cplusplus
}
#endif

#endif /* TFM_TFLM_INFERENCE_DEFS_H */