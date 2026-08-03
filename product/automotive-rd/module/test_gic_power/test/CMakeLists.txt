#
# Arm SCP/MCP Software
# Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

set(TEST_SRC mod_test_gic_power)
set(TEST_FILE test_gic_power)

set(UNIT_TEST_TARGET mod_test_gic_power_unit_test)

set(MODULE_SRC ${PRODUCT_ROOT}/automotive-rd/module/test_gic_power/src)
set(MODULE_INC ${PRODUCT_ROOT}/automotive-rd/module/test_gic_power/include)
set(DRIVER_SRC
    ${PRODUCT_ROOT}/automotive-rd/module/gicx00_multiview/src/mod_gicx00_multiview.c)

set(MODULE_UT_SRC ${CMAKE_CURRENT_LIST_DIR})
set(MODULE_UT_INC ${CMAKE_CURRENT_LIST_DIR})

list(
    APPEND
    OTHER_MODULE_INC
    ${PRODUCT_ROOT}/automotive-rd/module/gicx00_multiview/include
    ${MODULE_ROOT}/integration_test/include
    ${MODULE_ROOT}/power_domain/include)
list(APPEND MOCK_REPLACEMENTS fwk_core fwk_module)

include(${SCP_ROOT}/unit_test/module_common.cmake)

target_compile_definitions(
    ${UNIT_TEST_TARGET}
    PRIVATE "DRIVER_UNIT_TEST_SRC=\"${DRIVER_SRC}\"")
