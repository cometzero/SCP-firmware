/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Safety Island Platform Module.
 */

#include "platform_core.h"

#include <mod_power_domain.h>
#include <mod_safety_island_platform.h>

#include <fwk_assert.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_status.h>

#include <stdbool.h>
#include <stdint.h>

#define MOD_NAME "[SAFETY_ISLAND_PLATFORM]"

/* Module context */
struct safety_island_platform_ctx {
    /* Safety Island Cluster config data */
    struct safety_island_cluster_ctx *safety_island_ctx_table;
    /* Number of Safety Island clusters */
    uint32_t safety_island_cluster_count;
};

static struct safety_island_platform_ctx ctx;

static int init_si_cluster_cores(fwk_id_t safety_island_cluster_id)
{
    unsigned int pd_state;
    uint32_t core;
    struct safety_island_cluster_ctx *safety_island_cluster_ctx;
    uint32_t cluster_offset;
    uint32_t num_cores;
    uint32_t start_id;
    fwk_id_t pd_id;
    int status;

    /* Ensure we're dealing with an element id */
    if (!fwk_id_is_type(safety_island_cluster_id, FWK_ID_TYPE_ELEMENT)) {
        return FWK_E_PARAM;
    }

    safety_island_cluster_ctx =
        &ctx.safety_island_ctx_table[fwk_id_get_element_idx(
            safety_island_cluster_id)];

    cluster_offset =
        safety_island_cluster_ctx->config->cluster_layout.core_offset;
    num_cores = safety_island_cluster_ctx->config->cluster_layout.num_cores;

    start_id = platform_get_core_count() + cluster_offset +
        platform_get_cluster_count();

    /* Composite Power Domain state to be set for the Safety Island */
    pd_state = MOD_PD_COMPOSITE_STATE(
        MOD_PD_LEVEL_1, 0, 0, MOD_PD_STATE_ON, MOD_PD_STATE_ON);

    for (core = 0; core < num_cores; core++) {
        pd_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, start_id + core);
        status = safety_island_cluster_ctx->pd_restricted_api->set_state(
            pd_id, false, pd_state);

        if (status != FWK_SUCCESS) {
            return status;
        }
    }

    return FWK_SUCCESS;
}

/*
 * Framework handlers
 */
static int safety_island_platform_mod_init(
    fwk_id_t module_id,
    unsigned int safety_island_cluster_count,
    const void *unused)
{
    (void)module_id;
    (void)unused;

    /* If there are no Safety Island clusters to configure, warn and exit */
    if (safety_island_cluster_count == 0) {
        FWK_LOG_WARN(
            MOD_NAME
            "No Safety Island clusters defined; skipping configuration");
        return FWK_SUCCESS;
    }

    ctx.safety_island_ctx_table = fwk_mm_calloc(
        safety_island_cluster_count, sizeof(ctx.safety_island_ctx_table[0]));

    ctx.safety_island_cluster_count = safety_island_cluster_count;

    return FWK_SUCCESS;
}

static int safety_island_platform_cluster_init(
    fwk_id_t safety_island_cluster_id,
    unsigned int sub_element_count,
    const void *data)
{
    const struct safety_island_cluster_config *config;
    struct safety_island_cluster_ctx *safety_island_cluster_ctx;
    (void)sub_element_count;

    config = (const struct safety_island_cluster_config *)data;
    if (config == NULL) {
        return FWK_E_DATA;
    }

    safety_island_cluster_ctx =
        &ctx.safety_island_ctx_table[fwk_id_get_element_idx(
            safety_island_cluster_id)];

    safety_island_cluster_ctx->config = config;

    return FWK_SUCCESS;
}

static int safety_island_platform_bind(fwk_id_t id, unsigned int round)
{
    struct safety_island_cluster_ctx *safety_island_cluster_ctx;

    if ((round != 0) || fwk_id_is_type(id, FWK_ID_TYPE_MODULE)) {
        return FWK_SUCCESS;
    }

    safety_island_cluster_ctx =
        &ctx.safety_island_ctx_table[fwk_id_get_element_idx(id)];

    return fwk_module_bind(
        fwk_module_id_power_domain,
        mod_pd_api_id_restricted,
        &safety_island_cluster_ctx->pd_restricted_api);
}

static int safety_island_start(fwk_id_t id)
{
    /* Only start per-element; nothing to do at module start */
    if (fwk_id_is_type(id, FWK_ID_TYPE_MODULE)) {
        return FWK_SUCCESS;
    }

    return init_si_cluster_cores(id);
}

const struct fwk_module module_safety_island_platform = {
    .type = FWK_MODULE_TYPE_HAL,
    .init = safety_island_platform_mod_init,
    .element_init = safety_island_platform_cluster_init,
    .bind = safety_island_platform_bind,
    .start = safety_island_start,
};
