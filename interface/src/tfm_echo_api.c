/*
 * Copyright (c) 2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "tfm_echo_defs.h"
#include "psa/client.h"

/* Use the SID from header file */
// No need to redefine, use from header

psa_status_t tfm_echo_service(const uint8_t* data, 
                              size_t data_len,
                              uint8_t* out_data, 
                              size_t out_len,
                              size_t* out_size)
{
    psa_status_t status;
    psa_invec in_vec[1];
    psa_outvec out_vec[1];

    if (data == NULL || out_data == NULL || out_size == NULL) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    if (data_len > TFM_ECHO_MAX_DATA_SIZE || out_len < data_len) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }


    in_vec[0].base = data;
    in_vec[0].len = data_len;

    out_vec[0].base = out_data;
    out_vec[0].len = out_len;

    status = psa_call(TFM_ECHO_SERVICE_HANDLE, PSA_IPC_CALL, in_vec, 1, out_vec, 1);

    if (status == PSA_SUCCESS) {
        *out_size = out_vec[0].len;
    }

    return status;
}
