/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'pfdi_monitor'.
 */

#include "platform_core.h"
#include "si0_cfgd_pfdi_monitor.h"
#include "si0_cfgd_timer.h"
#include "si0_cfgd_transport.h"
#include "si_scr_info.h"

#include <mod_pfdi_monitor.h>

#include <fwk_id.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

/* Constant values for AP */
#define OOR_PFDI_PERIOD_US 5000000UL /* 5 seconds */
#define BOOT_TIMEOUT_US    50000000UL /* 50 seconds */

/* Constant values for SICL1 */
#define SICL1_OOR_PFDI_PERIOD_US 1000000UL /* 1 second */
#define SICL1_BOOT_TIMEOUT_US    10000000UL /* 10 seconds */

/* Power domain table has a single static element (SYSTOP) for this product. */
#define PD_STATIC_ELEMENT_COUNT (1U)

static struct mod_pfdi_monitor_core_config
    core_cfg_table[SI0_CFGD_MOD_PFDI_MONITOR_EIDX_COUNT];

#define PFDI_MONITOR_AP_CORE(cluster, core) \
    { \
        .name = "AP cluster " #cluster " core " #core, \
        .data = \
            &core_cfg_table \
                [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_##cluster##_CORE_##core], \
    }

#if (PLATFORM_VARIANT == RD_ASPEN_VARIANT_FVP) && (!RD_ASPEN_VARIANT_CFG1)
#    define PFDI_MONITOR_SICL1_CORE(core) \
        { \
            .name = "SI cluster 1 core " #core, \
            .data = \
                &core_cfg_table \
                    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_SI_CLUSTER1_CORE_##core], \
        }
#endif /* RD_ASPEN_VARIANT_CFG1 */

static struct fwk_element element_table[] = {
#if (PLATFORM_VARIANT == RD_ASPEN_VARIANT_FVP) && (!RD_ASPEN_VARIANT_CFG1)
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_SI_CLUSTER1_CORE_0] =
        PFDI_MONITOR_SICL1_CORE(0),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_SI_CLUSTER1_CORE_1] =
        PFDI_MONITOR_SICL1_CORE(1),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_SI_CLUSTER1_CORE_2] =
        PFDI_MONITOR_SICL1_CORE(2),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_SI_CLUSTER1_CORE_3] =
        PFDI_MONITOR_SICL1_CORE(3),
#endif /* RD_ASPEN_VARIANT_CFG1 */
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_0_CORE_0] =
        PFDI_MONITOR_AP_CORE(0, 0),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_0_CORE_1] =
        PFDI_MONITOR_AP_CORE(0, 1),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_0_CORE_2] =
        PFDI_MONITOR_AP_CORE(0, 2),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_0_CORE_3] =
        PFDI_MONITOR_AP_CORE(0, 3),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_1_CORE_0] =
        PFDI_MONITOR_AP_CORE(1, 0),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_1_CORE_1] =
        PFDI_MONITOR_AP_CORE(1, 1),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_1_CORE_2] =
        PFDI_MONITOR_AP_CORE(1, 2),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_1_CORE_3] =
        PFDI_MONITOR_AP_CORE(1, 3),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_2_CORE_0] =
        PFDI_MONITOR_AP_CORE(2, 0),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_2_CORE_1] =
        PFDI_MONITOR_AP_CORE(2, 1),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_2_CORE_2] =
        PFDI_MONITOR_AP_CORE(2, 2),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_2_CORE_3] =
        PFDI_MONITOR_AP_CORE(2, 3),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_3_CORE_0] =
        PFDI_MONITOR_AP_CORE(3, 0),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_3_CORE_1] =
        PFDI_MONITOR_AP_CORE(3, 1),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_3_CORE_2] =
        PFDI_MONITOR_AP_CORE(3, 2),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_3_CORE_3] =
        PFDI_MONITOR_AP_CORE(3, 3),
    [SI0_CFGD_MOD_PFDI_MONITOR_EIDX_COUNT] = { 0 },
};

static const struct fwk_element *get_pfdi_monitor_element_table(fwk_id_t unused)
{
    unsigned int core;
    unsigned int cluster;
    unsigned int ap_core_idx;
    unsigned int element_idx;
#if (PLATFORM_VARIANT == RD_ASPEN_VARIANT_FVP) && (!RD_ASPEN_VARIANT_CFG1)
    unsigned int si_pd_base_idx;

    /*
     * Power domain table ordering is:
     * - AP core elements [0..platform_get_core_count()-1]
     * - AP cluster elements
     * [platform_get_core_count()..+platform_get_cluster_count()-1]
     * - Static elements (SYSTOP) appended last
     * - SI CL1 elements (when present) appended after that
     */
    si_pd_base_idx = platform_get_core_count() + platform_get_cluster_count() +
        PD_STATIC_ELEMENT_COUNT;

    /* Safety Island CL1 cores */
    for (core = 0; core < SI1_CORE_COUNT; core++) {
        core_cfg_table[core] = (struct mod_pfdi_monitor_core_config){
            .alarm_id = FWK_ID_SUB_ELEMENT_INIT(
                FWK_MODULE_IDX_TIMER,
                SI0_SI0_TIMER_ALARM_ELEMENT_IDX,
                (unsigned int)
                        SI0_CFGD_PFDI_MONITOR_ALARM_IDX_SI_CLUSTER1_CORE0 +
                    core),
            .pd_source_id = FWK_ID_ELEMENT(
                FWK_MODULE_IDX_POWER_DOMAIN, si_pd_base_idx + core),
            .oor_pfdi_period_us = SICL1_OOR_PFDI_PERIOD_US,
            .onl_pfdi_period_us = SICL1_PFDI_ONLINE_TIMEOUT_US,
            .boot_timeout_us = SICL1_BOOT_TIMEOUT_US,
        };
    }
#endif /* RD_ASPEN_VARIANT_CFG1 */

    /* AP cores */
    for (cluster = 0; cluster < platform_get_cluster_count(); cluster++) {
        for (core = 0; core < CORES_PER_CLUSTER; core++) {
            ap_core_idx = (cluster * CORES_PER_CLUSTER) + core;
            element_idx = SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_0_CORE_0 +
                ap_core_idx;

            core_cfg_table[element_idx] = (struct mod_pfdi_monitor_core_config){
                .alarm_id = FWK_ID_SUB_ELEMENT_INIT(
                    FWK_MODULE_IDX_TIMER,
                    SI0_SI0_TIMER_ALARM_ELEMENT_IDX,
                    (unsigned int)
                            SI0_CFGD_PFDI_MONITOR_ALARM_IDX_AP_CLUSTER0_CORE0 +
                        ap_core_idx),
                .pd_source_id = FWK_ID_ELEMENT_INIT(
                    FWK_MODULE_IDX_POWER_DOMAIN, ap_core_idx),
                .oor_pfdi_period_us = OOR_PFDI_PERIOD_US,
                .onl_pfdi_period_us = PFDI_ONLINE_TIMEOUT_US,
                .boot_timeout_us = BOOT_TIMEOUT_US,
            };
        }
    }

#if (PLATFORM_VARIANT == RD_ASPEN_VARIANT_FVP) && (!RD_ASPEN_VARIANT_CFG1)
    element_table[PC_CONFIGURED_CORES_COUNT + SI1_CORE_COUNT].name = NULL;
#else
    element_table[PC_CONFIGURED_CORES_COUNT].name = NULL;
#endif /* RD_ASPEN_VARIANT_CFG1 */
    return element_table;
}

const struct fwk_module_config config_pfdi_monitor = {
    .elements = FWK_MODULE_DYNAMIC_ELEMENTS(get_pfdi_monitor_element_table),
};
