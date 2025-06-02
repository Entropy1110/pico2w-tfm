#-------------------------------------------------------------------------------
# Copyright (c) 2021-2023, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

# tflm application flag parsing - simplified version for non-test applications
# This ensures all test flags are disabled for tflm applications
#
# cmd_line: the output argument to collect the arguments via command line
#
function(parse_tflm_flag cmd_line)

    # Force all regression tests to be disabled for tflm applications
    set(TFM_NS_REG_TEST OFF)
    set(TFM_S_REG_TEST  OFF)
    
    # Enable required partitions for tflm applications (like PSA Crypto tests)
    set(TFM_PARTITION_PROTECTED_STORAGE OFF)
    set(TFM_PARTITION_INTERNAL_TRUSTED_STORAGE ON)
    set(TFM_PARTITION_CRYPTO ON)
    set(TFM_PARTITION_PLATFORM ON)
    set(TFM_PARTITION_TFLM_SECURE_SERVICE_SP ON)

    set(ns_reg_cmd      "-DTFM_NS_REG_TEST:BOOL=${TFM_NS_REG_TEST}")
    set(s_reg_cmd       "-DTFM_S_REG_TEST:BOOL=${TFM_S_REG_TEST}")
    set(ps_cmd          "-DTFM_PARTITION_PROTECTED_STORAGE:BOOL=${TFM_PARTITION_PROTECTED_STORAGE}")
    set(its_cmd         "-DTFM_PARTITION_INTERNAL_TRUSTED_STORAGE:BOOL=${TFM_PARTITION_INTERNAL_TRUSTED_STORAGE}")
    set(crypto_cmd      "-DTFM_PARTITION_CRYPTO:BOOL=${TFM_PARTITION_CRYPTO}")
    set(platform_cmd    "-DTFM_PARTITION_PLATFORM:BOOL=${TFM_PARTITION_PLATFORM}")
    set(tflm_cmd        "-DTFM_PARTITION_TFLM_SECURE_SERVICE_SP:BOOL=${TFM_PARTITION_TFLM_SECURE_SERVICE_SP}")
    
    # Set IPC backend and related configuration flags
    set(CONFIG_TFM_SPM_BACKEND "IPC")
    set(CONFIG_TFM_SPM_BACKEND_IPC ON)
    set(CONFIG_TFM_SPM_BACKEND_SFN OFF)
    set(backend_cmd     "-DCONFIG_TFM_SPM_BACKEND:STRING=${CONFIG_TFM_SPM_BACKEND}")
    set(backend_ipc_cmd "-DCONFIG_TFM_SPM_BACKEND_IPC:BOOL=${CONFIG_TFM_SPM_BACKEND_IPC}")
    set(backend_sfn_cmd "-DCONFIG_TFM_SPM_BACKEND_SFN:BOOL=${CONFIG_TFM_SPM_BACKEND_SFN}")
    
    # IPC backend supports higher isolation levels
    set(TFM_ISOLATION_LEVEL 2)
    set(iso_cmd         "-DTFM_ISOLATION_LEVEL:STRING=${TFM_ISOLATION_LEVEL}")
    
    set(${cmd_line}     "${${cmd_line}};${ns_reg_cmd};${s_reg_cmd};${ps_cmd};${its_cmd};${crypto_cmd};${platform_cmd};${tflm_cmd};${backend_cmd};${backend_ipc_cmd};${backend_sfn_cmd};${iso_cmd}" PARENT_SCOPE)
    set(TFM_S_REG_TEST  ${TFM_S_REG_TEST} PARENT_SCOPE)
    set(TFM_NS_REG_TEST ${TFM_NS_REG_TEST} PARENT_SCOPE)
    set(TFM_PARTITION_PROTECTED_STORAGE ${TFM_PARTITION_PROTECTED_STORAGE} PARENT_SCOPE)
    set(TFM_PARTITION_INTERNAL_TRUSTED_STORAGE ${TFM_PARTITION_INTERNAL_TRUSTED_STORAGE} PARENT_SCOPE)
    set(TFM_PARTITION_CRYPTO ${TFM_PARTITION_CRYPTO} PARENT_SCOPE)
    set(TFM_PARTITION_PLATFORM ${TFM_PARTITION_PLATFORM} PARENT_SCOPE)
    set(TFM_PARTITION_TFLM_SECURE_SERVICE_SP ${TFM_PARTITION_TFLM_SECURE_SERVICE_SP} PARENT_SCOPE)
    set(CONFIG_TFM_SPM_BACKEND ${CONFIG_TFM_SPM_BACKEND} PARENT_SCOPE)
    set(CONFIG_TFM_SPM_BACKEND_IPC ${CONFIG_TFM_SPM_BACKEND_IPC} PARENT_SCOPE)
    set(CONFIG_TFM_SPM_BACKEND_SFN ${CONFIG_TFM_SPM_BACKEND_SFN} PARENT_SCOPE)
    set(TFM_ISOLATION_LEVEL ${TFM_ISOLATION_LEVEL} PARENT_SCOPE)

endfunction()
