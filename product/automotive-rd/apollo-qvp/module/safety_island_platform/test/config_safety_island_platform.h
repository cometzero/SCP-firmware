/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'safety_island_platform'.
 */

#include "config_transport.h"

#include <mod_safety_island_platform.h>

#include <fwk_id.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

/* Safety Island Cluster 1 layout */
#define SI_CL1_ID       1U
#define SI_CL1_CORE_NUM 4U
#define SI_CL1_CORE_OFS 1U

enum safety_island_cluster_idx {
    SI_CL1_IDX,
    SI_CL_COUNT,
};

static const struct fwk_element
    safety_island_platform_element_table[SI_CL_COUNT + 1] = {
    [SI_CL1_IDX] = {
        .name = "Safety Island Cluster 1",
        .data = &((struct safety_island_cluster_config) {
            .cluster_layout = {SI_CL1_ID, SI_CL1_CORE_NUM, SI_CL1_CORE_OFS},
        }),
    },
    [SI_CL_COUNT] = { 0 },
};

static const struct fwk_element *get_safety_island_platform_element_table(
    fwk_id_t module_id)
{
    return safety_island_platform_element_table;
}

const struct fwk_module_config config_safety_island_platform = {
    .elements =
        FWK_MODULE_DYNAMIC_ELEMENTS(get_safety_island_platform_element_table),
};
