#
# Arm SCP/MCP Software
# Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

set(TEST_SRC mod_smcf_client)
set(TEST_FILE mod_smcf_client)

set(UNIT_TEST_TARGET mod_smcf_client_unit_test)

set(MODULE_SRC ${PRODUCT_ROOT}/${TEST_MODULE}/src)
set(MODULE_INC ${PRODUCT_ROOT}/${TEST_MODULE}/include)
set(MODULE_UT_SRC ${CMAKE_CURRENT_LIST_DIR})
set(MODULE_UT_INC ${CMAKE_CURRENT_LIST_DIR})
set(MODULE_UT_MOCK_SRC ${CMAKE_CURRENT_LIST_DIR}/mocks)

list(APPEND MOCK_REPLACEMENTS fwk_module)
list(APPEND MOCK_REPLACEMENTS fwk_core)
list(APPEND MOCK_REPLACEMENTS fwk_notification)
list(APPEND MOCK_REPLACEMENTS fwk_mm)
list(APPEND MOCK_REPLACEMENTS fwk_id)

list(APPEND OTHER_MODULE_INC ${MODULE_ROOT}/amu_smcf_drv/include)
list(APPEND OTHER_MODULE_INC ${MODULE_ROOT}/../interface/amu)
list(APPEND OTHER_MODULE_INC ${MODULE_ROOT}/sensor_smcf_drv/include)
list(APPEND OTHER_MODULE_INC ${MODULE_ROOT}/smcf/include)
list(APPEND OTHER_MODULE_INC ${PRODUCT_ROOT}/automotive-rd/apollo-fvp/si0_ramfw/include)
list(APPEND OTHER_MODULE_INC ${PRODUCT_ROOT}/automotive-rd/apollo-fvp/module/platform_smcf/include)
list(APPEND OTHER_MODULE_INC ${MODULE_ROOT}/power_domain/include)
list(APPEND OTHER_MODULE_INC ${MODULE_ROOT}/sensor/include)

include(${SCP_ROOT}/unit_test/module_common.cmake)

target_compile_definitions(${UNIT_TEST_TARGET} PUBLIC "BUILD_HAS_NOTIFICATION")
