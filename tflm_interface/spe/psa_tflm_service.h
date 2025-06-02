/*
 * Copyright (c) 2024, TZTZEN Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PSA_TFLM_SERVICE_H__
#define __PSA_TFLM_SERVICE_H__

#include "psa/service.h"
#include "psa_tflm_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Service-side function declarations */
static psa_status_t handle_echo(const psa_msg_t *msg);
static psa_status_t handle_load_model(const psa_msg_t *msg);
static psa_status_t handle_run_inference(const psa_msg_t *msg);
static psa_status_t handle_get_model_info(const psa_msg_t *msg);
static psa_status_t handle_unload_model(const psa_msg_t *msg);

/* Service initialization */
psa_status_t tflm_secure_service_sp_init(void);
void tflm_secure_service_sp_main(void);

#ifdef __cplusplus
}
#endif

#endif /* __PSA_TFLM_SERVICE_H__ */
