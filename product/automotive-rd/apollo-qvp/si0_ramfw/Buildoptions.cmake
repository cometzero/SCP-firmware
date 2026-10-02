#
# Arm SCP/MCP Software
# Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

cmake_dependent_option(
    SCP_ENABLE_SCMI_PFDI_MONITOR "Enable SCMI PFDI Monitor"
    "${SCP_ENABLE_SCMI_PFDI_MONITOR_INIT}"
    "DEFINED SCP_ENABLE_SCMI_PFDI_MONITOR_INIT"
    "${SCP_ENABLE_SCMI_PFDI_MONITOR}")

cmake_dependent_option(
    SCP_ENABLE_SCMI_PERF_FAST_CHANNELS "Enable the SCMI-perf Fast channels?"
    "${SCP_ENABLE_SCMI_PERF_FAST_CHANNELS_INIT}"
    "DEFINED SCP_ENABLE_SCMI_PERF_FAST_CHANNELS_INIT"
    "${SCP_ENABLE_SCMI_PERF_FAST_CHANNELS}")

# This is a dependency, not an independent platform option. Reset a stale ON
# cache entry when switching an existing build to the mailbox transport.
set(BUILD_HAS_MOD_TRANSPORT_FC "${SCP_ENABLE_SCMI_PERF_FAST_CHANNELS}"
    CACHE BOOL "Enable SCMI-perf fast-channel transport support" FORCE)
