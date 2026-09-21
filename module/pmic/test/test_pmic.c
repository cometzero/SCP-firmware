/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mod_pmic.h>

#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_status.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern const struct fwk_module module_pmic;

static const fwk_id_t driver_id = FWK_ID_ELEMENT_INIT(1, 2);
static const fwk_id_t driver_api_id = FWK_ID_API_INIT(1, 3);
static struct mod_pmic_driver_api driver_api;
static int bind_status;
static int operation_status;
static unsigned int calls;
static unsigned int expected_rail;
static uint32_t last_uv;
static bool last_enabled;

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
        *(const struct mod_pmic_driver_api **)api = &driver_api;
    return bind_status;
}

static void check_call(fwk_id_t id, unsigned int rail)
{
    assert(fwk_id_is_equal(id, driver_id));
    assert(rail == expected_rail);
    calls++;
}

static int set_voltage(fwk_id_t id, unsigned int rail, uint32_t uv)
{
    check_call(id, rail);
    last_uv = uv;
    return operation_status;
}

static int get_voltage(fwk_id_t id, unsigned int rail, uint32_t *uv)
{
    check_call(id, rail);
    *uv = last_uv;
    return operation_status;
}

static int set_enabled(fwk_id_t id, unsigned int rail, bool enabled)
{
    check_call(id, rail);
    last_enabled = enabled;
    return operation_status;
}

static int get_enabled(fwk_id_t id, unsigned int rail, bool *enabled)
{
    check_call(id, rail);
    *enabled = last_enabled;
    return operation_status;
}

int main(void)
{
    const fwk_id_t module_id = FWK_ID_MODULE_INIT(0);
    const fwk_id_t pmic_id = FWK_ID_ELEMENT_INIT(0, 0);
    const fwk_id_t invalid_id = FWK_ID_ELEMENT_INIT(0, 1);
    const fwk_id_t api_id = FWK_ID_API_INIT(0, MOD_PMIC_API_IDX_PMIC);
    struct mod_pmic_dev_config config = {
        .driver_id = driver_id,
        .driver_api_id = driver_api_id,
        .rail_count = 9,
    };
    const struct mod_pmic_api *api;
    bool enabled;
    uint32_t uv;
    unsigned int before;

    assert(module_pmic.init(module_id, 1, NULL) == FWK_SUCCESS);
    assert(module_pmic.element_init(pmic_id, 0, NULL) == FWK_E_PARAM);
    assert(module_pmic.element_init(pmic_id, 1, &config) == FWK_E_PARAM);
    assert(module_pmic.element_init(invalid_id, 0, &config) == FWK_E_PARAM);
    assert(
        module_pmic.process_bind_request(
            driver_id, module_id, api_id, (const void **)&api) == FWK_SUCCESS);
    assert(api->set_voltage(pmic_id, 0, 800000) == FWK_E_STATE);
    config.rail_count = 0;
    assert(module_pmic.element_init(pmic_id, 0, &config) == FWK_E_PARAM);
    config.rail_count = 9;
    config.driver_id = FWK_ID_NONE;
    assert(module_pmic.element_init(pmic_id, 0, &config) == FWK_E_PARAM);
    config.driver_id = driver_id;
    config.driver_api_id = driver_id;
    assert(module_pmic.element_init(pmic_id, 0, &config) == FWK_E_PARAM);
    config.driver_api_id = driver_api_id;
    assert(module_pmic.element_init(pmic_id, 0, &config) == FWK_SUCCESS);
    assert(api->set_voltage(pmic_id, 0, 800000) == FWK_E_STATE);
    assert(
        module_pmic.process_bind_request(
            driver_id, pmic_id, api_id, (const void **)&api) == FWK_SUCCESS);
    assert(
        module_pmic.process_bind_request(
            driver_id, invalid_id, api_id, (const void **)&api) == FWK_E_PARAM);
    assert(
        module_pmic.process_bind_request(
            driver_id, module_id, driver_api_id, (const void **)&api) ==
        FWK_E_PARAM);
    assert(
        module_pmic.process_bind_request(driver_id, module_id, api_id, NULL) ==
        FWK_E_PARAM);

    bind_status = FWK_E_DEVICE;
    assert(module_pmic.bind(pmic_id, 0) == FWK_E_DEVICE);
    bind_status = FWK_SUCCESS;
    assert(module_pmic.bind(pmic_id, 0) == FWK_SUCCESS);
    assert(module_pmic.bind(module_id, 0) == FWK_SUCCESS);
    assert(module_pmic.bind(pmic_id, 1) == FWK_SUCCESS);
    assert(api->set_voltage(pmic_id, 0, 800000) == FWK_E_SUPPORT);
    assert(api->get_voltage(pmic_id, 0, &uv) == FWK_E_SUPPORT);
    assert(api->set_enabled(pmic_id, 0, true) == FWK_E_SUPPORT);
    assert(api->get_enabled(pmic_id, 0, &enabled) == FWK_E_SUPPORT);
    driver_api.set_voltage = set_voltage;
    driver_api.get_voltage = get_voltage;
    driver_api.set_enabled = set_enabled;
    driver_api.get_enabled = get_enabled;

    for (expected_rail = 0; expected_rail < config.rail_count;
         expected_rail++) {
        assert(api->set_voltage(pmic_id, expected_rail, 800000) == FWK_SUCCESS);
        assert(api->get_voltage(pmic_id, expected_rail, &uv) == FWK_SUCCESS);
        assert(uv == 800000);
        assert(api->set_enabled(pmic_id, expected_rail, true) == FWK_SUCCESS);
        assert(
            api->get_enabled(pmic_id, expected_rail, &enabled) == FWK_SUCCESS);
        assert(enabled);
        assert(api->set_enabled(pmic_id, expected_rail, false) == FWK_SUCCESS);
        assert(
            api->get_enabled(pmic_id, expected_rail, &enabled) == FWK_SUCCESS);
        assert(!enabled);
    }

    before = calls;
    assert(api->get_voltage(pmic_id, 0, NULL) == FWK_E_PARAM);
    assert(api->get_enabled(pmic_id, 0, NULL) == FWK_E_PARAM);
    assert(api->set_voltage(invalid_id, 0, 800000) == FWK_E_PARAM);
    assert(api->set_voltage(driver_id, 0, 800000) == FWK_E_PARAM);
    assert(api->set_enabled(module_id, 0, true) == FWK_E_PARAM);
    assert(api->set_voltage(pmic_id, 9, 800000) == FWK_E_PARAM);
    assert(api->get_voltage(pmic_id, 9, &uv) == FWK_E_PARAM);
    assert(api->set_enabled(pmic_id, 9, true) == FWK_E_PARAM);
    assert(api->get_enabled(pmic_id, 9, &enabled) == FWK_E_PARAM);
    assert(calls == before);

    expected_rail = 3;
    operation_status = FWK_E_TIMEOUT;
    assert(api->set_voltage(pmic_id, 3, 900000) == FWK_E_TIMEOUT);
    assert(api->get_voltage(pmic_id, 3, &uv) == FWK_E_TIMEOUT);
    assert(api->set_enabled(pmic_id, 3, true) == FWK_E_TIMEOUT);
    assert(api->get_enabled(pmic_id, 3, &enabled) == FWK_E_TIMEOUT);

    puts("PMIC HAL tests: PASS");
    return 0;
}
