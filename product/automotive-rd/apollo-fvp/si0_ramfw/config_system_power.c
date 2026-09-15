/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'system_power'.
 */

#include "platform_core.h"
#include "si0_cfgd_timer.h"
#include "si0_mmap.h"

#include <mod_power_domain.h>
#include <mod_ppu_v1.h>
#include <mod_si0_platform.h>
#include <mod_system_power.h>

#include <fwk_element.h>
#include <fwk_id.h>
#include <fwk_interrupt.h>
#include <fwk_macros.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_log.h>
#include <fwk_status.h>

#include <stdint.h>
#include <inttypes.h>

#ifndef APOLLO_FVP_SUSPEND_KEEP_SYSTOP_ON
#define APOLLO_FVP_SUSPEND_KEEP_SYSTOP_ON 0
#endif

#if APOLLO_FVP_TEST_SUSPEND_WAKE_US > 0 && APOLLO_FVP_SUSPEND_KEEP_SYSTOP_ON
static int ap_children_off_check(void)
{
    for (unsigned int cluster = 0; cluster < platform_get_cluster_count(); cluster++) {
        uintptr_t base = SI0_ATW1_CLUSTER_UTILITY_BASE +
            cluster * SI0_CLUSTER_UTILITY_SIZE;
        uintptr_t ppu = base + SI0_CLUSTER_UTILITY_CLUSTER_PPU_OFFSET;
        uint32_t pwsr = *(const volatile uint32_t *)(ppu + 8);

        /* PPU PWSR[3:0] is the actual operating mode; OFF is zero. */
        if ((pwsr & 0xf) != 0) {
            FWK_LOG_ERR("[SYS-POW TEST] cluster%u not OFF: PWSR=%08x", cluster, pwsr);
            return FWK_E_STATE;
        }
        for (unsigned int core = 0;
             core < platform_get_core_per_cluster_count(cluster); core++) {
            ppu = base + SI0_CLUSTER_UTILITY_CORE_PPU0_OFFSET +
                core * SI0_CLUSTER_UTILITY_CORE_PPU_OFFSET;
            pwsr = *(const volatile uint32_t *)(ppu + 8);
            if ((pwsr & 0xf) != 0) {
                FWK_LOG_ERR("[SYS-POW TEST] cluster%u core%u not OFF: PWSR=%08x",
                            cluster, core, pwsr);
                return FWK_E_STATE;
            }
        }
    }
    return FWK_SUCCESS;
}
#endif

#if APOLLO_FVP_TEST_SUSPEND_WAKE_US > 0 && APOLLO_FVP_AP_SRAM_RETAINED && \
    !APOLLO_FVP_SUSPEND_KEEP_SYSTOP_ON
/* Diagnostic FNV-1a only: not a cryptographic integrity check or restoration.
 * Both regions are bounded by the platform map (one MiB each). The saved
 * fingerprints live in SCP's always-on private SRAM, never in AP memory.
 */
static uint64_t ap_sram_fingerprint(uintptr_t base, size_t size)
{
    const volatile uint8_t *bytes = (const volatile uint8_t *)base;
    uint64_t hash = UINT64_C(14695981039346656037);

    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int ap_sram_retention_check(bool before_power_off)
{
    static const uintptr_t bases[] = {
        SI0_ATW6_AP_PERIPHERAL_SRAM_BASE,
        SI0_ATW7_AP_PERIPHERAL_NS_SRAM_BASE,
    };
    static const size_t sizes[] = {
        SI0_ATW6_AP_PERIPHERAL_SRAM_SIZE,
        SI0_ATW7_AP_PERIPHERAL_NS_SRAM_SIZE,
    };
    static uint64_t saved[2];
    static bool valid;
    bool matched = true;

    if (!before_power_off && !valid) {
        FWK_LOG_ERR("[AP SRAM TEST] Missing pre-OFF fingerprint");
        return FWK_E_STATE;
    }
    valid = false;
    for (unsigned int i = 0; i < FWK_ARRAY_SIZE(bases); i++) {
        uint64_t current = ap_sram_fingerprint(bases[i], sizes[i]);

        FWK_LOG_INFO("[AP SRAM TEST] %s base=0x%" PRIxPTR
                     " size=0x%zx fnv64=%016" PRIx64,
                     before_power_off ? "before-OFF" : "after-ON",
                     bases[i], sizes[i], current);
        if (before_power_off)
            saved[i] = current;
        else if (saved[i] != current) {
            FWK_LOG_ERR("[AP SRAM TEST] mismatch region=%u expected=%016" PRIx64,
                        i, saved[i]);
            matched = false;
        }
    }
    if (before_power_off) {
        valid = true;
        return FWK_SUCCESS;
    }
    if (!matched)
        return FWK_E_DATA;
    FWK_LOG_INFO("[AP SRAM TEST] retained 2 MiB fingerprint match");
    return FWK_SUCCESS;
}
#endif

/* Indices for system power module elements */
enum cfgd_mod_system_power_element_idx {
    CFGD_MOD_SYSTEM_POWER_EIDX_SYS_PPU,
    CFGD_MOD_SYSTEM_POWER_EIDX_COUNT
};

static const uint8_t sys_pwr_state_table[MOD_PD_STATE_COUNT_MAX] = {
    [MOD_PD_STATE_OFF] = MOD_PD_STATE_OFF,
    [MOD_PD_STATE_ON] = MOD_PD_STATE_ON,
#if APOLLO_FVP_TEST_SUSPEND_WAKE_US > 0
#if APOLLO_FVP_SUSPEND_KEEP_SYSTOP_ON
    [MOD_SYSTEM_POWER_POWER_STATE_SLEEP0] = MOD_PD_STATE_ON,
#else
    [MOD_SYSTEM_POWER_POWER_STATE_SLEEP0] = MOD_PD_STATE_OFF,
#endif
#endif
};

static struct fwk_element element_table[CFGD_MOD_SYSTEM_POWER_EIDX_COUNT + 1] = {
    [CFGD_MOD_SYSTEM_POWER_EIDX_SYS_PPU] = {
        .name = "SYS-PPU-0",
        .data = &((struct mod_system_power_dev_config) {
            .api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_PPU_V1,
                MOD_PPU_V1_API_IDX_POWER_DOMAIN_DRIVER),
            .sys_state_table = sys_pwr_state_table,
        }),
    },
    [CFGD_MOD_SYSTEM_POWER_EIDX_COUNT] = { 0 }, /* Termination description */
};

static const struct fwk_element *system_power_get_element_table(fwk_id_t unused)
{
    unsigned int ppu_idx_base;
    unsigned int core_count;
    unsigned int cluster_count;

    core_count = platform_get_core_count();
    cluster_count = platform_get_cluster_count();

    /* The system PPUs are placed after the core and cluster PPUs */
    ppu_idx_base = core_count + cluster_count;

    /* Configure System PPU id */
    for (unsigned int i = 0; i < (FWK_ARRAY_SIZE(element_table) - 1); i++) {
        struct mod_system_power_dev_config *dev_config =
            (struct mod_system_power_dev_config *)element_table[i].data;

        dev_config->sys_ppu_id =
            fwk_id_build_element_id(fwk_module_id_ppu_v1, ppu_idx_base + i);
    }

    return element_table;
}

static struct mod_system_power_config system_power_config = {
    .soc_wakeup_irq = FWK_INTERRUPT_NONE,
#if APOLLO_FVP_TEST_SUSPEND_WAKE_US > 0
    .test_wake_alarm_id = FWK_ID_SUB_ELEMENT_INIT(
        FWK_MODULE_IDX_TIMER, SI0_SI0_TIMER_ALARM_ELEMENT_IDX,
        SI0_CFGD_TEST_AP_SUSPEND_WAKE_ALARM_IDX),
    .test_wake_delay_us = APOLLO_FVP_TEST_SUSPEND_WAKE_US,
#if APOLLO_FVP_SUSPEND_KEEP_SYSTOP_ON
    .test_keep_systop_on = true,
    .test_children_off_check = ap_children_off_check,
#elif APOLLO_FVP_AP_SRAM_RETAINED
    .test_retention_check = ap_sram_retention_check,
#endif
#endif
    .driver_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
    .driver_api_id = FWK_ID_API_INIT(
        FWK_MODULE_IDX_SI0_PLATFORM,
        MOD_SI0_PLATFORM_API_IDX_SYSTEM_POWER_DRIVER),
    .initial_system_power_state = MOD_PD_STATE_ON,
};

const struct fwk_module_config config_system_power = {
    .data = &system_power_config,
    .elements = FWK_MODULE_DYNAMIC_ELEMENTS(system_power_get_element_table),
};
