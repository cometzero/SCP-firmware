#
# Arm SCP/MCP Software
# Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

set(SCP_FIRMWARE "rdaspen-si0-bl2")
set(SCP_FIRMWARE_TARGET "rdaspen-si0-bl2")

set(SCP_TOOLCHAIN_INIT "GNU")

set(SCP_GENERATE_FLAT_BINARY_INIT TRUE)

set(SCP_ARCHITECTURE "aarch64")

set(SCP_ENABLE_NOTIFICATIONS_INIT TRUE)

set(SCP_ENABLE_IMAGE_INTEGRITY_VERIFICATION TRUE)

set(SCP_ENABLE_OUTBAND_MSG_SUPPORT TRUE)

set(SCP_ENABLE_AE_EXTENSION TRUE)

set(SCP_ENABLE_SCMI_NOTIFICATIONS TRUE)

set(SCP_ENABLE_SCMI_PFDI_MONITOR_INIT FALSE)

set(SCP_ENABLE_SCMI_PERF_FAST_CHANNELS_INIT FALSE)

set(SCP_ENABLE_SCMI_PERF_FAST_CHANNELS TRUE)

if (NOT DEFINED SCP_PC_CONFIGURED_CORES_COUNT)
    set(SCP_PC_CONFIGURED_CORES_COUNT 4)
endif()

if (NOT DEFINED SCP_PFDI_ONLINE_TIMEOUT_US)
    set(SCP_PFDI_ONLINE_TIMEOUT_US 100000UL)
endif()

list(PREPEND SCP_MODULE_PATHS
     "${CMAKE_CURRENT_LIST_DIR}/../module/si0_platform"
     "${CMAKE_CURRENT_LIST_DIR}/../module/ros_clock"
     "${CMAKE_CURRENT_LIST_DIR}/../module/cluster_control"
     "${CMAKE_CURRENT_LIST_DIR}/../module/ras_handlers"
     "${CMAKE_CURRENT_LIST_DIR}/../../module/fmu"
     "${CMAKE_CURRENT_LIST_DIR}/../../module/test_fmu"
     "${CMAKE_CURRENT_LIST_DIR}/../../module/ssu"
     "${CMAKE_CURRENT_LIST_DIR}/../../module/test_ssu"
     "${CMAKE_CURRENT_LIST_DIR}/../../module/sbistc"
     "${CMAKE_CURRENT_LIST_DIR}/../../module/test_sbistc"
     "${CMAKE_CURRENT_LIST_DIR}/../../module/gicx00_multiview"
     "${CMAKE_CURRENT_LIST_DIR}/../module/pfdi_monitor"
     "${CMAKE_CURRENT_LIST_DIR}/../module/scmi_pfdi_monitor"
     "${CMAKE_CURRENT_LIST_DIR}/../module/safety_island_platform")

list(APPEND SCP_MODULES
    "armv8r-mpu"
    "pl011"
    "ni-710ae"
    "cmn-cyprus"
    "ppu-v1"
    "gicx00-multiview"
    "gicx00"
    "system-pll"
    "ros-clock"
    "clock"
    "gtimer"
    "timer"
    "sid"
    "system-info"
    "pcid"
    "mhu3"
    "transport"
    "system-power"
    "power-domain"
    "fmu"
    "sbistc"
    "apcontext"
    "scmi"
    "sds"
    "scmi-power-domain"
    "scmi-system-power"
    "ssu"
    "cluster-control"
    "si0-platform"
    "ras-handlers"
    "safety-island-platform"
    "dvfs"
    "scmi-perf"
    "mock-psu"
    "psu"
    "fch-polled"
    "smcf"
    "amu-smcf-drv"
)

if(SCP_ENABLE_DEBUGGER)
    list(APPEND SCP_MODULES "debugger-cli"
        "integration-test"
        "test-fmu"
        "test-ssu"
        "test-sbistc")
endif()

if(SCP_ENABLE_SCMI_PFDI_MONITOR)
    list(APPEND SCP_MODULES
        "pfdi-monitor"
        "scmi-pfdi-monitor")
endif()
