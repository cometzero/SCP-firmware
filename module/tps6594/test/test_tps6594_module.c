/* SPDX-License-Identifier: BSD-3-Clause */
#include "tps6594.h"

#include <mod_gpio.h>
#include <mod_pmic.h>
#include <mod_i2c.h>
#include <mod_timer.h>

#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_status.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const struct fwk_module module_tps6594;
extern const struct fwk_module module_gpio;
extern const struct fwk_module module_pmic;

static const fwk_id_t pmic_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_TPS6594);
static const fwk_id_t pin_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TPS6594, 0);
static const fwk_id_t i2c_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_I2C, 0);
static const fwk_id_t timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0);
static const fwk_id_t gpio_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_GPIO);
static const fwk_id_t gpio_pin_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_GPIO, 0);
static const struct mod_tps6594_element_config pin = { .pmic = 1, .pin = 10 };
static const fwk_id_t device_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TPS6594, 1);
static const fwk_id_t hal_pmic_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PMIC, 0);
static const struct mod_tps6594_element_config device = {
    .pmic = 1, .type = MOD_TPS6594_ELEMENT_PMIC,
};
static const uint8_t addresses[] = { 0x48, 0x49 };
static uint8_t registers[2][256];
static unsigned int reads, writes, buck_logs, ldo_logs, gpio_logs,
    counter_calls;
static unsigned int counter_fail_at;
static int frequency_status, transfer_status, bind_failure;
static uint32_t frequency = 1000000;
static void *allocations[2];
static unsigned int allocation_count;

bool fwk_module_is_valid_element_id(fwk_id_t id)
{
    return fwk_id_is_type(id, FWK_ID_TYPE_ELEMENT) &&
        fwk_id_get_module_idx(id) < FWK_MODULE_IDX_COUNT &&
        (fwk_id_get_element_idx(id) == 0 || fwk_id_is_equal(id, device_id));
}

const void *fwk_module_get_data(fwk_id_t id)
{
    if (fwk_id_is_equal(id, device_id))
        return &device;
    assert(fwk_id_is_equal(id, pin_id));
    return &pin;
}

void *fwk_mm_calloc(size_t count, size_t size)
{
    void *allocation = calloc(count, size);
    assert(allocation && allocation_count < 2);
    allocations[allocation_count++] = allocation;
    return allocation;
}

void fwk_log_printf(const char *format, ...)
{
    char line[256];
    va_list args;

    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    buck_logs += strstr(line, " BUCK") != NULL;
    ldo_logs += strstr(line, " LDO") != NULL;
    gpio_logs += strstr(line, " GPIO") != NULL;
}

static int get_frequency(fwk_id_t id, uint32_t *value)
{
    assert(fwk_id_is_equal(id, timer_id));
    *value = frequency;
    return frequency_status;
}

static int get_counter(fwk_id_t id, uint64_t *value)
{
    assert(fwk_id_is_equal(id, timer_id));
    counter_calls++;
    *value = counter_calls * 100;
    return counter_calls == counter_fail_at ? FWK_E_DEVICE : FWK_SUCCESS;
}

static int transfer(fwk_id_t id, struct mod_i2c_request *request)
{
    unsigned int address, reg, count;

    assert(fwk_id_is_equal(id, i2c_id));
    assert(request->target_address >= 0x48 && request->target_address <= 0x49);
    assert(request->transmit_data && request->transmit_byte_count);
    address = request->target_address - 0x48;
    reg = request->transmit_data[0];
    if (transfer_status != FWK_SUCCESS)
        return transfer_status;
    if (request->receive_byte_count) {
        assert(request->transmit_byte_count == 1 && request->receive_data);
        count = request->receive_byte_count;
        assert(count <= 15 && reg + count <= 256);
        reads++;
        memcpy(request->receive_data, registers[address] + reg, count);
    } else {
        count = request->transmit_byte_count - 1;
        assert(count && count <= 15 && reg + count <= 256);
        assert(request->receive_data == NULL);
        writes++;
        memcpy(registers[address] + reg, request->transmit_data + 1, count);
    }
    return FWK_SUCCESS;
}

static struct mod_i2c_api i2c_api = { .transfer_as_controller = transfer };
static const struct mod_timer_api timer_api = {
    .get_frequency = get_frequency,
    .get_counter = get_counter,
};

int fwk_module_bind(fwk_id_t target, fwk_id_t api_id, const void *api)
{
    if (fwk_id_is_equal(target, pin_id) || fwk_id_is_equal(target, device_id))
        return module_tps6594.process_bind_request(
            gpio_pin_id, target, api_id, (const void **)api);
    if (fwk_id_is_equal(target, i2c_id)) {
        assert(fwk_id_is_equal(api_id, mod_i2c_api_id_i2c));
        if (bind_failure == FWK_MODULE_IDX_I2C)
            return FWK_E_DEVICE;
        *(const struct mod_i2c_api **)api = &i2c_api;
    } else {
        assert(fwk_id_is_equal(target, timer_id));
        assert(fwk_id_is_equal(api_id, MOD_TIMER_API_ID_TIMER));
        if (bind_failure == FWK_MODULE_IDX_TIMER)
            return FWK_E_DEVICE;
        *(const struct mod_timer_api **)api = &timer_api;
    }
    return FWK_SUCCESS;
}

static void test_adapter(void)
{
    struct tps6594_i2c adapter = { .id = i2c_id, .api = &i2c_api };
    uint8_t data[16] = { 0x17, 0x82 };

    assert(
        tps6594_i2c_transfer(&adapter, 0x49, 0x3d, data, 2, false) ==
        FWK_SUCCESS);
    assert(registers[1][0x3d] == 0x17 && registers[1][0x3e] == 0x82);
    memset(data, 0, sizeof(data));
    assert(
        tps6594_i2c_transfer(&adapter, 0x49, 0x3d, data, 2, true) ==
        FWK_SUCCESS);
    assert(data[0] == 0x17 && data[1] == 0x82);
    assert(tps6594_i2c_transfer(NULL, 0x49, 0, data, 1, true) == FWK_E_PARAM);
    assert(
        tps6594_i2c_transfer(&adapter, 0x49, 0, NULL, 1, true) == FWK_E_PARAM);
    assert(
        tps6594_i2c_transfer(&adapter, 0x49, 0, data, 0, true) == FWK_E_PARAM);
    assert(
        tps6594_i2c_transfer(&adapter, 0x49, 0, data, 16, true) == FWK_E_PARAM);
    assert(
        tps6594_i2c_transfer(&adapter, 0x80, 0, data, 1, true) == FWK_E_PARAM);
    transfer_status = FWK_E_TIMEOUT;
    assert(
        tps6594_i2c_transfer(&adapter, 0x49, 0, data, 1, true) ==
        FWK_E_TIMEOUT);
    transfer_status = FWK_SUCCESS;
    adapter.api = NULL;
    assert(
        tps6594_i2c_transfer(&adapter, 0x49, 0, data, 1, true) ==
        FWK_E_SUPPORT);
}

int main(void)
{
    struct mod_tps6594_config config = {
        .i2c_id = i2c_id,
        .timer_id = timer_id,
        .addresses = addresses,
        .count = 2,
    };
    struct mod_gpio_dev_config gpio_config = {
        .driver_id = pin_id,
        .driver_api_id =
            FWK_ID_API_INIT(FWK_MODULE_IDX_TPS6594, MOD_TPS6594_API_IDX_GPIO),
    };
    const struct mod_gpio_api *gpio;
    const struct mod_pmic_api *pmic;
    const struct mod_pmic_dev_config pmic_config = {
        .driver_id = device_id,
        .driver_api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_TPS6594,
            MOD_TPS6594_API_IDX_PMIC_DRIVER),
        .rail_count = TPS6594_RAIL_COUNT,
    };
    unsigned int rail;
    uint32_t uv;
    uint8_t before[sizeof(registers)];
    bool value;

    test_adapter();
    reads = writes = 0;
    memset(registers, 0xa5, sizeof(registers));
    memcpy(before, registers, sizeof(before));
    assert(module_tps6594.init(pmic_id, 1, NULL) == FWK_E_DATA);
    assert(module_tps6594.init(pmic_id, 1, &config) == FWK_SUCCESS);
    assert(module_tps6594.element_init(pin_id, 0, &pin) == FWK_SUCCESS);
    assert(module_tps6594.element_init(device_id, 0, &device) == FWK_SUCCESS);
    bind_failure = FWK_MODULE_IDX_I2C;
    assert(module_tps6594.bind(pmic_id, 0) == FWK_E_DEVICE);
    bind_failure = FWK_MODULE_IDX_TIMER;
    assert(module_tps6594.bind(pmic_id, 0) == FWK_E_DEVICE);
    bind_failure = -1;
    assert(module_tps6594.bind(pmic_id, 0) == FWK_SUCCESS);
    assert(module_gpio.init(gpio_id, 1, NULL) == FWK_SUCCESS);
    assert(
        module_gpio.element_init(gpio_pin_id, 0, &gpio_config) == FWK_SUCCESS);
    assert(module_gpio.bind(gpio_pin_id, 0) == FWK_SUCCESS);
    assert(
        module_gpio.process_bind_request(
            pmic_id,
            gpio_id,
            FWK_ID_API(FWK_MODULE_IDX_GPIO, MOD_GPIO_API_IDX_GPIO),
            (const void **)&gpio) == FWK_SUCCESS);
    assert(gpio->write(gpio_pin_id, true) == FWK_E_PARAM);
    assert(module_pmic.init(FWK_ID_MODULE(FWK_MODULE_IDX_PMIC), 1, NULL) == FWK_SUCCESS);
    assert(module_pmic.element_init(hal_pmic_id, 0, &pmic_config) == FWK_SUCCESS);
    assert(module_pmic.bind(hal_pmic_id, 0) == FWK_SUCCESS);
    assert(module_pmic.process_bind_request(pmic_id, hal_pmic_id,
        FWK_ID_API(FWK_MODULE_IDX_PMIC, MOD_PMIC_API_IDX_PMIC),
        (const void **)&pmic) == FWK_SUCCESS);
    assert(pmic->get_enabled(hal_pmic_id, 0, &value) == FWK_E_STATE);

    frequency_status = FWK_E_DEVICE;
    assert(module_tps6594.start(pmic_id) == FWK_E_DEVICE);
    assert(reads == 0);
    frequency_status = FWK_SUCCESS;
    frequency = 0;
    assert(module_tps6594.start(pmic_id) == FWK_E_DATA);
    frequency = 1000000;
    counter_fail_at = 1;
    assert(module_tps6594.start(pmic_id) == FWK_E_DEVICE);
    assert(reads == 0);
    counter_fail_at = 0;
    transfer_status = FWK_E_TIMEOUT;
    assert(module_tps6594.start(pmic_id) == FWK_E_TIMEOUT);
    assert(gpio->write(gpio_pin_id, true) == FWK_E_PARAM);
    transfer_status = FWK_SUCCESS;
    counter_calls = 0;
    counter_fail_at = 2;
    assert(module_tps6594.start(pmic_id) == FWK_E_DEVICE);
    assert(gpio->write(gpio_pin_id, true) == FWK_E_PARAM);
    counter_fail_at = 0;
    reads = buck_logs = ldo_logs = gpio_logs = 0;
    assert(module_tps6594.start(pmic_id) == FWK_SUCCESS);
    assert(reads == 18 && writes == 0);
    assert(buck_logs == 10 && ldo_logs == 8 && gpio_logs == 22);
    assert(memcmp(before, registers, sizeof(before)) == 0);

    for (rail = 0; rail < TPS6594_RAIL_COUNT; rail++) {
        assert(pmic->set_voltage(hal_pmic_id, rail, rail < 5 ? 900000 : 1800000) == FWK_SUCCESS);
        assert(pmic->get_voltage(hal_pmic_id, rail, &uv) == FWK_SUCCESS);
        assert(uv == (rail < 5 ? 900000U : 1800000U));
        assert(pmic->set_enabled(hal_pmic_id, rail, false) == FWK_SUCCESS);
        assert(pmic->get_enabled(hal_pmic_id, rail, &value) == FWK_SUCCESS && !value);
        assert(pmic->set_enabled(hal_pmic_id, rail, true) == FWK_SUCCESS);
        assert(pmic->get_enabled(hal_pmic_id, rail, &value) == FWK_SUCCESS && value);
    }
    /* VSEL selects the second bank and leaves the inactive bank alone. */
    registers[1][0x04] |= 8;
    assert(pmic->set_voltage(hal_pmic_id, 0, 1100000) == FWK_SUCCESS);
    assert(pmic->get_voltage(hal_pmic_id, 0, &uv) == FWK_SUCCESS && uv == 1100000);
    assert(registers[1][0x0e] == 0x4b && registers[1][0x0f] == 0x73);
    registers[1][0x23] = 0;
    assert(pmic->get_voltage(hal_pmic_id, 5, &uv) == FWK_E_RANGE);

    assert(gpio->set_direction(gpio_pin_id, true) == FWK_SUCCESS);
    assert(registers[1][0x3b] == 5);
    assert(gpio->write(gpio_pin_id, false) == FWK_SUCCESS);
    assert(registers[1][0x3e] == 0xa1);
    registers[1][0x40] = 4;
    assert(gpio->read(gpio_pin_id, &value) == FWK_SUCCESS && value);
    registers[1][0x40] = 0;
    assert(gpio->read(gpio_pin_id, &value) == FWK_SUCCESS && !value);
    assert(memcmp(before, registers[0], sizeof(registers[0])) == 0);
    transfer_status = FWK_E_TIMEOUT;
    assert(gpio->read(gpio_pin_id, &value) == FWK_E_TIMEOUT);
    assert(pmic->get_voltage(hal_pmic_id, 0, &uv) == FWK_E_TIMEOUT);
    assert(pmic->get_enabled(hal_pmic_id, 0, &value) == FWK_E_TIMEOUT);
    while (allocation_count)
        free(allocations[--allocation_count]);
    puts(
        "TPS6594 module tests PASS: timer, bind, snapshot, GPIO/PMIC HAL, I2C "
        "adapter");
    return 0;
}
