#-------------------------------------------------------------------------------
# Copyright (c) 2021-2023, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

# Provision application flag parsing - simplified version for non-test applications
# This ensures all test flags are disabled for provision applications
#
# cmd_line: the output argument to collect the arguments via command line
#
function(parse_provision_flag cmd_line)

    # Force all regression tests to be disabled for provision applications
    set(TFM_NS_REG_TEST OFF)
    set(TFM_S_REG_TEST  OFF)
    
    # Enable required partitions for provision applications (like PSA Crypto tests)
    set(TFM_PARTITION_PROTECTED_STORAGE OFF)
    set(TFM_PARTITION_INTERNAL_TRUSTED_STORAGE ON)
    set(TFM_PARTITION_CRYPTO ON)
    set(TFM_PARTITION_PLATFORM ON)

    set(ns_reg_cmd      "-DTFM_NS_REG_TEST:BOOL=${TFM_NS_REG_TEST}")
    set(s_reg_cmd       "-DTFM_S_REG_TEST:BOOL=${TFM_S_REG_TEST}")
    set(ps_cmd          "-DTFM_PARTITION_PROTECTED_STORAGE:BOOL=${TFM_PARTITION_PROTECTED_STORAGE}")
    set(its_cmd         "-DTFM_PARTITION_INTERNAL_TRUSTED_STORAGE:BOOL=${TFM_PARTITION_INTERNAL_TRUSTED_STORAGE}")
    set(crypto_cmd      "-DTFM_PARTITION_CRYPTO:BOOL=${TFM_PARTITION_CRYPTO}")
    set(platform_cmd    "-DTFM_PARTITION_PLATFORM:BOOL=${TFM_PARTITION_PLATFORM}")
    
    set(${cmd_line}     "${${cmd_line}};${ns_reg_cmd};${s_reg_cmd};${ps_cmd};${its_cmd};${crypto_cmd};${platform_cmd}" PARENT_SCOPE)
    set(TFM_S_REG_TEST  ${TFM_S_REG_TEST} PARENT_SCOPE)
    set(TFM_NS_REG_TEST ${TFM_NS_REG_TEST} PARENT_SCOPE)
    set(TFM_PARTITION_PROTECTED_STORAGE ${TFM_PARTITION_PROTECTED_STORAGE} PARENT_SCOPE)
    set(TFM_PARTITION_INTERNAL_TRUSTED_STORAGE ${TFM_PARTITION_INTERNAL_TRUSTED_STORAGE} PARENT_SCOPE)
    set(TFM_PARTITION_CRYPTO ${TFM_PARTITION_CRYPTO} PARENT_SCOPE)
    set(TFM_PARTITION_PLATFORM ${TFM_PARTITION_PLATFORM} PARENT_SCOPE)

endfunction()
