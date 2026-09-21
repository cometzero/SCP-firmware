/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mod_gpio.h>

#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_status.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

extern const struct fwk_module module_gpio;

static const fwk_id_t driver_id = FWK_ID_SUB_ELEMENT_INIT(1, 0, 3);
static const fwk_id_t driver_api_id = FWK_ID_API_INIT(1, 2);
static struct mod_gpio_driver_api driver_api;
static int bind_status;
static int operation_status;
static unsigned int calls;
static bool last_value;

void *fwk_mm_calloc(size_t count, size_t size)
{
    void *allocation = calloc(count, size);

    assert(allocation != NULL);
    return allocation;
}

int fwk_module_bind(fwk_id_t target, fwk_id_t api_id, const void *api)
{
    assert(fwk_id_is_equal(target, driver_id));
    assert(fwk_id_is_equal(api_id, driver_api_id));
    if (bind_status == FWK_SUCCESS)
        *(const struct mod_gpio_driver_api **)api = &driver_api;
    return bind_status;
}

static int set_value(fwk_id_t id, bool value)
{
    assert(fwk_id_is_equal(id, driver_id));
    calls++;
    last_value = value;
    return operation_status;
}

static int read_value(fwk_id_t id, bool *value)
{
    assert(fwk_id_is_equal(id, driver_id));
    calls++;
    *value = last_value;
    return operation_status;
}

int main(void)
{
    const fwk_id_t module_id = FWK_ID_MODULE_INIT(0);
    const fwk_id_t pin_id = FWK_ID_ELEMENT_INIT(0, 0);
    const fwk_id_t invalid_pin = FWK_ID_ELEMENT_INIT(0, 1);
    const fwk_id_t api_id = FWK_ID_API_INIT(0, MOD_GPIO_API_IDX_GPIO);
    struct mod_gpio_dev_config config = {
        .driver_id = driver_id,
        .driver_api_id = driver_api_id,
    };
    const struct mod_gpio_api *api;
    bool value = false;
    unsigned int before;

    assert(module_gpio.init(module_id, 1, NULL) == FWK_SUCCESS);
    assert(module_gpio.element_init(pin_id, 0, NULL) == FWK_E_PARAM);
    assert(module_gpio.element_init(pin_id, 1, &config) == FWK_E_PARAM);
    assert(module_gpio.element_init(pin_id, 0, &config) == FWK_SUCCESS);
    assert(
        module_gpio.process_bind_request(
            driver_id, module_id, api_id, (const void **)&api) == FWK_SUCCESS);
    assert(api->write(pin_id, true) == FWK_E_STATE);

    bind_status = FWK_E_DEVICE;
    assert(module_gpio.bind(pin_id, 0) == FWK_E_DEVICE);
    bind_status = FWK_SUCCESS;
    assert(module_gpio.bind(pin_id, 0) == FWK_SUCCESS);
    assert(module_gpio.bind(module_id, 0) == FWK_SUCCESS);
    assert(module_gpio.bind(pin_id, 1) == FWK_SUCCESS);

    assert(api->set_direction(pin_id, true) == FWK_E_SUPPORT);
    assert(api->write(pin_id, true) == FWK_E_SUPPORT);
    assert(api->read(pin_id, &value) == FWK_E_SUPPORT);
    driver_api.set_direction = set_value;
    driver_api.write = set_value;
    driver_api.read = read_value;
    assert(api->set_direction(pin_id, true) == FWK_SUCCESS);
    assert(last_value);
    assert(api->set_direction(pin_id, false) == FWK_SUCCESS);
    assert(!last_value);
    assert(api->write(pin_id, true) == FWK_SUCCESS);
    assert(api->read(pin_id, &value) == FWK_SUCCESS);
    assert(value);

    before = calls;
    assert(api->read(pin_id, NULL) == FWK_E_PARAM);
    assert(api->write(invalid_pin, true) == FWK_E_PARAM);
    assert(api->write(driver_id, true) == FWK_E_PARAM);
    assert(api->set_direction(module_id, true) == FWK_E_PARAM);
    assert(calls == before);
    operation_status = FWK_E_TIMEOUT;
    assert(api->write(pin_id, false) == FWK_E_TIMEOUT);
    assert(api->read(pin_id, &value) == FWK_E_TIMEOUT);
    assert(api->set_direction(pin_id, false) == FWK_E_TIMEOUT);

    puts("GPIO HAL tests: PASS");
    return 0;
}
