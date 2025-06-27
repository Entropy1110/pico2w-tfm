/*
 * Copyright (c) 2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __TFM_ECHO_DEFS_H__
#define __TFM_ECHO_DEFS_H__

#include <stdint.h>
#include "psa/client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Echo service definitions
 *
 * This header file contains definitions for the Echo service, including
 * service ID, version, handle, and maximum data size.
 */
#define TFM_ECHO_SERVICE_SID                                       (0x00000105U)
#define TFM_ECHO_SERVICE_VERSION                                   (1U)
#define TFM_ECHO_SERVICE_HANDLE                                    (0x40000101U)
#define TFM_SP_ECHO_SERVICE                                            (0x1bc)

/**
 * \brief Maximum size for echo data
 */
#define TFM_ECHO_MAX_DATA_SIZE  256


/**
 * \brief Echo service that returns the same data sent to it
 *
 * \param[in]  data     Pointer to input data buffer
 * \param[in]  data_len Length of input data
 * \param[out] out_data Pointer to output data buffer
 * \param[in]  out_len  Size of output buffer
 * \param[out] out_size Actual size of returned data
 *
 * \return Returns error code as specified in \ref psa_status_t
 */
psa_status_t tfm_echo_service(const uint8_t* data, 
                              size_t data_len,
                              uint8_t* out_data, 
                              size_t out_len,
                              size_t* out_size);

#ifdef __cplusplus
}
#endif

#endif /* __TFM_ECHO_DEFS_H__ */
