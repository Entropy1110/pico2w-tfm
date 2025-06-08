#-------------------------------------------------------------------------------
# Copyright (c) 2025, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.21)

# This is important to add this SPE subdirectory at first as it
# brings configuration from TF-M build
add_subdirectory(${CONFIG_SPE_PATH} ${CMAKE_BINARY_DIR}/spe)

# Ensure LOG functionality is always available regardless of test configuration
set(TFM_NS_LOG_DISABLE OFF CACHE BOOL "LOG functionality must remain enabled" FORCE)

####################### NS application libraries #######################

# Choose whether to use the TF-M mailbox API or TrustZone API to communicate
# with the SPE based on the TF-M config. If the TF-M config provides both, then
# default to the TrustZone APIs.
if (TFM_PARTITION_NS_AGENT_MAILBOX AND NOT CONFIG_TFM_USE_TRUSTZONE)
    set(TFM_NS_MAILBOX_API ON  CACHE BOOL "Build NS code for a remote processing element using mailbox APIs")
else()
    set(TFM_NS_MAILBOX_API OFF CACHE BOOL "Build NS code for a remote processing element using mailbox APIs")
endif()

# Interface files exported from TF-M secure build
set(SPE_INSTALL_INTERFACE_SRC    ${CONFIG_SPE_PATH}/interface/src)
set(SPE_INSTALL_INTERFACE_INC    ${CONFIG_SPE_PATH}/interface/include)
# lib path
set(APP_LIB_DIR                  ${CMAKE_CURRENT_LIST_DIR}/../lib)

# OS wrapper library
add_library(os_wrapper STATIC)

target_sources(os_wrapper
    PRIVATE
        os_wrapper_cmsis_rtos_v2.c
        $<$<BOOL:${TFM_NS_MANAGE_NSID}>:tfm_nsid_map_table.c>
)

target_include_directories(os_wrapper
    PUBLIC
        ${APP_LIB_DIR}/os_wrapper
        ${APP_LIB_DIR}
        ${SPE_INSTALL_INTERFACE_INC}
        .
)

target_link_libraries(os_wrapper
    PRIVATE
        RTX_OS
        tfm_nsid_manager
)

# NSID manager
add_library(tfm_nsid_manager INTERFACE)

target_include_directories(tfm_nsid_manager
    INTERFACE
        ${APP_LIB_DIR}/nsid_manager
)

target_sources(tfm_nsid_manager
    INTERFACE
        $<$<BOOL:${TFM_NS_MANAGE_NSID}>:${APP_LIB_DIR}/nsid_manager/tfm_nsid_manager.c>
)

target_compile_definitions(tfm_nsid_manager
    INTERFACE
        $<$<BOOL:${TFM_NS_MANAGE_NSID}>:TFM_NS_MANAGE_NSID>
        $<$<BOOL:${TEST_NS_MANAGE_NSID}>:TEST_NS_MANAGE_NSID>
)

target_sources(RTX_OS
    INTERFACE
        $<$<NOT:$<BOOL:${TFM_NS_MAILBOX_API}>>:${APP_LIB_DIR}/nsid_manager/tz_shim_layer.c>
        os_config_cmsis_rtx.c
)

target_link_libraries(RTX_OS
    INTERFACE
        tfm_nsid_manager
)

# Multi-core library
if(TFM_NS_MAILBOX_API)
    add_library(ns_multi_core STATIC)

    target_sources(ns_multi_core
        PRIVATE
            $<$<NOT:$<BOOL:${TFM_MULTI_CORE_NS_OS_MAILBOX_THREAD}>>:${SPE_INSTALL_INTERFACE_SRC}/multi_core/tfm_ns_mailbox.c>
            $<$<BOOL:${TFM_MULTI_CORE_NS_OS_MAILBOX_THREAD}>:${SPE_INSTALL_INTERFACE_SRC}/multi_core/tfm_ns_mailbox_thread.c>
            ${APP_LIB_DIR}/multi_core/tfm_ns_mailbox_rtos_api.c
            $<$<BOOL:${TEST_NS_MULTI_CORE}>:${APP_LIB_DIR}/multi_core/tfm_ns_mailbox_test.c>
    )

    target_compile_definitions(ns_multi_core
        PUBLIC
            $<$<BOOL:${TFM_MULTI_CORE_NS_OS_MAILBOX_THREAD}>:TFM_MULTI_CORE_NS_OS_MAILBOX_THREAD>
            TFM_MULTI_CORE_NS_OS
    )

    target_link_libraries(ns_multi_core
        PRIVATE
            platform_ns
            tfm_api_ns
            os_wrapper
    )
endif()

# TF-M NS logging
add_library(tfm_ns_log STATIC)

target_sources(tfm_ns_log
    PRIVATE
        ${APP_LIB_DIR}/log/tfm_log_raw.c
)

target_include_directories(tfm_ns_log
    PUBLIC
        ${APP_LIB_DIR}/log/
)

target_compile_definitions(tfm_ns_log
    INTERFACE
        $<$<NOT:$<BOOL:${TFM_NS_LOG_DISABLE}>>:TFM_NS_LOG>
)

target_link_libraries(tfm_ns_log
    PRIVATE
        platform_ns
)

################## Update plaform_ns with NS settings #################

target_include_directories(platform_ns
    PUBLIC
        $<$<BOOL:${TFM_NS_MAILBOX_API}>:${SPE_INSTALL_INTERFACE_INC}/multi_core>
        ${SPE_INSTALL_INTERFACE_INC}
        ${CMAKE_CURRENT_LIST_DIR}/../interface/include
)

target_compile_definitions(platform_ns
    PUBLIC
        $<$<BOOL:${TFM_NS_MAILBOX_API}>:TFM_MULTI_CORE_NS_OS>
)

################# Update NS interface with NS settings ################

# Add echo service API to tfm_api_ns
target_sources(tfm_api_ns
    PRIVATE
        # NS specific implementation of NS interface dispatcher
        $<$<NOT:$<BOOL:${TFM_NS_MAILBOX_API}>>:${SPE_INSTALL_INTERFACE_SRC}/os_wrapper/tfm_ns_interface_rtos.c>
        # Echo service API is now included directly in tfm_tflm_broker
)

target_compile_definitions(tfm_api_ns
    PUBLIC
        $<$<BOOL:${TEST_NS_MULTI_CORE}>:TFM_MULTI_CORE_TEST>
)

target_link_libraries(tfm_api_ns
    PRIVATE
        $<$<BOOL:${TFM_NS_MAILBOX_API}>:tfm_api_ns_mailbox>
        $<$<NOT:$<BOOL:${TFM_NS_MAILBOX_API}>>:tfm_api_ns_tz>
        $<$<NOT:$<BOOL:${TFM_NS_MAILBOX_API}>>:os_wrapper>
)

######################## NS application broker ########################

add_library(tfm_tflm_broker STATIC)

target_sources(tfm_tflm_broker
    PRIVATE
        main_ns.c
        ../tflm_ns/echo_test_app.c
        ../tflm_ns/psa_crypto_test.c
        ../tflm_ns/test_runner.c
        ../tflm_ns/cmsis_nn_inference_test.c
        ../models/simple_xor_model.c
        ../interface/src/tfm_echo_api.c
        ../interface/src/tfm_cmsis_nn_inference_api.c
)

target_include_directories(tfm_tflm_broker
    PUBLIC
        .
        ../tflm_ns
        ../interface/include
        ../models
)

target_link_libraries(tfm_tflm_broker
    PRIVATE
        os_wrapper
        tfm_nsid_manager
    PUBLIC
        RTX_OS
        tfm_api_ns
        tfm_ns_log
        $<$<BOOL:${TFM_NS_MAILBOX_API}>:ns_multi_core>
)

target_compile_definitions(tfm_tflm_broker
    PUBLIC
        TFM_NS_LOG
        $<$<BOOL:${TFM_NS_MAILBOX_API}>:TFM_NS_MAILBOX_API>
)
