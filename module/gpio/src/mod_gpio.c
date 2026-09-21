/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mod_gpio.h>

#include <fwk_id.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_status.h>

#include <stdbool.h>
#include <stddef.h>

struct gpio_dev_ctx {
    const struct mod_gpio_dev_config *config;
    const struct mod_gpio_driver_api *api;
};

static struct {
    fwk_id_t id;
    unsigned int element_count;
    struct gpio_dev_ctx *devices;
} gpio_ctx;

static struct gpio_dev_ctx *get_ctx(fwk_id_t id)
{
    if (!fwk_id_is_type(id, FWK_ID_TYPE_ELEMENT) ||
        (fwk_id_get_module_idx(id) != fwk_id_get_module_idx(gpio_ctx.id)) ||
        (fwk_id_get_element_idx(id) >= gpio_ctx.element_count))
        return NULL;

    return &gpio_ctx.devices[fwk_id_get_element_idx(id)];
}

static int gpio_set_direction(fwk_id_t id, bool output)
{
    struct gpio_dev_ctx *ctx = get_ctx(id);

    if (ctx == NULL)
        return FWK_E_PARAM;
    if (ctx->api == NULL)
        return FWK_E_STATE;
    if (ctx->api->set_direction == NULL)
        return FWK_E_SUPPORT;

    return ctx->api->set_direction(ctx->config->driver_id, output);
}

static int gpio_write(fwk_id_t id, bool value)
{
    struct gpio_dev_ctx *ctx = get_ctx(id);

    if (ctx == NULL)
        return FWK_E_PARAM;
    if (ctx->api == NULL)
        return FWK_E_STATE;
    if (ctx->api->write == NULL)
        return FWK_E_SUPPORT;

    return ctx->api->write(ctx->config->driver_id, value);
}

static int gpio_read(fwk_id_t id, bool *value)
{
    struct gpio_dev_ctx *ctx = get_ctx(id);

    if ((ctx == NULL) || (value == NULL))
        return FWK_E_PARAM;
    if (ctx->api == NULL)
        return FWK_E_STATE;
    if (ctx->api->read == NULL)
        return FWK_E_SUPPORT;

    return ctx->api->read(ctx->config->driver_id, value);
}

static const struct mod_gpio_api gpio_api = {
    .set_direction = gpio_set_direction,
    .write = gpio_write,
    .read = gpio_read,
};

static int gpio_init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *data)
{
    gpio_ctx.id = module_id;
    gpio_ctx.element_count = element_count;
    if (element_count != 0)
        gpio_ctx.devices =
            fwk_mm_calloc(element_count, sizeof(*gpio_ctx.devices));

    return FWK_SUCCESS;
}

static int gpio_element_init(
    fwk_id_t element_id,
    unsigned int sub_element_count,
    const void *data)
{
    struct gpio_dev_ctx *ctx = get_ctx(element_id);
    const struct mod_gpio_dev_config *config = data;

    if ((ctx == NULL) || (config == NULL) || (sub_element_count != 0))
        return FWK_E_PARAM;
    if (fwk_id_is_equal(config->driver_id, FWK_ID_NONE) ||
        !fwk_id_is_type(config->driver_api_id, FWK_ID_TYPE_API))
        return FWK_E_PARAM;

    ctx->config = config;
    return FWK_SUCCESS;
}

static int gpio_bind(fwk_id_t id, unsigned int round)
{
    struct gpio_dev_ctx *ctx;

    if ((round != 0) || !fwk_id_is_type(id, FWK_ID_TYPE_ELEMENT))
        return FWK_SUCCESS;

    ctx = get_ctx(id);
    if ((ctx == NULL) || (ctx->config == NULL))
        return FWK_E_PARAM;

    return fwk_module_bind(
        ctx->config->driver_id, ctx->config->driver_api_id, &ctx->api);
}

static int gpio_process_bind_request(
    fwk_id_t source_id,
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void **api)
{
    if ((fwk_id_get_api_idx(api_id) != MOD_GPIO_API_IDX_GPIO) ||
        (!fwk_id_is_equal(target_id, gpio_ctx.id) &&
         (get_ctx(target_id) == NULL)))
        return FWK_E_PARAM;

    *api = &gpio_api;
    return FWK_SUCCESS;
}

const struct fwk_module module_gpio = {
    .type = FWK_MODULE_TYPE_HAL,
    .api_count = MOD_GPIO_API_IDX_COUNT,
    .init = gpio_init,
    .element_init = gpio_element_init,
    .bind = gpio_bind,
    .process_bind_request = gpio_process_bind_request,
};
