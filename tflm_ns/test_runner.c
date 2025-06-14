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
    
#ifdef DEV_MODE
    LOG_MSG("Starting TF-M Test Suite (DEV_MODE)...\r\n");
    LOG_MSG("DEV_MODE: Only HUK-derived model key test will run\r\n");
    
    /* DEV_MODE: Only run HUK key derivation test */
    test_tinymaix_comprehensive_suite(); // This will only run get_model_key test in DEV_MODE
    
    LOG_MSG("DEV_MODE tests completed!\r\n");
#else
    LOG_MSG("Starting TF-M Test Suite (Production Mode)...\r\n");
    
    /* Production Mode: Run all tests except DEV_MODE specific ones */
    /* Test 1: Echo Service */
    test_echo_service();
    
    /* Test 2: PSA Encryption */
    test_psa_encryption();
    
    /* Test 3: PSA Hash */
    test_psa_hash();
    
    /* Test 4: TinyMaix Comprehensive Suite */
    test_tinymaix_comprehensive_suite();
    
    LOG_MSG("All production tests completed!\r\n");
#endif
}
