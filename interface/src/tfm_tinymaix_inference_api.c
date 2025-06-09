#include "psa/client.h"
#include "tfm_tinymaix_inference_defs.h"

tfm_tinymaix_status_t tfm_tinymaix_load_model(const uint8_t* model_data, size_t model_size)
{
    psa_status_t status;
    psa_handle_t handle;
    uint32_t result = 1;
    
    /* Connect to service */
    handle = psa_connect(TFM_TINYMAIX_INFERENCE_SID, 1);
    if (handle <= 0) {
        return TINYMAIX_STATUS_ERROR_GENERIC;
    }
    
    psa_outvec out_vec[] = {
        {.base = &result, .len = sizeof(result)}
    };
    
    if (model_data == NULL || model_size == 0) {
        /* Use built-in model - no input data */
        status = psa_call(handle, TINYMAIX_IPC_LOAD_MODEL, NULL, 0, out_vec, 1);
    } else {
        /* Use custom model data */
        psa_invec in_vec[] = {
            {.base = model_data, .len = model_size}
        };
        status = psa_call(handle, TINYMAIX_IPC_LOAD_MODEL, in_vec, 1, out_vec, 1);
    }
    
    psa_close(handle);
    
    if (status != PSA_SUCCESS || result != 0) {
        return TINYMAIX_STATUS_ERROR_MODEL_LOAD_FAILED;
    }
    
    return TINYMAIX_STATUS_SUCCESS;
}

tfm_tinymaix_status_t tfm_tinymaix_run_inference(int* predicted_class)
{
    psa_status_t status;
    psa_handle_t handle;
    int result = -1;
    
    if (!predicted_class) {
        return TINYMAIX_STATUS_ERROR_INVALID_PARAM;
    }
    
    /* Connect to service */
    handle = psa_connect(TFM_TINYMAIX_INFERENCE_SID, 1);
    if (handle <= 0) {
        return TINYMAIX_STATUS_ERROR_GENERIC;
    }
    
    psa_outvec out_vec[] = {
        {.base = &result, .len = sizeof(result)}
    };
    
    status = psa_call(handle, TINYMAIX_IPC_RUN_INFERENCE, NULL, 0, out_vec, 1);
    
    psa_close(handle);
    
    if (status != PSA_SUCCESS || result < 0) {
        return TINYMAIX_STATUS_ERROR_INFERENCE_FAILED;
    }
    
    *predicted_class = result;
    return TINYMAIX_STATUS_SUCCESS;
}