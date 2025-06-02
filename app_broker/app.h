/*
 * Copyright (c) 2017-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __TFM_TFLM_APP_H__
#define __TFM_TFLM_APP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Simple macro to mark UNUSED variables
 *
 */
#define UNUSED_VARIABLE(X) ((void)(X))

/**
 * \brief Main TFLM thread entry point
 *
 */
void tflm_main(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __TFM_TFLM_APP_H__ */
