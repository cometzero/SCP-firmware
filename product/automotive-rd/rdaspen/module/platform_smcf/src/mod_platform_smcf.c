/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     RD-Aspen System Support.
 */

#include <mod_platform_smcf.h>
#include <mod_smcf.h>

#include <fwk_event.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_macros.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_status.h>

#define MOD_NAME "[PLATFORM_SMCF] "

/* Module context */
struct platform_smcf_ctx {
    /* SMCF data sampling API */
    struct smcf_data_api *data_api;
};

static struct platform_smcf_ctx platform_smcf_ctx;

/* SMCF data sampling api */
static int smcf_start_sampling(fwk_id_t element_id)
{
    int status;
    status = platform_smcf_ctx.data_api->start_data_sampling(element_id);
    if (status != FWK_SUCCESS) {
        return status;
    }

    /* Add external trigger signal here if necessary */

    return FWK_SUCCESS;
}

static int smcf_stop_sampling(fwk_id_t element_id)
{
    int status;
    status = platform_smcf_ctx.data_api->stop_data_sampling(element_id);
    if (status != FWK_SUCCESS) {
        return status;
    }

    /* Add external trigger signal here if necessary */

    return FWK_SUCCESS;
}

static const struct smcf_data_api platform_smcf_sampling_api = {
    .start_data_sampling = smcf_start_sampling,
    .stop_data_sampling = smcf_stop_sampling,
};

/*
 * Framework handlers
 */
static int platform_smcf_mod_init(
    fwk_id_t module_id,
    unsigned int unused,
    const void *unused2)
{
    return FWK_SUCCESS;
}

static int platform_smcf_bind(fwk_id_t id, unsigned int round)
{
    int status;

    /* Only bind in the first round of calls */
    if (round > 0) {
        return FWK_SUCCESS;
    }

    /* Bind to SMCF Data API */
    status = fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF),
        FWK_ID_API(FWK_MODULE_IDX_SMCF, MOD_SMCF_API_IDX_DATA),
        &platform_smcf_ctx.data_api);
    if (status != FWK_SUCCESS) {
        return status;
    }
    return FWK_SUCCESS;
}

static int platform_smcf_process_bind_request(
    fwk_id_t requester_id,
    fwk_id_t pd_id,
    fwk_id_t api_id,
    const void **api)
{
    enum mod_platform_smcf_api_idx api_id_type;
    api_id_type = (enum mod_platform_smcf_api_idx)fwk_id_get_api_idx(api_id);
    switch (api_id_type) {
    case MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API:
        *api = &platform_smcf_sampling_api;
        break;
    default:
        return FWK_E_PARAM;
    }

    return FWK_SUCCESS;
}

const struct fwk_module module_platform_smcf = {
    .type = FWK_MODULE_TYPE_HAL,
    .api_count = MOD_SMCF_PLATFORM_API_COUNT,
    .init = platform_smcf_mod_init,
    .bind = platform_smcf_bind,
    .process_bind_request = platform_smcf_process_bind_request,
};

const struct fwk_module_config config_platform_smcf = { 0 };
