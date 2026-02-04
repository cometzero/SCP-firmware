#
# Arm SCP/MCP Software
# Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

add_library(${SCP_MODULE_TARGET} SCP_MODULE)

target_include_directories(
    ${SCP_MODULE_TARGET}
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_sources(
    ${SCP_MODULE_TARGET}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/mod_ros_clock.c")

target_link_libraries(${SCP_MODULE_TARGET}
                        PRIVATE module-clock module-power-domain)

include(${CMAKE_CURRENT_LIST_DIR}/../../rdaspen_variant.cmake)
rdaspen_apply_platform_variant(${SCP_MODULE_TARGET})
