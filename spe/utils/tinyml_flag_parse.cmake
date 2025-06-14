#-------------------------------------------------------------------------------
# Copyright (c) 2021-2023, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

# TINYML application flag parsing - simplified version for non-test applications
# This ensures all test flags are disabled for TINYML applications
#
# cmd_line: the output argument to collect the arguments via command line
#
function(parse_tinyml_flag cmd_line)

    # Force all regression tests to be disabled for TINYML applications
    set(TFM_NS_REG_TEST OFF)
    set(TFM_S_REG_TEST  OFF)
    
    # Enable required partitions for TINYML applications
    set(TFM_PARTITION_PROTECTED_STORAGE OFF)
    set(TFM_PARTITION_INTERNAL_TRUSTED_STORAGE ON)
    set(TFM_PARTITION_CRYPTO ON)
    set(TFM_PARTITION_PLATFORM ON)
    set(TFM_PARTITION_INITIAL_ATTESTATION ON)
    set(TFM_PARTITION_ECHO_SERVICE ON)

    # Out-of-tree partition configuration
    get_filename_component(PROJECT_ROOT "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)
    set(TFM_EXTRA_MANIFEST_LIST_FILES "${PROJECT_ROOT}/partitions/manifest_list.yaml")
    set(TFM_EXTRA_PARTITION_PATHS "${PROJECT_ROOT}/partitions")

    set(ns_reg_cmd      "-DTFM_NS_REG_TEST:BOOL=${TFM_NS_REG_TEST}")
    set(s_reg_cmd       "-DTFM_S_REG_TEST:BOOL=${TFM_S_REG_TEST}")
    set(ps_cmd          "-DTFM_PARTITION_PROTECTED_STORAGE:BOOL=${TFM_PARTITION_PROTECTED_STORAGE}")
    set(its_cmd         "-DTFM_PARTITION_INTERNAL_TRUSTED_STORAGE:BOOL=${TFM_PARTITION_INTERNAL_TRUSTED_STORAGE}")
    set(crypto_cmd      "-DTFM_PARTITION_CRYPTO:BOOL=${TFM_PARTITION_CRYPTO}")
    set(platform_cmd    "-DTFM_PARTITION_PLATFORM:BOOL=${TFM_PARTITION_PLATFORM}")
    set(attestation_cmd "-DTFM_PARTITION_INITIAL_ATTESTATION:BOOL=${TFM_PARTITION_INITIAL_ATTESTATION}")
    set(echo_cmd        "-DTFM_PARTITION_ECHO_SERVICE:BOOL=${TFM_PARTITION_ECHO_SERVICE}")
    set(extra_manifest_cmd "-DTFM_EXTRA_MANIFEST_LIST_FILES:STRING=${TFM_EXTRA_MANIFEST_LIST_FILES}")
    set(extra_partition_cmd "-DTFM_EXTRA_PARTITION_PATHS:STRING=${TFM_EXTRA_PARTITION_PATHS}")
    
    set(${cmd_line}     "${${cmd_line}};${ns_reg_cmd};${s_reg_cmd};${ps_cmd};${its_cmd};${crypto_cmd};${platform_cmd};${attestation_cmd};${echo_cmd};${extra_manifest_cmd};${extra_partition_cmd}" PARENT_SCOPE)
    set(TFM_S_REG_TEST  ${TFM_S_REG_TEST} PARENT_SCOPE)
    set(TFM_NS_REG_TEST ${TFM_NS_REG_TEST} PARENT_SCOPE)
    set(TFM_PARTITION_PROTECTED_STORAGE ${TFM_PARTITION_PROTECTED_STORAGE} PARENT_SCOPE)
    set(TFM_PARTITION_INTERNAL_TRUSTED_STORAGE ${TFM_PARTITION_INTERNAL_TRUSTED_STORAGE} PARENT_SCOPE)
    set(TFM_PARTITION_CRYPTO ${TFM_PARTITION_CRYPTO} PARENT_SCOPE)
    set(TFM_PARTITION_PLATFORM ${TFM_PARTITION_PLATFORM} PARENT_SCOPE)
    set(TFM_PARTITION_INITIAL_ATTESTATION ${TFM_PARTITION_INITIAL_ATTESTATION} PARENT_SCOPE)
    set(TFM_PARTITION_ECHO_SERVICE ${TFM_PARTITION_ECHO_SERVICE} PARENT_SCOPE)
    set(TFM_EXTRA_MANIFEST_LIST_FILES ${TFM_EXTRA_MANIFEST_LIST_FILES} PARENT_SCOPE)
    set(TFM_EXTRA_PARTITION_PATHS ${TFM_EXTRA_PARTITION_PATHS} PARENT_SCOPE)

endfunction()
