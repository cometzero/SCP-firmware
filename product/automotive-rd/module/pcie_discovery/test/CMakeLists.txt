#
# Arm SCP/MCP Software
# Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

set(TEST_SRC mod_pcie_discovery)
set(TEST_FILE mod_pcie_discovery)
set(UNIT_TEST_TARGET mod_pcie_discovery_unit_test)

set(MODULE_SRC ${PRODUCT_ROOT}/automotive-rd/module/pcie_discovery/src)
set(MODULE_INC ${PRODUCT_ROOT}/automotive-rd/module/pcie_discovery/include)
set(MODULE_UT_SRC ${CMAKE_CURRENT_LIST_DIR})
set(MODULE_UT_INC ${CMAKE_CURRENT_LIST_DIR})

list(APPEND MOCK_REPLACEMENTS fwk_module)

include(${SCP_ROOT}/unit_test/module_common.cmake)

target_compile_options(
    ${UNIT_TEST_TARGET} PRIVATE -Wno-error=int-to-pointer-cast)
