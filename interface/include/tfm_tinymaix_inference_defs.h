#ifndef __TFM_TINYMAIX_INFERENCE_DEFS_H__
#define __TFM_TINYMAIX_INFERENCE_DEFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* TinyMaix Inference Service SID */
#define TFM_TINYMAIX_INFERENCE_SID       (0x00000107U)


#define TINYMAIX_IPC_LOAD_ENCRYPTED_MODEL (0x1002U)
#define TINYMAIX_IPC_RUN_INFERENCE       (0x1003U)

/* TinyMaix status codes */
typedef enum {
    TINYMAIX_STATUS_SUCCESS = 0,
    TINYMAIX_STATUS_ERROR_INVALID_PARAM = -1,
    TINYMAIX_STATUS_ERROR_MODEL_LOAD_FAILED = -3,
    TINYMAIX_STATUS_ERROR_INFERENCE_FAILED = -4,
    TINYMAIX_STATUS_ERROR_GENERIC = -100
} tfm_tinymaix_status_t;

/* TinyMaix API function declarations - encrypted model only */
tfm_tinymaix_status_t tfm_tinymaix_load_encrypted_model();
tfm_tinymaix_status_t tfm_tinymaix_run_inference(int* predicted_class);

/* TODO : Add function to run inference with custom image data */
tfm_tinymaix_status_t tfm_tinymaix_run_inference_with_data(const uint8_t* image_data, size_t image_size, int* predicted_class);

#ifdef __cplusplus
}
#endif

#endif /* __TFM_TINYMAIX_INFERENCE_DEFS_H__ */