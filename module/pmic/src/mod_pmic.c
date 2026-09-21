/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mod_pmic.h>

#include <fwk_id.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_status.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pmic_dev_ctx {
    const struct mod_pmic_dev_config *config;
    const struct mod_pmic_driver_api *api;
};

static struct {
    fwk_id_t id;
    unsigned int element_count;
    struct pmic_dev_ctx *devices;
} pmic_ctx;

static struct pmic_dev_ctx *get_ctx(fwk_id_t id)
{
    if (!fwk_id_is_type(id, FWK_ID_TYPE_ELEMENT) ||
        (fwk_id_get_module_idx(id) != fwk_id_get_module_idx(pmic_ctx.id)) ||
        (fwk_id_get_element_idx(id) >= pmic_ctx.element_count))
        return NULL;

    return &pmic_ctx.devices[fwk_id_get_element_idx(id)];
}

static int get_rail_ctx(
    fwk_id_t id,
    unsigned int rail,
    struct pmic_dev_ctx **ctx)
{
    *ctx = get_ctx(id);
    if (*ctx == NULL)
        return FWK_E_PARAM;
    if ((*ctx)->config == NULL)
        return FWK_E_STATE;
    if (rail >= (*ctx)->config->rail_count)
        return FWK_E_PARAM;
    if ((*ctx)->api == NULL)
        return FWK_E_STATE;

    return FWK_SUCCESS;
}

static int pmic_set_voltage(fwk_id_t id, unsigned int rail, uint32_t uv)
{
    struct pmic_dev_ctx *ctx;
    int status = get_rail_ctx(id, rail, &ctx);

    if (status != FWK_SUCCESS)
        return status;
    if (ctx->api->set_voltage == NULL)
        return FWK_E_SUPPORT;

    return ctx->api->set_voltage(ctx->config->driver_id, rail, uv);
}

static int pmic_get_voltage(fwk_id_t id, unsigned int rail, uint32_t *uv)
{
    struct pmic_dev_ctx *ctx;
    int status;

    if (uv == NULL)
        return FWK_E_PARAM;
    status = get_rail_ctx(id, rail, &ctx);
    if (status != FWK_SUCCESS)
        return status;
    if (ctx->api->get_voltage == NULL)
        return FWK_E_SUPPORT;

    return ctx->api->get_voltage(ctx->config->driver_id, rail, uv);
}

static int pmic_set_enabled(fwk_id_t id, unsigned int rail, bool enabled)
{
    struct pmic_dev_ctx *ctx;
    int status = get_rail_ctx(id, rail, &ctx);

    if (status != FWK_SUCCESS)
        return status;
    if (ctx->api->set_enabled == NULL)
        return FWK_E_SUPPORT;

    return ctx->api->set_enabled(ctx->config->driver_id, rail, enabled);
}

static int pmic_get_enabled(fwk_id_t id, unsigned int rail, bool *enabled)
{
    struct pmic_dev_ctx *ctx;
    int status;

    if (enabled == NULL)
        return FWK_E_PARAM;
    status = get_rail_ctx(id, rail, &ctx);
    if (status != FWK_SUCCESS)
        return status;
    if (ctx->api->get_enabled == NULL)
        return FWK_E_SUPPORT;

    return ctx->api->get_enabled(ctx->config->driver_id, rail, enabled);
}

static const struct mod_pmic_api pmic_api = {
    .set_voltage = pmic_set_voltage,
    .get_voltage = pmic_get_voltage,
    .set_enabled = pmic_set_enabled,
    .get_enabled = pmic_get_enabled,
};

static int pmic_init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *data)
{
    pmic_ctx.id = module_id;
    pmic_ctx.element_count = element_count;
    if (element_count != 0)
        pmic_ctx.devices =
            fwk_mm_calloc(element_count, sizeof(*pmic_ctx.devices));

    return FWK_SUCCESS;
}

static int pmic_element_init(
    fwk_id_t element_id,
    unsigned int sub_element_count,
    const void *data)
{
    struct pmic_dev_ctx *ctx = get_ctx(element_id);
    const struct mod_pmic_dev_config *config = data;

    if ((ctx == NULL) || (config == NULL) || (sub_element_count != 0))
        return FWK_E_PARAM;
    if (fwk_id_is_equal(config->driver_id, FWK_ID_NONE) ||
        !fwk_id_is_type(config->driver_api_id, FWK_ID_TYPE_API) ||
        (config->rail_count == 0))
        return FWK_E_PARAM;

    ctx->config = config;
    return FWK_SUCCESS;
}

static int pmic_bind(fwk_id_t id, unsigned int round)
{
    struct pmic_dev_ctx *ctx;

    if ((round != 0) || !fwk_id_is_type(id, FWK_ID_TYPE_ELEMENT))
        return FWK_SUCCESS;

    ctx = get_ctx(id);
    if ((ctx == NULL) || (ctx->config == NULL))
        return FWK_E_PARAM;

    return fwk_module_bind(
        ctx->config->driver_id, ctx->config->driver_api_id, &ctx->api);
}

static int pmic_process_bind_request(
    fwk_id_t source_id,
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void **api)
{
    if ((api == NULL) || !fwk_id_is_type(api_id, FWK_ID_TYPE_API) ||
        (fwk_id_get_module_idx(api_id) != fwk_id_get_module_idx(pmic_ctx.id)) ||
        (fwk_id_get_api_idx(api_id) != MOD_PMIC_API_IDX_PMIC) ||
        (!fwk_id_is_equal(target_id, pmic_ctx.id) &&
         (get_ctx(target_id) == NULL)))
        return FWK_E_PARAM;

    *api = &pmic_api;
    return FWK_SUCCESS;
}

const struct fwk_module module_pmic = {
    .type = FWK_MODULE_TYPE_HAL,
    .api_count = MOD_PMIC_API_IDX_COUNT,
    .init = pmic_init,
    .element_init = pmic_element_init,
    .bind = pmic_bind,
    .process_bind_request = pmic_process_bind_request,
};
