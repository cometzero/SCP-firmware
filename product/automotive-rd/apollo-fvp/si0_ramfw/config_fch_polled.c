/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "si0_cfgd_dvfs.h"
#include "si0_cfgd_scmi.h"
#include "si0_cfgd_timer.h"
#include "si0_mmap.h"

#include <mod_fch_polled.h>
#include <mod_scmi_perf.h>

#include <fwk_element.h>
#include <fwk_id.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

#include <stdint.h>

#define FC_LEVEL_SET_ADDR(PERF_IDX) \
    (SI0_AP_SCMI_FAST_CHANNEL_BASE + \
     MOD_SCMI_PERF_FAST_CHANNEL_OFFSET_LEVEL_SET + \
     (MOD_SCMI_PERF_FAST_CHANNEL_OFFSET_TOTAL * PERF_IDX))

#define FC_LIMIT_SET_ADDR(PERF_IDX) \
    (SI0_AP_SCMI_FAST_CHANNEL_BASE + \
     MOD_SCMI_PERF_FAST_CHANNEL_OFFSET_LIMIT_SET + \
     (MOD_SCMI_PERF_FAST_CHANNEL_OFFSET_TOTAL * PERF_IDX))

#define FC_LEVEL_GET_ADDR(PERF_IDX) \
    (SI0_AP_SCMI_FAST_CHANNEL_BASE + \
     MOD_SCMI_PERF_FAST_CHANNEL_OFFSET_LEVEL_GET + \
     (MOD_SCMI_PERF_FAST_CHANNEL_OFFSET_TOTAL * PERF_IDX))

#define FC_LIMIT_GET_ADDR(PERF_IDX) \
    (SI0_AP_SCMI_FAST_CHANNEL_BASE + \
     MOD_SCMI_PERF_FAST_CHANNEL_OFFSET_LIMIT_GET + \
     (MOD_SCMI_PERF_FAST_CHANNEL_OFFSET_TOTAL * PERF_IDX))

#define FC_LEVEL_SET_AP_ADDR(PERF_IDX) \
    (FC_LEVEL_SET_ADDR(PERF_IDX) - SI0_AP_SCMI_PAYLOAD_NS_A2P_BASE + \
     SI0_AP_PERIPHERAL_SRAM_AP_VIEW_ADDR)

#define FC_LIMIT_SET_AP_ADDR(PERF_IDX) \
    (FC_LIMIT_SET_ADDR(PERF_IDX) - SI0_AP_SCMI_PAYLOAD_NS_A2P_BASE + \
     SI0_AP_PERIPHERAL_SRAM_AP_VIEW_ADDR)

#define FC_LEVEL_GET_AP_ADDR(PERF_IDX) \
    (FC_LEVEL_GET_ADDR(PERF_IDX) - SI0_AP_SCMI_PAYLOAD_NS_A2P_BASE + \
     SI0_AP_PERIPHERAL_SRAM_AP_VIEW_ADDR)

#define FC_LIMIT_GET_AP_ADDR(PERF_IDX) \
    (FC_LIMIT_GET_ADDR(PERF_IDX) - SI0_AP_SCMI_PAYLOAD_NS_A2P_BASE + \
     SI0_AP_PERIPHERAL_SRAM_AP_VIEW_ADDR)

#define FCH_ADDR_INIT(scp_addr, ap_addr, len) \
    &((struct mod_fch_polled_channel_config){ \
        .fch_addr = { \
            .local_view_address = scp_addr, \
            .target_view_address = ap_addr, \
            .length = len, \
        } })

#define FCH_LEVEL_SET_ENTRY(CL) \
    { \
        .name = "FCH_CLUSTER" #CL "_PERF_LEVEL_SET", \
        .data = FCH_ADDR_INIT( \
            FC_LEVEL_SET_ADDR(DVFS_ELEMENT_IDX_CLUSTER##CL), \
            FC_LEVEL_SET_AP_ADDR(DVFS_ELEMENT_IDX_CLUSTER##CL), \
            FCH_POLLED_LEVEL_SET_LENGTH) \
    }

#define FCH_LIMIT_SET_ENTRY(CL) \
    { \
        .name = "FCH_CLUSTER" #CL "_PERF_LIMIT_SET", \
        .data = FCH_ADDR_INIT( \
            FC_LIMIT_SET_ADDR(DVFS_ELEMENT_IDX_CLUSTER##CL), \
            FC_LIMIT_SET_AP_ADDR(DVFS_ELEMENT_IDX_CLUSTER##CL), \
            FCH_POLLED_LIMIT_SET_LENGTH) \
    }

#define FCH_LEVEL_GET_ENTRY(CL) \
    { \
        .name = "FCH_CLUSTER" #CL "_PERF_LEVEL_GET", \
        .data = FCH_ADDR_INIT( \
            FC_LEVEL_GET_ADDR(DVFS_ELEMENT_IDX_CLUSTER##CL), \
            FC_LEVEL_GET_AP_ADDR(DVFS_ELEMENT_IDX_CLUSTER##CL), \
            FCH_POLLED_LEVEL_GET_LENGTH) \
    }

#define FCH_LIMIT_GET_ENTRY(CL) \
    { \
        .name = "FCH_CLUSTER" #CL "_PERF_LIMIT_GET", \
        .data = FCH_ADDR_INIT( \
            FC_LIMIT_GET_ADDR(DVFS_ELEMENT_IDX_CLUSTER##CL), \
            FC_LIMIT_GET_AP_ADDR(DVFS_ELEMENT_IDX_CLUSTER##CL), \
            FCH_POLLED_LIMIT_GET_LENGTH) \
    }

static struct mod_fch_polled_config module_config = {
    .fch_alarm_id = FWK_ID_SUB_ELEMENT_INIT(
        FWK_MODULE_IDX_TIMER,
        SI0_SI0_TIMER_ALARM_ELEMENT_IDX,
        SI0_CFGD_FAST_CHANNEL_TIMER_IDX),
    .fch_poll_rate = APOLLO_FVP_FCH_RATE,
    .rate_limit = APOLLO_FVP_FCH_RATE,
    .attributes = 0,
};

enum fch_polled_length {
    FCH_POLLED_LEVEL_SET_LENGTH = sizeof(uint32_t),
    FCH_POLLED_LIMIT_SET_LENGTH =
        sizeof(struct mod_scmi_perf_fast_channel_limit),
    FCH_POLLED_LEVEL_GET_LENGTH = sizeof(uint32_t),
    FCH_POLLED_LIMIT_GET_LENGTH =
        sizeof(struct mod_scmi_perf_fast_channel_limit)
};

static const struct fwk_element fch_polled_element_table[] = {
    [APOLLO_FVP_PLAT_FCH_CLUSTER0_PERF_LEVEL_SET] = FCH_LEVEL_SET_ENTRY(0),
    [APOLLO_FVP_PLAT_FCH_CLUSTER0_PERF_LIMIT_SET] = FCH_LIMIT_SET_ENTRY(0),
    [APOLLO_FVP_PLAT_FCH_CLUSTER0_PERF_LEVEL_GET] = FCH_LEVEL_GET_ENTRY(0),
    [APOLLO_FVP_PLAT_FCH_CLUSTER0_PERF_LIMIT_GET] = FCH_LIMIT_GET_ENTRY(0),

    [APOLLO_FVP_PLAT_FCH_CLUSTER1_PERF_LEVEL_SET] = FCH_LEVEL_SET_ENTRY(1),
    [APOLLO_FVP_PLAT_FCH_CLUSTER1_PERF_LIMIT_SET] = FCH_LIMIT_SET_ENTRY(1),
    [APOLLO_FVP_PLAT_FCH_CLUSTER1_PERF_LEVEL_GET] = FCH_LEVEL_GET_ENTRY(1),
    [APOLLO_FVP_PLAT_FCH_CLUSTER1_PERF_LIMIT_GET] = FCH_LIMIT_GET_ENTRY(1),

    [APOLLO_FVP_PLAT_FCH_CLUSTER2_PERF_LEVEL_SET] = FCH_LEVEL_SET_ENTRY(2),
    [APOLLO_FVP_PLAT_FCH_CLUSTER2_PERF_LIMIT_SET] = FCH_LIMIT_SET_ENTRY(2),
    [APOLLO_FVP_PLAT_FCH_CLUSTER2_PERF_LEVEL_GET] = FCH_LEVEL_GET_ENTRY(2),
    [APOLLO_FVP_PLAT_FCH_CLUSTER2_PERF_LIMIT_GET] = FCH_LIMIT_GET_ENTRY(2),

    [APOLLO_FVP_PLAT_FCH_CLUSTER3_PERF_LEVEL_SET] = FCH_LEVEL_SET_ENTRY(3),
    [APOLLO_FVP_PLAT_FCH_CLUSTER3_PERF_LIMIT_SET] = FCH_LIMIT_SET_ENTRY(3),
    [APOLLO_FVP_PLAT_FCH_CLUSTER3_PERF_LEVEL_GET] = FCH_LEVEL_GET_ENTRY(3),
    [APOLLO_FVP_PLAT_FCH_CLUSTER3_PERF_LIMIT_GET] = FCH_LIMIT_GET_ENTRY(3),

    [APOLLO_FVP_PLAT_FCH_COUNT] = { 0 },
};

static const struct fwk_element *fch_polled_get_element_table(
    fwk_id_t module_id)
{
    return fch_polled_element_table;
}

const struct fwk_module_config config_fch_polled = {
    .data = &module_config,
    .elements = FWK_MODULE_DYNAMIC_ELEMENTS(fch_polled_get_element_table),
};
