#
# Arm SCP/MCP Software
# Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

set(TEST_SRC mod_pcie_setup)
set(TEST_FILE mod_pcie_setup)
set(UNIT_TEST_TARGET mod_pcie_setup_unit_test)

set(MODULE_SRC ${PRODUCT_ROOT}/automotive-rd/module/pcie_setup/src)
set(MODULE_INC ${PRODUCT_ROOT}/automotive-rd/module/pcie_setup/include)
set(MODULE_UT_SRC ${CMAKE_CURRENT_LIST_DIR})
set(MODULE_UT_INC ${CMAKE_CURRENT_LIST_DIR})

list(APPEND OTHER_MODULE_INC
     ${PRODUCT_ROOT}/automotive-rd/module/pcie_discovery/include)
list(APPEND OTHER_MODULE_INC
     ${PRODUCT_ROOT}/automotive-rd/interface/io_block)
list(APPEND OTHER_MODULE_INC ${SCP_ROOT}/interface/address_remapper)
list(APPEND OTHER_MODULE_INC ${SCP_ROOT}/interface/cmn)
list(APPEND OTHER_MODULE_INC ${MODULE_ROOT}/sds/include)
list(APPEND OTHER_MODULE_INC ${MODULE_ROOT}/system_info/include)

list(APPEND MOCK_REPLACEMENTS fwk_mm)
list(APPEND MOCK_REPLACEMENTS fwk_module)
list(APPEND MOCK_REPLACEMENTS fwk_notification)

include(${SCP_ROOT}/unit_test/module_common.cmake)

target_compile_definitions(${UNIT_TEST_TARGET} PUBLIC "BUILD_HAS_NOTIFICATION")
