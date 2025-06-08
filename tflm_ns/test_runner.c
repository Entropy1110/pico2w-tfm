/*
 * Copyright (c) 2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "app.h"
#include "tfm_log.h"

/**
 * \brief Run all TF-M tests
 */
void run_all_tests(void* arg)
{
    UNUSED_VARIABLE(arg);
    LOG_MSG("Starting TF-M Test Suite...\r\n");
    
    /* Test 1: Echo Service */
    test_echo_service();
    
    /* Test 2: PSA Encryption */
    test_psa_encryption();
    
    /* Test 3: PSA Hash */
    test_psa_hash();
    
    /* Test 4: CMSIS-NN Inference */
    cmsis_nn_inference_test();

    
    LOG_MSG("All tests completed!\r\n");
}
