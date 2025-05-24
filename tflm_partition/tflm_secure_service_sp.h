/*
 * Copyright (c) 2024, Your Organization
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TFLM_SECURE_SERVICE_SP_H__
#define __TFLM_SECURE_SERVICE_SP_H__

#include <stdint.h>
#include "psa_manifest/pid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Main entry point for the TFLM secure service partition */
void tflm_secure_service_sp_main(void);

#ifdef __cplusplus
}
#endif

#endif /* __TFLM_SECURE_SERVICE_SP_H__ */
