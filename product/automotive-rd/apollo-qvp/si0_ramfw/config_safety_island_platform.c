/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'safety_island_platform'.
 */

#include "platform_core.h"
#include "si_scr_info.h"

#include <mod_safety_island_platform.h>

#include <fwk_id.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

enum safety_island_cluster_idx {
    SI_CL1_IDX,
    SI_CL_COUNT,
};

static struct fwk_element
    safety_island_platform_element_table[SI_CL_COUNT + 1] = {
    [SI_CL1_IDX] = {
        .name = "Safety Island Cluster 1",
        .data = &((struct safety_island_cluster_config) {
            .cluster_layout = {SI_CL1_ID, SI1_CORE_COUNT, SI_CL1_CORE_OFS},
        }),
    },
    [SI_CL_COUNT] = { 0 },
};

static const struct fwk_element *get_safety_island_platform_element_table(
    fwk_id_t module_id)
{
    /* If the Safety Island Cluster 1 is not present, remove it from the element
     * table.
     */
    if (!si_cl1_present()) {
        safety_island_platform_element_table[0].name = NULL;
    }
    return safety_island_platform_element_table;
}

const struct fwk_module_config config_safety_island_platform = {
    .elements =
        FWK_MODULE_DYNAMIC_ELEMENTS(get_safety_island_platform_element_table),
};
