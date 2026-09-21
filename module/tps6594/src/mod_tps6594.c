/* SPDX-License-Identifier: BSD-3-Clause */
#include "tps6594.h"

#include <mod_gpio.h>
#include <mod_pmic.h>
#include <mod_i2c.h>
#include <mod_timer.h>

#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_status.h>

static const struct mod_tps6594_config *config;
static struct tps6594_i2c controller;
static struct tps6594_bus bus = {
    .ctx = &controller,
    .transfer = tps6594_i2c_transfer,
};
static bool ready;
static const struct mod_timer_api *timer_api;

static int gpio_direction(unsigned int pmic, unsigned int pin, bool output);

static const struct mod_tps6594_element_config *pin_config(fwk_id_t id)
{
    return fwk_module_get_data(id);
}

static int driver_direction(fwk_id_t id, bool output)
{
    const struct mod_tps6594_element_config *pin;

    if (!fwk_module_is_valid_element_id(id) ||
        fwk_id_get_module_idx(id) != FWK_MODULE_IDX_TPS6594)
        return FWK_E_PARAM;
    pin = pin_config(id);
    if (pin->type != MOD_TPS6594_ELEMENT_GPIO)
        return FWK_E_PARAM;
    return gpio_direction(pin->pmic, pin->pin, output);
}

static int set_voltage(unsigned int pmic, unsigned int rail, uint32_t uv)
{
    if (!ready || pmic >= config->count)
        return FWK_E_PARAM;
    return tps6594_voltage(&bus, config->addresses[pmic], rail, uv);
}

static int set_enabled(unsigned int pmic, unsigned int rail, bool enabled)
{
    if (!ready || pmic >= config->count)
        return FWK_E_PARAM;
    return tps6594_enable(&bus, config->addresses[pmic], rail, enabled);
}

static int gpio_direction(unsigned int pmic, unsigned int pin, bool output)
{
    if (!ready || pmic >= config->count)
        return FWK_E_PARAM;
    return tps6594_gpio_direction(&bus, config->addresses[pmic], pin, output);
}

static int gpio_write(unsigned int pmic, unsigned int pin, bool value)
{
    if (!ready || pmic >= config->count)
        return FWK_E_PARAM;
    return tps6594_gpio_write(&bus, config->addresses[pmic], pin, value);
}

static int gpio_read(unsigned int pmic, unsigned int pin, bool *value)
{
    if (!ready || pmic >= config->count)
        return FWK_E_PARAM;
    return tps6594_gpio_read(&bus, config->addresses[pmic], pin, value);
}

static int driver_write(fwk_id_t id, bool value)
{
    const struct mod_tps6594_element_config *pin;

    if (!fwk_module_is_valid_element_id(id) ||
        fwk_id_get_module_idx(id) != FWK_MODULE_IDX_TPS6594)
        return FWK_E_PARAM;
    pin = pin_config(id);
    if (pin->type != MOD_TPS6594_ELEMENT_GPIO)
        return FWK_E_PARAM;
    return gpio_write(pin->pmic, pin->pin, value);
}

static int driver_read(fwk_id_t id, bool *value)
{
    const struct mod_tps6594_element_config *pin;

    if (!fwk_module_is_valid_element_id(id) ||
        fwk_id_get_module_idx(id) != FWK_MODULE_IDX_TPS6594)
        return FWK_E_PARAM;
    pin = pin_config(id);
    if (pin->type != MOD_TPS6594_ELEMENT_GPIO)
        return FWK_E_PARAM;
    return gpio_read(pin->pmic, pin->pin, value);
}

static int pmic_address(fwk_id_t id, uint8_t *address)
{
    const struct mod_tps6594_element_config *device;

    if (!fwk_module_is_valid_element_id(id) ||
        fwk_id_get_module_idx(id) != FWK_MODULE_IDX_TPS6594)
        return FWK_E_PARAM;
    device = fwk_module_get_data(id);
    if (device->type != MOD_TPS6594_ELEMENT_PMIC)
        return FWK_E_PARAM;
    if (!ready)
        return FWK_E_STATE;
    *address = config->addresses[device->pmic];
    return FWK_SUCCESS;
}

static int pmic_set_voltage(fwk_id_t id, unsigned int rail, uint32_t uv)
{
    uint8_t address;
    int status = pmic_address(id, &address);

    return status == FWK_SUCCESS ? tps6594_voltage(&bus, address, rail, uv) : status;
}

static int pmic_get_voltage(fwk_id_t id, unsigned int rail, uint32_t *uv)
{
    uint8_t address;
    int status = pmic_address(id, &address);

    return status == FWK_SUCCESS ? tps6594_get_voltage(&bus, address, rail, uv) : status;
}

static int pmic_set_enabled(fwk_id_t id, unsigned int rail, bool enabled)
{
    uint8_t address;
    int status = pmic_address(id, &address);

    return status == FWK_SUCCESS ? tps6594_enable(&bus, address, rail, enabled) : status;
}

static int pmic_get_enabled(fwk_id_t id, unsigned int rail, bool *enabled)
{
    uint8_t address;
    int status = pmic_address(id, &address);

    return status == FWK_SUCCESS ? tps6594_get_enabled(&bus, address, rail, enabled) : status;
}

static const struct mod_pmic_driver_api pmic_api = {
    .set_voltage = pmic_set_voltage,
    .get_voltage = pmic_get_voltage,
    .set_enabled = pmic_set_enabled,
    .get_enabled = pmic_get_enabled,
};

static const struct mod_gpio_driver_api gpio_api = {
    .set_direction = driver_direction,
    .write = driver_write,
    .read = driver_read,
};

static int read_faults(unsigned int pmic, uint8_t status[11])
{
    if (!ready || pmic >= config->count || !status)
        return FWK_E_PARAM;
    return bus.transfer(bus.ctx, config->addresses[pmic], 0x6d, status, 11, true);
}

static const struct mod_tps6594_api api = {
    .set_voltage = set_voltage,
    .set_enabled = set_enabled,
    .gpio_direction = gpio_direction,
    .gpio_write = gpio_write,
    .gpio_read = gpio_read,
    .read_faults = read_faults,
};

/* QVP exposes GPIO0->1 and GPIO8->9 loopbacks; restore after validation. */
static int gpio_test(uint8_t address)
{
    uint8_t original[11], direction[11], outputs[2], expected[2], inputs[2];
    unsigned int pin, i;
    int status, restore_status;

    status = bus.transfer(bus.ctx, address, 0x31, original, 11, true);
    if (status != FWK_SUCCESS)
        return status;
    status = bus.transfer(bus.ctx, address, 0x3d, outputs, 2, true);
    if (status != FWK_SUCCESS)
        return status;
    /* Reset defaults are open-drain; unpulled pins cannot self-read high. */
    for (i = 0; i < 11; i++)
        direction[i] = (original[i] & ~(1U << 1)) | 1;
    expected[0] = expected[1] = 0;
    status = bus.transfer(bus.ctx, address, 0x3d, expected, 2, false);
    if (status != FWK_SUCCESS)
        goto restore;
    status = bus.transfer(bus.ctx, address, 0x31, direction, 11, false);
    if (status != FWK_SUCCESS)
        goto restore;
    /* Walking one plus all-low checks both levels and bank boundaries. */
    for (pin = 0; pin <= 11; pin++) {
        expected[0] = pin < 8 ? 1U << pin : 0;
        expected[1] = pin >= 8 && pin < 11 ? 1U << (pin - 8) : 0;
        status = bus.transfer(bus.ctx, address, 0x3d, expected, 2, false);
        if (status != FWK_SUCCESS)
            break;
        status = bus.transfer(bus.ctx, address, 0x3f, inputs, 2, true);
        if (status != FWK_SUCCESS)
            break;
        if (inputs[0] != expected[0] || (inputs[1] & 7) != expected[1]) {
            FWK_LOG_ERR("[TPS6594] GPIO walk pin=%u actual=%02x:%02x expected=%02x:%02x",
                pin, inputs[0], inputs[1], expected[0], expected[1]);
            status = FWK_E_DEVICE;
            break;
        }
    }
    if (status != FWK_SUCCESS)
        goto restore;
    /* Validate the physical input path across both register banks. */
    direction[1] &= ~1U;
    direction[9] &= ~1U;
    status = bus.transfer(bus.ctx, address, 0x31, direction, 11, false);
    if (status != FWK_SUCCESS)
        goto restore;
    for (i = 0; i < 2; i++) {
        expected[0] = expected[1] = i;
        status = bus.transfer(bus.ctx, address, 0x3d, expected, 2, false);
        if (status != FWK_SUCCESS)
            goto restore;
        status = bus.transfer(bus.ctx, address, 0x3f, inputs, 2, true);
        if (status != FWK_SUCCESS)
            goto restore;
        if ((inputs[0] & 2) != i * 2 || (inputs[1] & 2) != i * 2) {
            FWK_LOG_ERR("[TPS6594] GPIO loop level=%u actual=%02x:%02x expected_bit1=%u",
                i, inputs[0], inputs[1], i * 2);
            status = FWK_E_DEVICE;
            goto restore;
        }
    }
restore:
    restore_status = bus.transfer(bus.ctx, address, 0x31, original, 11, false);
    if (status == FWK_SUCCESS)
        status = restore_status;
    restore_status = bus.transfer(bus.ctx, address, 0x3d, outputs, 2, false);
    if (status == FWK_SUCCESS)
        status = restore_status;
    /* Discard only GPIO events generated by the temporary loopback test. */
    expected[0] = 0x07;
    expected[1] = 0xff;
    restore_status = bus.transfer(bus.ctx, address, 0x63, expected, 2, false);
    if (status == FWK_SUCCESS)
        status = restore_status;
    restore_status = bus.transfer(bus.ctx, address, 0x63, inputs, 2, true);
    if (status == FWK_SUCCESS)
        status = restore_status;
    if (status == FWK_SUCCESS && ((inputs[0] & 7) || inputs[1])) {
        FWK_LOG_ERR("[TPS6594] GPIO clear actual=%02x:%02x expected_leaves=0",
            inputs[0], inputs[1]);
        status = FWK_E_DEVICE;
    }
    return status;
}

static int module_init(fwk_id_t id, unsigned int elements, const void *data)
{
    config = data;
    if (!config || !config->addresses || !config->count ||
        !fwk_module_is_valid_element_id(config->i2c_id) ||
        !fwk_module_is_valid_element_id(config->timer_id))
        return FWK_E_DATA;
    if (config->gpio_self_test && !config->configure_registers)
        return FWK_E_DATA;
    controller.id = config->i2c_id;
    ready = false;
    return FWK_SUCCESS;
}

static int element_init(fwk_id_t id, unsigned int sub_elements, const void *data)
{
    const struct mod_tps6594_element_config *pin = data;

    if (!pin || pin->pmic >= config->count ||
        (pin->type != MOD_TPS6594_ELEMENT_GPIO && pin->type != MOD_TPS6594_ELEMENT_PMIC) ||
        (pin->type == MOD_TPS6594_ELEMENT_GPIO && pin->pin >= TPS6594_GPIO_COUNT))
        return FWK_E_DATA;
    return FWK_SUCCESS;
}

static int module_bind(fwk_id_t id, unsigned int round)
{
    int status;

    if (round || !fwk_id_is_type(id, FWK_ID_TYPE_MODULE))
        return FWK_SUCCESS;
    status = fwk_module_bind(config->i2c_id, mod_i2c_api_id_i2c,
        &controller.api);
    if (status != FWK_SUCCESS)
        return status;
    return fwk_module_bind(config->timer_id, MOD_TIMER_API_ID_TIMER, &timer_api);
}

static int print_status(uint8_t address)
{
    struct tps6594_status s;
    unsigned int i, bank;
    int status = tps6594_read_status(&bus, address, &s);

    if (status != FWK_SUCCESS)
        return status;
    for (i = 0; i < 5; i++) {
        bank = !!(s.buck_ctrl[i * 2] & 8);
        FWK_LOG_INFO("[TPS6594] address=0x%02x BUCK%u enable=%u "
            "vout_bank=%u vout_raw=0x%02x stat=0x%02x", address, i + 1,
            s.buck_ctrl[i * 2] & 1, bank + 1, s.buck_vout[i * 2 + bank],
            s.faults[i / 2]);
    }
    for (i = 0; i < 4; i++)
        FWK_LOG_INFO("[TPS6594] address=0x%02x LDO%u enable=%u "
            "vout_raw=0x%02x stat=0x%02x", address, i + 1,
            s.ldo_ctrl[i] & 1, s.ldo_vout[i], s.faults[3 + i / 2]);
    for (i = 0; i < TPS6594_GPIO_COUNT; i++)
        FWK_LOG_INFO("[TPS6594] address=0x%02x GPIO%u mux=%u "
            "direction=%s out=%u in=%u", address, i + 1,
            s.gpio_conf[i] >> 5, s.gpio_conf[i] & 1 ? "output" : "input",
            (s.gpio_out[i / 8] >> (i % 8)) & 1,
            (s.gpio_in[i / 8] >> (i % 8)) & 1);
    return FWK_SUCCESS;
}

static int module_start(fwk_id_t id)
{
    unsigned int i;
    uint32_t frequency;
    uint64_t start, end, elapsed;
    int status;

    if (!fwk_id_is_type(id, FWK_ID_TYPE_MODULE))
        return FWK_SUCCESS;
    status = timer_api->get_frequency(config->timer_id, &frequency);
    if (status != FWK_SUCCESS)
        return status;
    if (!frequency)
        return FWK_E_DATA;
    status = timer_api->get_counter(config->timer_id, &start);
    if (status != FWK_SUCCESS)
        return status;
    FWK_LOG_INFO("[TPS6594] begin count=%u before=power", config->count);
    if (!controller.api || !controller.api->transfer_as_controller)
        return FWK_E_SUPPORT;
    for (i = 0; i < config->count; i++) {
        status = tps6594_probe(&bus, config->addresses[i]);
        if (status == FWK_SUCCESS)
            status = print_status(config->addresses[i]);
        if (status == FWK_SUCCESS && config->configure_registers)
            status = tps6594_init(&bus, config->addresses[i], config->rail_uv);
        if (status == FWK_SUCCESS && config->gpio_self_test) {
            status = gpio_test(config->addresses[i]);
            if (status != FWK_SUCCESS)
                FWK_LOG_ERR("[TPS6594] phase=gpio address=0x%02x status=%d",
                    config->addresses[i], status);
        } else if (status != FWK_SUCCESS) {
            FWK_LOG_ERR("[TPS6594] phase=register-init address=0x%02x status=%d",
                config->addresses[i], status);
        }
        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR("[TPS6594] failed address=0x%02x status=%d",
                config->addresses[i], status);
            return status;
        }
        FWK_LOG_INFO("[TPS6594] ready address=0x%02x rails=9 gpio=11 "
            "policy=%s rtc=untouched", config->addresses[i],
            config->configure_registers ? "configure" : "preserve");
        FWK_LOG_INFO("[TPS6594] check address=0x%02x probe=PASS "
            "rail_config=%s gpio_test=%s", config->addresses[i],
            config->configure_registers ? "PASS" : "SKIP",
            config->gpio_self_test ? "PASS" : "SKIP");
    }
    status = timer_api->get_counter(config->timer_id, &end);
    if (status != FWK_SUCCESS)
        return status;
    elapsed = end - start;
    elapsed = (elapsed / frequency) * 1000000 +
        ((elapsed % frequency) * 1000000) / frequency;
    ready = true;
    FWK_LOG_INFO("[TPS6594] complete count=%u elapsed_us=%u", config->count,
        (unsigned int)elapsed);
    /* Subsequent PPU start may now perform the deferred SYS0 power-on. */
    FWK_LOG_INFO("[TPS6594] power-ready");
    return FWK_SUCCESS;
}

static int bind_request(fwk_id_t source, fwk_id_t target, fwk_id_t api_id,
    const void **result)
{
    if (fwk_id_get_api_idx(api_id) == MOD_TPS6594_API_IDX_GPIO &&
        fwk_module_is_valid_element_id(target) &&
        pin_config(target)->type == MOD_TPS6594_ELEMENT_GPIO) {
        *result = &gpio_api;
        return FWK_SUCCESS;
    }
    if (fwk_id_get_api_idx(api_id) == MOD_TPS6594_API_IDX_PMIC_DRIVER &&
        fwk_module_is_valid_element_id(target) &&
        pin_config(target)->type == MOD_TPS6594_ELEMENT_PMIC) {
        *result = &pmic_api;
        return FWK_SUCCESS;
    }
    if (!fwk_id_is_type(target, FWK_ID_TYPE_MODULE) ||
        fwk_id_get_api_idx(api_id) != MOD_TPS6594_API_IDX_PMIC)
        return FWK_E_PARAM;
    *result = &api;
    return FWK_SUCCESS;
}

const struct fwk_module module_tps6594 = {
    .type = FWK_MODULE_TYPE_DRIVER,
    .api_count = MOD_TPS6594_API_COUNT,
    .init = module_init,
    .element_init = element_init,
    .bind = module_bind,
    .start = module_start,
    .process_bind_request = bind_request,
};
