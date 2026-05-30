/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'power_domain'.
 */

#include "platform_core.h"
#include "si0_cfgd_power_domain.h"
#include "si_scr_info.h"

#include <power_domain_utils.h>

#include <mod_power_domain.h>
#include <mod_ppu_v1.h>
#include <mod_system_power.h>

#include <fwk_element.h>
#include <fwk_id.h>
#include <fwk_macros.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_string.h>

#define PD_STATIC_ELEMENT_COUNT (PD_STATIC_DEV_IDX_SYSTOP + 1)

/* Mask for the cluster valid power states */
#define CLUSTER_VALID_STATE_MASK (MOD_PD_STATE_OFF_MASK | MOD_PD_STATE_ON_MASK)

/* Mask for the core valid power states */
#define CORE_VALID_STATE_MASK (MOD_PD_STATE_OFF_MASK | MOD_PD_STATE_ON_MASK)

/* Mask of the allowed states for the systop power domain */
static const uint32_t systop_allowed_state_mask_table[] = {
    [0] = MOD_PD_STATE_ON_MASK
};

/*
 * Mask of the allowed states for the cluster power domain depending on the
 * system states.
 */
static const uint32_t cluster_pd_allowed_state_mask_table[] = {
    [MOD_PD_STATE_OFF] = MOD_PD_STATE_OFF_MASK,
    [MOD_PD_STATE_ON] = CLUSTER_VALID_STATE_MASK,
};

/* Mask of the allowed states for a core depending on the cluster states. */
static const uint32_t core_pd_allowed_state_mask_table[] = {
    [MOD_PD_STATE_OFF] = MOD_PD_STATE_OFF_MASK | MOD_PD_STATE_SLEEP_MASK,
    [MOD_PD_STATE_ON] = CORE_VALID_STATE_MASK,
};

/* Power module specific configuration data (none) */
static const struct mod_power_domain_config platform_power_domain_config = {
    0
};

/* Create SI Cluster1 power domain elements */
static int create_si_cluster_elements(
    struct fwk_element *si_dst,
    unsigned int dst_base_idx,
    unsigned int dst_capacity)
{
    struct fwk_element dummy_static_table = { 0 };
    const struct fwk_element *si_src = NULL;
    struct mod_power_domain_element_config *pd_config;

    if (si_dst == NULL)
        return FWK_E_PARAM;

    if (SI1_TOTAL_ELEMENTS > dst_capacity)
        return FWK_E_RANGE;

    si_src = create_power_domain_element_table(
        SI1_CORE_COUNT,
        SI1_CLUSTER_COUNT,
        FWK_MODULE_IDX_PPU_V1,
        MOD_PPU_V1_API_IDX_POWER_DOMAIN_DRIVER,
        core_pd_allowed_state_mask_table,
        FWK_ARRAY_SIZE(core_pd_allowed_state_mask_table),
        cluster_pd_allowed_state_mask_table,
        FWK_ARRAY_SIZE(cluster_pd_allowed_state_mask_table),
        &dummy_static_table,
        0);

    if (si_src == NULL)
        return FWK_E_NOMEM;

    fwk_str_memcpy(
        si_dst, si_src, SI1_TOTAL_ELEMENTS * sizeof(struct fwk_element));

    for (unsigned int i = 0; i < SI1_TOTAL_ELEMENTS; ++i) {
        pd_config = (struct mod_power_domain_element_config *)si_dst[i].data;
        if (i < SI1_CORE_COUNT) {
            pd_config->parent_idx = dst_base_idx + SI1_CORE_COUNT;
        } else {
            pd_config->parent_idx = PD_STATIC_DEV_IDX_NONE;
        }

        pd_config->driver_id =
            FWK_ID_ELEMENT(FWK_MODULE_IDX_PPU_V1, dst_base_idx + i);
    }

    return FWK_SUCCESS;
}

static const struct fwk_element *platform_power_domain_get_element_table(
    fwk_id_t module_id)
{
    struct mod_power_domain_element_config *systop_pd_config;
    const struct fwk_element *systop_elements = NULL;
    const struct fwk_element *final_elements = NULL;
    struct fwk_element *all_elements = NULL;
    struct fwk_element *si_dst = NULL;
    unsigned int systop_count;
    unsigned int total_count;
    unsigned int si_capacity;
    int st;

    systop_pd_config =
        fwk_mm_calloc(1, sizeof(struct mod_power_domain_element_config));
    systop_pd_config->attributes.pd_type = MOD_PD_TYPE_SYSTEM;
    systop_pd_config->parent_idx = PD_STATIC_DEV_IDX_NONE;
    systop_pd_config->driver_id = FWK_ID_MODULE(FWK_MODULE_IDX_SYSTEM_POWER);
    systop_pd_config->api_id = FWK_ID_API(
        FWK_MODULE_IDX_SYSTEM_POWER, MOD_SYSTEM_POWER_API_IDX_PD_DRIVER);
    systop_pd_config->allowed_state_mask_table =
        systop_allowed_state_mask_table;
    systop_pd_config->allowed_state_mask_table_size =
        FWK_ARRAY_SIZE(systop_allowed_state_mask_table);

    struct fwk_element pd_static_element_table[] = {
        [PD_STATIC_DEV_IDX_SYSTOP] = {
            .name = "SYSTOP",
            .data = systop_pd_config,
        },
    };

    /* Create power doamin elements for SYSTOP */
    systop_elements = create_power_domain_element_table(
        platform_get_core_count(),
        platform_get_cluster_count(),
        FWK_MODULE_IDX_PPU_V1,
        MOD_PPU_V1_API_IDX_POWER_DOMAIN_DRIVER,
        core_pd_allowed_state_mask_table,
        FWK_ARRAY_SIZE(core_pd_allowed_state_mask_table),
        cluster_pd_allowed_state_mask_table,
        FWK_ARRAY_SIZE(cluster_pd_allowed_state_mask_table),
        pd_static_element_table,
        FWK_ARRAY_SIZE(pd_static_element_table));

    if (systop_elements == NULL) {
        return NULL;
    }
    final_elements = systop_elements;

    if (si_cl1_present()) {
        /* Create SI_Cluster1 elements */
        systop_count = platform_get_core_count() +
            platform_get_cluster_count() +
            FWK_ARRAY_SIZE(pd_static_element_table);
        total_count = systop_count + SI1_TOTAL_ELEMENTS;

        /* +1 for the final terminator */
        all_elements =
            fwk_mm_calloc(total_count + 1, sizeof(struct fwk_element));

        fwk_str_memcpy(
            all_elements,
            systop_elements,
            systop_count * sizeof(struct fwk_element));

        /* Fill SI block */
        si_dst = all_elements + systop_count;
        si_capacity = total_count - systop_count;

        st = create_si_cluster_elements(si_dst, systop_count, si_capacity);
        if (st != FWK_SUCCESS)
            return NULL;

        final_elements = all_elements;
    }

    return final_elements;
}

const struct fwk_module_config config_power_domain = {
    .data = &platform_power_domain_config,
    .elements =
        FWK_MODULE_DYNAMIC_ELEMENTS(platform_power_domain_get_element_table),
};
