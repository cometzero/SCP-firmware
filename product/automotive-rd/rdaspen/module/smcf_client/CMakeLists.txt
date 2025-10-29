#
# Arm SCP/MCP Software
# Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

add_library(${SCP_MODULE_TARGET} SCP_MODULE)

target_include_directories(${SCP_MODULE_TARGET}
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/interface/amu/"
    "${CMAKE_SOURCE_DIR}/module/sensor_smcf_drv/include/"
    "${CMAKE_SOURCE_DIR}/module/power_domain/include/")

target_sources(${SCP_MODULE_TARGET}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/mod_smcf_client.c")

target_link_libraries(
    ${SCP_MODULE_TARGET} PRIVATE module-platform-smcf
                                 module-smcf
                                 module-amu-smcf-drv
                                 module-sensor)
