/*
 * Copyright (c) 2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __APP_H__
#define __APP_H__

/**
 * \brief Simple macro to mark UNUSED variables
 *
 */
#define UNUSED_VARIABLE(X) ((void)(X))


#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Test echo service functionality
 */
void test_echo_service(void);

/**
 * \brief Test PSA encryption functionality
 */
void test_psa_encryption(void);

/**
 * \brief Test PSA hash functionality
 */
void test_psa_hash(void);

/**
 * \brief Test TinyMaix inference functionality (basic test)
 */
void test_tinymaix_inference(void);

/**
 * \brief Test TinyMaix performance
 */
void test_tinymaix_performance(void);

/**
 * \brief Test TinyMaix error handling
 */
void test_tinymaix_error_handling(void);


/**
 * \brief Run comprehensive TinyMaix test suite
 */
void test_tinymaix_comprehensive_suite(void);


/**
 * \brief Run all TF-M tests
 * \param[in] arg Thread argument (unused)
 */
void run_all_tests(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H__ */
