/*
 * Copyright (c) 2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "psa/service.h"
#include "psa_manifest/echo_service_manifest.h"
#include <string.h>

/* Maximum echo data size */
#define TFM_ECHO_MAX_DATA_SIZE 256

/* Initialization function for the echo service */
psa_status_t echo_service_init(void)
{
    /* Nothing to initialize for this simple service */
    return PSA_SUCCESS;
}

/* Main entry point for the partition - required for IPC model */
void echo_service_entry(void)
{
    psa_msg_t msg;
    psa_status_t status;
    uint8_t input_buffer[TFM_ECHO_MAX_DATA_SIZE];
    uint8_t output_buffer[TFM_ECHO_MAX_DATA_SIZE];
    size_t bytes_read;

    /* Service loop: continuously wait for and process messages */
    while (1) {
        /* Wait for a message from a client */
        psa_wait(TFM_ECHO_SERVICE_SIGNAL, PSA_BLOCK);
        
        /* Get the message */
        if (psa_get(TFM_ECHO_SERVICE_SIGNAL, &msg) != PSA_SUCCESS) {
            continue;
        }
        
        /* Process the message */
        switch (msg.type) {
            case PSA_IPC_CONNECT:
                /* Connection request */
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            case PSA_IPC_CALL:
                /* Process the request */
                /* Validate input parameters */
                if (msg.in_size[0] > TFM_ECHO_MAX_DATA_SIZE) {
                    status = PSA_ERROR_INVALID_ARGUMENT;
                } else if (msg.out_size[0] < msg.in_size[0]) {
                    status = PSA_ERROR_BUFFER_TOO_SMALL;
                } else {
                    /* Read input data */
                    bytes_read = psa_read(msg.handle, 0, input_buffer, msg.in_size[0]);
                    if (bytes_read != msg.in_size[0]) {
                        status = PSA_ERROR_COMMUNICATION_FAILURE;
                    } else {
                        /* Echo the data (simple copy) */
                        memcpy(output_buffer, input_buffer, bytes_read);
                        
                        /* Write output data back to client */
                        psa_write(msg.handle, 0, output_buffer, bytes_read);
                        status = PSA_SUCCESS;
                    }
                }
                
                psa_reply(msg.handle, status);
                break;
                
            case PSA_IPC_DISCONNECT:
                /* Client disconnected */
                psa_reply(msg.handle, PSA_SUCCESS);
                break;
                
            default:
                /* Unsupported message type */
                psa_reply(msg.handle, PSA_ERROR_NOT_SUPPORTED);
                break;
        }
    }
}
