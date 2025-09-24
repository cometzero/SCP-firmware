/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "si0_cfgd_dvfs.h"
#include "si0_cfgd_psu.h"
#include "si0_cfgd_timer.h"
#include "si0_clock.h"

#include <mod_dvfs.h>
#include <mod_scmi_perf.h>

#include <fwk_element.h>
#include <fwk_macros.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

/*
 * The power cost figures in this file are built using the dynamic power
 * consumption formula (P = CfV^2), where C represents the capacitance of one
 * processing element in the domain (a core or shader core). This power figure
 * is scaled linearly with the number of processing elements in the performance
 * domain to give a rough representation of the overall power draw. The
 * capacitance constants are given in mW/MHz/V^2 and were taken from the Linux
 * device trees, which provide a dynamic-power-coefficient field in uW/MHz/V^2.
 * This conversion of units, from uW/MHz/V^2 to mW/MHz/V^2, is done by dividing
 * by 1000.
 */

/*
 * dynamic-power-coeffient/1000.
 * This value may be assumed just to show the DVFS capabilities on FVP.
 * The actual value may vary on silicon platform.
 */
#define CORTEX_A720AE_DPC 0.495

static struct mod_dvfs_opp operating_points_cpu[4] = {
    {
        .level = 1800 * 1000000UL,
        .frequency = 1800 * FWK_KHZ,
        .voltage = 750,
        .power = (uint32_t)(CORTEX_A720AE_DPC * 1800 * 0.750 * 0.750),
    },
    {
        .level = 2000 * 1000000UL,
        .frequency = 2000 * FWK_KHZ,
        .voltage = 850,
        .power = (uint32_t)(CORTEX_A720AE_DPC * 2000 * 0.850 * 0.850),
    },
    {
        .level = 2500 * 1000000UL,
        .frequency = 2500 * FWK_KHZ,
        .voltage = 950,
        .power = (uint32_t)(CORTEX_A720AE_DPC * 2500 * 0.950 * 0.950),
    },
    { 0 }
};

static const struct mod_dvfs_domain_config cpu_cluster0 = {
    .psu_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PSU, PSU_ELEMENT_IDX_CLUSTER0),
    .clock_id =
        FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_CLOCK, CFGD_MOD_CLOCK_EIDX_CORE),
    .alarm_id = FWK_ID_SUB_ELEMENT_INIT(
        FWK_MODULE_IDX_TIMER,
        1,
        SI0_CFGD_DVFS_ALARM_IDX_CLUSTER0),
    .retry_us = 1000,
    .latency = 1200,
    .sustained_idx = 2,
    .opps = operating_points_cpu,
};

static const struct mod_dvfs_domain_config cpu_cluster1 = {
    .psu_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PSU, PSU_ELEMENT_IDX_CLUSTER1),
    .clock_id =
        FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_CLOCK, CFGD_MOD_CLOCK_EIDX_CORE),
    .alarm_id = FWK_ID_SUB_ELEMENT_INIT(
        FWK_MODULE_IDX_TIMER,
        1,
        SI0_CFGD_DVFS_ALARM_IDX_CLUSTER1),
    .retry_us = 1000,
    .latency = 1200,
    .sustained_idx = 2,
    .opps = operating_points_cpu,
};

static const struct mod_dvfs_domain_config cpu_cluster2 = {
    .psu_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PSU, PSU_ELEMENT_IDX_CLUSTER2),
    .clock_id =
        FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_CLOCK, CFGD_MOD_CLOCK_EIDX_CORE),
    .alarm_id = FWK_ID_SUB_ELEMENT_INIT(
        FWK_MODULE_IDX_TIMER,
        1,
        SI0_CFGD_DVFS_ALARM_IDX_CLUSTER2),
    .retry_us = 1000,
    .latency = 1200,
    .sustained_idx = 2,
    .opps = operating_points_cpu,
};

static const struct mod_dvfs_domain_config cpu_cluster3 = {
    .psu_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PSU, PSU_ELEMENT_IDX_CLUSTER3),
    .clock_id =
        FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_CLOCK, CFGD_MOD_CLOCK_EIDX_CORE),
    .alarm_id = FWK_ID_SUB_ELEMENT_INIT(
        FWK_MODULE_IDX_TIMER,
        1,
        SI0_CFGD_DVFS_ALARM_IDX_CLUSTER3),
    .retry_us = 1000,
    .latency = 1200,
    .sustained_idx = 2,
    .opps = operating_points_cpu,
};

static const struct fwk_element element_table[DVFS_ELEMENT_IDX_COUNT + 1] = {
    [DVFS_ELEMENT_IDX_CLUSTER0] =
    {
        .name = "DVFS_CPU_CLUSTER0",
        .data = &cpu_cluster0,
    },
    [DVFS_ELEMENT_IDX_CLUSTER1] =
    {
        .name = "DVFS_CPU_CLUSTER1",
        .data = &cpu_cluster1,
    },
    [DVFS_ELEMENT_IDX_CLUSTER2] =
    {
        .name = "DVFS_CPU_CLUSTER2",
        .data = &cpu_cluster2,
    },
    [DVFS_ELEMENT_IDX_CLUSTER3] =
    {
        .name = "DVFS_CPU_CLUSTER3",
        .data = &cpu_cluster3,
    },
    { 0 },
};

static const struct fwk_element *dvfs_get_element_table(fwk_id_t module_id)
{
    return element_table;
}

const struct fwk_module_config config_dvfs = {
    .elements = FWK_MODULE_DYNAMIC_ELEMENTS(dvfs_get_element_table),
};
