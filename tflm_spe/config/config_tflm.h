/*
 * Copyright (c) 2019-2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __CONFIG_TFLM_H__
#define __CONFIG_TFLM_H__

/* Platform partition log level */
#ifndef TFM_PARTITION_LOG_LEVEL
#define TFM_PARTITION_LOG_LEVEL TFM_PARTITION_LOG_LEVEL_INFO
#endif

/* Size of the stack to be allocated for each Thread Mode interrupt*/
#define CONFIG_TFM_CONN_HANDLE_MAX_NUM      8

#endif /* __CONFIG_TFLM_H__ */
