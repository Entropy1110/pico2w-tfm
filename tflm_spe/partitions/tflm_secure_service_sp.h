/*
 * Copyright (c) 2024, Your Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TFLM_SECURE_SERVICE_SP_H__
#define __TFLM_SECURE_SERVICE_SP_H__

#include <stdint.h>
#include "psa_manifest/pid.h"
#include "psa/service.h"
#include "psa_tflm_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SFN model initialization entry point */
psa_status_t tflm_secure_service_sp_init(void);

/* SFN model service call handler */
psa_status_t tfm_tflm_secure_service_sfn(const psa_msg_t *msg);

/* Legacy IPC main entry point (kept for compatibility) */
void tflm_secure_service_sp_main(void);

#ifdef __cplusplus
}
#endif

#endif /* __TFLM_SECURE_SERVICE_SP_H__ */
