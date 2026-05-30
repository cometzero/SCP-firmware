/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'ppu_v1'.
 */

#include "platform_core.h"
#include "si0_cfgd_power_domain.h"
#include "si0_mmap.h"
#include "si_scr_info.h"

#include <mod_power_domain.h>
#include <mod_ppu_v1.h>

#include <fwk_element.h>
#include <fwk_id.h>
#include <fwk_interrupt.h>
#include <fwk_macros.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_string.h>

#include <stdio.h>

#define PPU_STATIC_ELEMENT_COUNT 1
#define PPU_CORE_NAME_SIZE       12
#define PPU_CLUS_NAME_SIZE       7

#define SI1_PPU_AE_BASE      (SI1_CLUSTER_UTILITY_BUS_BASE + 0x80000)
#define SI1_CORE_BASE_OFFSET (0x40000)
#define SI1_CORE_STRIDE      (0x100000)

/* Module configuration data */
static struct mod_ppu_v1_config ppu_v1_config_data = {
    .pd_notification_id = FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN,
        MOD_PD_NOTIFICATION_IDX_POWER_STATE_TRANSITION),
    .pd_source_id = FWK_ID_MODULE(FWK_MODULE_IDX_POWER_DOMAIN),
};

/* List of static PPU elements */
static struct fwk_element ppu_element_table[] = {
    {
        .name = "SYS0",
        .data = &((struct mod_ppu_v1_pd_config){
            .pd_type = MOD_PD_TYPE_SYSTEM,
            .ppu.reg_base = SI0_PPU_SYS0_BASE,
            .default_power_on = true,
            .observer_id = FWK_ID_NONE_INIT,
        }),
    },
};

/* Cluster PPU base address */
static inline uintptr_t cluster_utility_cluster_ppu_base(
    unsigned int cluster_idx)
{
    fwk_assert(cluster_idx < platform_get_cluster_count());

    return (
        SI0_ATW1_CLUSTER_UTILITY_BASE +
        (cluster_idx * SI0_CLUSTER_UTILITY_SIZE) +
        SI0_CLUSTER_UTILITY_CLUSTER_PPU_OFFSET);
}

/* Application core PPU base address */
static inline uintptr_t cluster_utility_core_ppu_base(
    unsigned int cluster_idx,
    unsigned int core_idx)
{
    fwk_assert(cluster_idx < platform_get_cluster_count());
    fwk_assert(core_idx < platform_get_core_per_cluster_count(cluster_idx));

    return (
        SI0_ATW1_CLUSTER_UTILITY_BASE +
        (cluster_idx * SI0_CLUSTER_UTILITY_SIZE) +
        SI0_CLUSTER_UTILITY_CORE_PPU0_OFFSET +
        (core_idx * SI0_CLUSTER_UTILITY_CORE_PPU_OFFSET));
}

/* Cluster AE register base address */
static inline uintptr_t cluster_utility_ae_base(unsigned int cluster_idx)
{
    fwk_assert(cluster_idx < platform_get_cluster_count());

    return (
        SI0_ATW1_CLUSTER_UTILITY_BASE +
        (cluster_idx * SI0_CLUSTER_UTILITY_SIZE) +
        SI0_CLUSTER_UTILITY_CLUSTER_AE_OFFSET);
}

/* SI Cluster 1 core PPU base address */
static inline uintptr_t si_cluster_ppu_base(unsigned int core_index)
{
    return SI1_CLUSTER_UTILITY_BUS_BASE + SI1_CORE_BASE_OFFSET +
        (core_index * SI1_CORE_STRIDE);
}

/* Setup SI Cluster 1 elements */
static int setup_si_cluster1_elements(
    struct fwk_element *element_table,
    unsigned int base_index,
    unsigned int element_capacity)
{
    int snprintf_ret;
    struct fwk_element *element;
    struct mod_ppu_v1_pd_config *pd_config;
    struct mod_ppu_v1_pd_config *pd_config_table_si;

    if (element_table == NULL)
        return FWK_E_PARAM;

    if (base_index > element_capacity)
        return FWK_E_RANGE;
    if (SI1_TOTAL_ELEMENTS > (element_capacity - base_index))
        return FWK_E_RANGE;

    pd_config_table_si =
        fwk_mm_calloc(SI1_TOTAL_ELEMENTS, sizeof(struct mod_ppu_v1_pd_config));

    for (unsigned int i = 0; i < SI1_CORE_COUNT; ++i) {
        pd_config = &pd_config_table_si[i];
        element = &element_table[base_index + i];

        pd_config->pd_type = MOD_PD_TYPE_CORE;
        pd_config->ppu.reg_base = si_cluster_ppu_base(i);
        pd_config->cluster_ae_reg_base = SI1_PPU_AE_BASE;
        pd_config->ppu.irq = FWK_INTERRUPT_NONE;
        pd_config->cluster_id =
            FWK_ID_ELEMENT(FWK_MODULE_IDX_PPU_V1, base_index + SI1_CORE_COUNT);
        pd_config->observer_id = FWK_ID_NONE;

        element->name = fwk_mm_alloc(PPU_CORE_NAME_SIZE, 1);

        snprintf_ret = snprintf(
            (char *)element->name, PPU_CORE_NAME_SIZE, "CLUS1CORE%u", i);
        fwk_assert((snprintf_ret >= 0) && (snprintf_ret <= PPU_CORE_NAME_SIZE));
        element->data = pd_config;
    }

    /* Configure SI Cluster 1 */
    pd_config = &pd_config_table_si[SI1_CORE_COUNT];
    element = &element_table[base_index + SI1_CORE_COUNT];

    pd_config->pd_type = MOD_PD_TYPE_CLUSTER;
    pd_config->ppu.reg_base = SI1_CLUSTER_UTILITY_BUS_BASE + 0x10000;
    pd_config->cluster_ae_reg_base = SI1_PPU_AE_BASE;
    pd_config->ppu.irq = FWK_INTERRUPT_NONE;
    pd_config->observer_id = FWK_ID_NONE;
    pd_config->observer_api = FWK_ID_NONE;
    pd_config->opmode = PPU_V1_OPMODE_07;

    element->name = "CLUS1";
    element->data = pd_config;

    return FWK_SUCCESS;
}

static const struct fwk_element *ppu_v1_get_element_table(fwk_id_t module_id)
{
    struct fwk_element *element_table;
    struct mod_ppu_v1_pd_config *pd_config_table;
    unsigned int cluster_idx;
    unsigned int core_count;
    unsigned int cluster_count;
    unsigned int core_element_count = 0;
    unsigned int number_elements;
    unsigned int extended_elements = 0;
    int snprintf_ret_val;
    unsigned int static_table_count = FWK_ARRAY_SIZE(ppu_element_table);

    core_count = platform_get_core_count();
    cluster_count = platform_get_cluster_count();
    if (si_cl1_present()) {
        extended_elements = SI1_TOTAL_ELEMENTS;
    }

    /*
     * Allocate element descriptors based on:
     *   Number of cores
     *   + Number of cluster descriptors
     *   + Number of system power domain descriptors
     *   + Number of SI power domain descriptors (only for MID variant):
     *      cluster1: core0-3, cluster1 - ( 5 elements)
     *   + 1 terminator descriptor
     */
    number_elements =
        core_count + cluster_count + static_table_count + extended_elements + 1;

    element_table = fwk_mm_calloc(number_elements, sizeof(struct fwk_element));

    pd_config_table = fwk_mm_calloc(
        core_count + cluster_count, sizeof(struct mod_ppu_v1_pd_config));

    for (cluster_idx = 0; cluster_idx < cluster_count; cluster_idx++) {
        struct fwk_element *element;
        struct mod_ppu_v1_pd_config *pd_config;
        unsigned int core_idx;

        for (core_idx = 0;
             core_idx < platform_get_core_per_cluster_count(cluster_idx);
             core_idx++) {
            element = &element_table[core_element_count];
            pd_config = &pd_config_table[core_element_count];

            element->name = fwk_mm_alloc(PPU_CORE_NAME_SIZE, 1);

            snprintf_ret_val = snprintf(
                (char *)element->name,
                PPU_CORE_NAME_SIZE,
                "CLUS%uCORE%u",
                cluster_idx,
                core_idx);

            fwk_assert(
                (snprintf_ret_val >= 0) &&
                (snprintf_ret_val <= PPU_CORE_NAME_SIZE));

            element->data = pd_config;

            pd_config->pd_type = MOD_PD_TYPE_CORE;
            pd_config->ppu.reg_base =
                cluster_utility_core_ppu_base(cluster_idx, core_idx);
            pd_config->ppu.irq = FWK_INTERRUPT_NONE;
            pd_config->cluster_id = FWK_ID_ELEMENT(
                FWK_MODULE_IDX_PPU_V1, (core_count + cluster_idx));
            pd_config->observer_id = FWK_ID_NONE;
            pd_config->cluster_ae_reg_base =
                cluster_utility_ae_base(cluster_idx);
            core_element_count++;
        }

        element = &element_table[core_count + cluster_idx];
        pd_config = &pd_config_table[core_count + cluster_idx];

        element->name = fwk_mm_alloc(PPU_CLUS_NAME_SIZE, 1);

        snprintf_ret_val = snprintf(
            (char *)element->name, PPU_CLUS_NAME_SIZE, "CLUS%u", cluster_idx);

        fwk_assert(
            (snprintf_ret_val >= 0) &&
            (snprintf_ret_val <= PPU_CLUS_NAME_SIZE));

        element->data = pd_config;

        pd_config->pd_type = MOD_PD_TYPE_CLUSTER;
        pd_config->ppu.irq = FWK_INTERRUPT_NONE;
        pd_config->observer_id = FWK_ID_NONE;
        pd_config->observer_api = FWK_ID_NONE;
        pd_config->ppu.reg_base = cluster_utility_cluster_ppu_base(cluster_idx);
        pd_config->cluster_ae_reg_base = cluster_utility_ae_base(cluster_idx);
        /* L3 cache operating policy is ALL_SLICE_FULL_RAM_ON */
        pd_config->opmode = PPU_V1_OPMODE_07;
    }

    fwk_str_memcpy(
        &element_table[core_count + cluster_count],
        ppu_element_table,
        sizeof(ppu_element_table));

    if (si_cl1_present()) {
        /* Insert SI CLUS1 elements after static table */
        unsigned int si_base_index =
            core_count + cluster_count + static_table_count;
        /* usable capacity excludes the terminator slot */
        unsigned int usable_capacity = number_elements - 1;

        if (setup_si_cluster1_elements(
                element_table, si_base_index, usable_capacity) != FWK_SUCCESS) {
            return NULL;
        }
    }

    /*
     * Configure pd_source_id with the SYSTOP identifier from the power domain
     * module which is dynamically defined based on the number of cores.
     */
    ppu_v1_config_data.pd_source_id = fwk_id_build_element_id(
        fwk_module_id_power_domain,
        core_count + cluster_count + PD_STATIC_DEV_IDX_SYSTOP);

    return element_table;
}

const struct fwk_module_config config_ppu_v1 = {
    .data = &ppu_v1_config_data,
    .elements = FWK_MODULE_DYNAMIC_ELEMENTS(ppu_v1_get_element_table),
};
