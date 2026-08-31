#
# Arm SCP/MCP Software
# Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

foreach(module IN ITEMS io_block pcie_discovery pcie_setup)
    set(shared_path "${SCP_ROOT}/product/automotive-rd/module/${module}")
    set(rd1ae_path "${SCP_ROOT}/product/automotive-rd/rd1ae/module/${module}")

    if(NOT EXISTS "${shared_path}/Module.cmake")
        message(FATAL_ERROR "Shared module is missing: ${shared_path}")
    endif()

    if(EXISTS "${rd1ae_path}")
        message(FATAL_ERROR "RD1AE-local module remains: ${rd1ae_path}")
    endif()
endforeach()
