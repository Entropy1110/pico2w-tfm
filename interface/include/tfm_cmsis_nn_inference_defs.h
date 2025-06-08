#ifndef TFM_CMSIS_NN_INFERENCE_DEFS_H
#define TFM_CMSIS_NN_INFERENCE_DEFS_H

#include <stdint.h>
#include <stddef.h>
#include "psa/client.h"

#ifdef __cplusplus
extern "C" {
#endif

// CMSIS-NN Inference Service SID
#define TFM_CMSIS_NN_INFERENCE_SERVICE_SID                         (0x00000106U)
#define TFM_CMSIS_NN_INFERENCE_SERVICE_VERSION                     (1U)
// CMSIS-NN Inference Service Function IDs
#define TFM_CMSIS_NN_LOAD_MODEL               (1)
#define TFM_CMSIS_NN_RUN_INFERENCE            (2)
#define TFM_CMSIS_NN_GET_INPUT_SIZE           (3)
#define TFM_CMSIS_NN_GET_OUTPUT_SIZE          (4)
#define TFM_CMSIS_NN_SET_INPUT_DATA           (5)
#define TFM_CMSIS_NN_GET_OUTPUT_DATA          (6)

// Return codes
#define TFM_CMSIS_NN_SUCCESS                  (0)
#define TFM_CMSIS_NN_ERROR_INVALID_PARAMETER  (-1)
#define TFM_CMSIS_NN_ERROR_MODEL_NOT_LOADED   (-2)
#define TFM_CMSIS_NN_ERROR_INFERENCE_FAILED   (-3)
#define TFM_CMSIS_NN_ERROR_MEMORY_ALLOCATION  (-4)
#define TFM_CMSIS_NN_ERROR_MODEL_LOADING_FAILED (-5)

// Maximum sizes
#define TFM_CMSIS_NN_MAX_MODEL_SIZE           (32768)  // 32KB max model size
#define TFM_CMSIS_NN_MAX_INPUT_SIZE           (2048)   // 2KB max input size
#define TFM_CMSIS_NN_MAX_OUTPUT_SIZE          (1024)   // 1KB max output size

// Service entry points
void cmsis_nn_inference_entry(void);
psa_status_t cmsis_nn_inference_init(void);

// Client API functions
psa_status_t tfm_cmsis_nn_load_model(const uint8_t* model_data, size_t model_size);
psa_status_t tfm_cmsis_nn_set_input_data(const uint8_t* input_data, size_t input_size);
psa_status_t tfm_cmsis_nn_run_inference(void);
psa_status_t tfm_cmsis_nn_get_output_data(uint8_t* output_data, size_t output_size, size_t* actual_size);
psa_status_t tfm_cmsis_nn_get_input_size(size_t* input_size);
psa_status_t tfm_cmsis_nn_get_output_size(size_t* output_size);
void tfm_cmsis_nn_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* TFM_CMSIS_NN_INFERENCE_DEFS_H */