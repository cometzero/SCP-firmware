#
# Arm SCP/MCP Software
# Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

set(TEST_SRC mod_gicx00_multiview)
set(TEST_FILE mod_gicx00_multiview)

set(UNIT_TEST_TARGET mod_gicx00_multiview_unit_test)

set(MODULE_SRC ${PRODUCT_ROOT}/automotive-rd/module/gicx00_multiview/src)
set(MODULE_INC ${PRODUCT_ROOT}/automotive-rd/module/gicx00_multiview/include)

set(MODULE_UT_SRC ${CMAKE_CURRENT_LIST_DIR})
set(MODULE_UT_INC ${CMAKE_CURRENT_LIST_DIR})

list(APPEND MOCK_REPLACEMENTS fwk_module)

include(${SCP_ROOT}/unit_test/module_common.cmake)
