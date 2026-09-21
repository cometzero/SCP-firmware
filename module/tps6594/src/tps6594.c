/* SPDX-License-Identifier: BSD-3-Clause */
#include "tps6594.h"

#include <fwk_status.h>

int tps6594_read_status(const struct tps6594_bus *bus, uint8_t address,
    struct tps6594_status *snapshot)
{
    int status;
    unsigned int i;
    struct {
        uint8_t reg;
        uint8_t *data;
        size_t count;
    } reads[] = {
        { 0x04, snapshot ? snapshot->buck_ctrl : NULL, 10 },
        { 0x0e, snapshot ? snapshot->buck_vout : NULL, 10 },
        { 0x1d, snapshot ? snapshot->ldo_ctrl : NULL, 4 },
        { 0x23, snapshot ? snapshot->ldo_vout : NULL, 4 },
        { 0x31, snapshot ? snapshot->gpio_conf : NULL, 11 },
        { 0x3d, snapshot ? snapshot->gpio_out : NULL, 2 },
        { 0x3f, snapshot ? snapshot->gpio_in : NULL, 2 },
        { 0x6d, snapshot ? snapshot->faults : NULL, 11 },
    };

    if (!snapshot)
        return FWK_E_PARAM;
    /* Live status only: do not read or acknowledge latched INT/RTC state. */
    for (i = 0; i < sizeof(reads) / sizeof(reads[0]); i++) {
        status = bus->transfer(bus->ctx, address, reads[i].reg,
            reads[i].data, reads[i].count, true);
        if (status != FWK_SUCCESS)
            return status;
    }
    return FWK_SUCCESS;
}

int tps6594_probe(const struct tps6594_bus *bus, uint8_t address)
{
    uint8_t revision;

    /* DEV_REV has no read-to-clear side effects. Leave all PMIC state intact. */
    return bus->transfer(bus->ctx, address, 0x01, &revision, 1, true);
}

static int update(const struct tps6594_bus *bus, uint8_t address,
    uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t old, verify;
    int status = bus->transfer(bus->ctx, address, reg, &old, 1, true);

    if (status != FWK_SUCCESS)
        return status;
    value = (old & ~mask) | (value & mask);
    if (value == old)
        return FWK_SUCCESS;
    status = bus->transfer(bus->ctx, address, reg, &value, 1, false);
    if (status != FWK_SUCCESS)
        return status;
    status = bus->transfer(bus->ctx, address, reg, &verify, 1, true);
    return status != FWK_SUCCESS ? status :
        (verify == value ? FWK_SUCCESS : FWK_E_DEVICE);
}

static int selector(unsigned int rail, uint32_t uv, uint8_t *value)
{
    uint32_t base, step, first;

    if (rail >= TPS6594_RAIL_COUNT)
        return FWK_E_PARAM;
    if (rail < 5) {
        if (uv < 600000) {
            base = 300000; step = 20000; first = 0;
            if (uv > 580000)
                return FWK_E_RANGE;
        } else if (uv < 1100000) {
            base = 600000; step = 5000; first = 0x0f;
        } else if (uv < 1660000) {
            base = 1100000; step = 10000; first = 0x73;
        } else {
            base = 1660000; step = 20000; first = 0xab;
            if (uv > 3340000)
                return FWK_E_RANGE;
        }
    } else if (rail < 8) {
        base = 600000; step = 50000; first = 4;
        if (uv > 3300000)
            return FWK_E_RANGE;
    } else {
        base = 1200000; step = 25000; first = 0x20;
        if (uv > 3300000)
            return FWK_E_RANGE;
    }
    if (uv < base || (uv - base) % step)
        return FWK_E_RANGE;
    *value = first + (uv - base) / step;
    if (rail >= 5 && rail < 8)
        *value <<= 1;
    return FWK_SUCCESS;
}

int tps6594_voltage(const struct tps6594_bus *bus, uint8_t address,
    unsigned int rail, uint32_t uv)
{
    uint8_t value, control, reg;
    int status = selector(rail, uv, &value);

    if (status != FWK_SUCCESS)
        return status;
    reg = rail < 5 ? 0x0e + rail * 2 : 0x23 + rail - 5;
    if (rail < 5) {
        status = bus->transfer(bus->ctx, address, 0x04 + rail * 2,
            &control, 1, true);
        if (status != FWK_SUCCESS)
            return status;
        if (control & 8)
            reg++;
    }
    return update(bus, address, reg,
        rail < 5 ? 0xff : (rail < 8 ? 0x7e : 0x7f), value);
}

int tps6594_get_voltage(const struct tps6594_bus *bus, uint8_t address,
    unsigned int rail, uint32_t *uv)
{
    uint8_t value, control, reg;
    uint32_t voltage;
    int status;

    if (rail >= TPS6594_RAIL_COUNT || !uv)
        return FWK_E_PARAM;
    reg = rail < 5 ? 0x0e + rail * 2 : 0x23 + rail - 5;
    if (rail < 5) {
        status = bus->transfer(bus->ctx, address, 0x04 + rail * 2,
            &control, 1, true);
        if (status != FWK_SUCCESS)
            return status;
        reg += !!(control & 8);
    }
    status = bus->transfer(bus->ctx, address, reg, &value, 1, true);
    if (status != FWK_SUCCESS)
        return status;
    if (rail < 5) {
        if (value < 0x0f)
            voltage = 300000 + value * 20000;
        else if (value < 0x73)
            voltage = 600000 + (value - 0x0f) * 5000;
        else if (value < 0xab)
            voltage = 1100000 + (value - 0x73) * 10000;
        else
            voltage = 1660000 + (value - 0xab) * 20000;
    } else if (rail < 8) {
        value = (value & 0x7e) >> 1;
        if (value < 4 || value > 0x3a)
            return FWK_E_RANGE;
        voltage = 600000 + (value - 4) * 50000;
    } else {
        value &= 0x7f;
        if (value < 0x20 || value > 0x74)
            return FWK_E_RANGE;
        voltage = 1200000 + (value - 0x20) * 25000;
    }
    *uv = voltage;
    return FWK_SUCCESS;
}

int tps6594_get_enabled(const struct tps6594_bus *bus, uint8_t address,
    unsigned int rail, bool *enabled)
{
    uint8_t value;
    int status;

    if (rail >= TPS6594_RAIL_COUNT || !enabled)
        return FWK_E_PARAM;
    status = bus->transfer(bus->ctx, address,
        rail < 5 ? 0x04 + rail * 2 : 0x1d + rail - 5, &value, 1, true);
    if (status == FWK_SUCCESS)
        *enabled = !!(value & 1);
    return status;
}

int tps6594_enable(const struct tps6594_bus *bus, uint8_t address,
    unsigned int rail, bool enabled)
{
    if (rail >= TPS6594_RAIL_COUNT)
        return FWK_E_PARAM;
    return update(bus, address, rail < 5 ? 0x04 + rail * 2 : 0x1d + rail - 5,
        1, enabled ? 1 : 0);
}

int tps6594_gpio_direction(const struct tps6594_bus *bus, uint8_t address,
    unsigned int pin, bool output)
{
    if (pin >= TPS6594_GPIO_COUNT)
        return FWK_E_PARAM;
    /* GPIO function 0, preserve pull/deglitch/drive configuration. */
    return update(bus, address, 0x31 + pin, 0xe1, output ? 1 : 0);
}

int tps6594_gpio_write(const struct tps6594_bus *bus, uint8_t address,
    unsigned int pin, bool value)
{
    if (pin >= TPS6594_GPIO_COUNT)
        return FWK_E_PARAM;
    return update(bus, address, 0x3d + pin / 8, 1U << (pin % 8),
        value ? 1U << (pin % 8) : 0);
}

int tps6594_gpio_read(const struct tps6594_bus *bus, uint8_t address,
    unsigned int pin, bool *value)
{
    uint8_t data;
    int status;

    if (pin >= TPS6594_GPIO_COUNT || !value)
        return FWK_E_PARAM;
    status = bus->transfer(bus->ctx, address, 0x3f + pin / 8, &data, 1, true);
    if (status == FWK_SUCCESS)
        *value = !!(data & (1U << (pin % 8)));
    return status;
}

int tps6594_init(const struct tps6594_bus *bus, uint8_t address,
    const uint32_t rail_uv[TPS6594_RAIL_COUNT])
{
    uint8_t data[11], verify[11], controls[9], value;
    unsigned int i;
    int status;

    /* Validate all settings before changing hardware. */
    for (i = 0; i < TPS6594_RAIL_COUNT; i++) {
        status = selector(i, rail_uv[i], &value);
        if (status != FWK_SUCCESS)
            return status;
    }
    status = tps6594_probe(bus, address);
    if (status != FWK_SUCCESS)
        return status;

    /* Mask rail/VMON/GPIO faults in one burst. Leave RTC masks untouched. */
    for (i = 0; i < 9; i++)
        data[i] = 0xff;
    status = bus->transfer(bus->ctx, address, 0x49, data, 9, false);
    if (status != FWK_SUCCESS)
        return status;
    status = bus->transfer(bus->ctx, address, 0x49, verify, 9, true);
    if (status != FWK_SUCCESS)
        return status;
    for (i = 0; i < 9; i++)
        if (verify[i] != data[i])
            return FWK_E_DEVICE;
    /* W1C each leaf once, never write read-only summary bits. */
    status = bus->transfer(bus->ctx, address, 0x5c, data, 3, false);
    if (status != FWK_SUCCESS)
        return status;
    status = bus->transfer(bus->ctx, address, 0x60, data, 3, false);
    if (status != FWK_SUCCESS)
        return status;
    status = bus->transfer(bus->ctx, address, 0x64, data, 1, false);
    if (status != FWK_SUCCESS)
        return status;
    /* INT_GPIO low bits are GPIO9..11 leaves; bit 3 is a summary. */
    value = 0x07;
    status = bus->transfer(bus->ctx, address, 0x63, &value, 1, false);
    if (status != FWK_SUCCESS)
        return status;

    /* VSEL selects VOUT1 or VOUT2; preserve the inactive bank and VSEL. */
    status = bus->transfer(bus->ctx, address, 0x04, controls, 9, true);
    if (status != FWK_SUCCESS)
        return status;
    status = bus->transfer(bus->ctx, address, 0x0e, data, 10, true);
    if (status != FWK_SUCCESS)
        return status;
    for (i = 0; i < 5; i++)
        (void)selector(i, rail_uv[i], &data[i * 2 + !!(controls[i * 2] & 8)]);
    status = bus->transfer(bus->ctx, address, 0x0e, data, 10, false);
    if (status != FWK_SUCCESS)
        return status;
    status = bus->transfer(bus->ctx, address, 0x0e, verify, 10, true);
    if (status != FWK_SUCCESS)
        return status;
    for (i = 0; i < 10; i++)
        if (data[i] != verify[i])
            return FWK_E_DEVICE;
    status = bus->transfer(bus->ctx, address, 0x23, data, 4, true);
    if (status != FWK_SUCCESS)
        return status;
    for (i = 0; i < 4; i++) {
        (void)selector(i + 5, rail_uv[i + 5], &value);
        data[i] = (data[i] & (i < 3 ? 0x81 : 0x80)) | value;
    }
    status = bus->transfer(bus->ctx, address, 0x23, data, 4, false);
    if (status != FWK_SUCCESS)
        return status;
    status = bus->transfer(bus->ctx, address, 0x23, verify, 4, true);
    if (status != FWK_SUCCESS)
        return status;
    for (i = 0; i < 4; i++)
        if (data[i] != verify[i])
            return FWK_E_DEVICE;

    /* Configure all pins as inputs. Board validation drives disconnected pins. */
    status = bus->transfer(bus->ctx, address, 0x31, data, 11, true);
    if (status != FWK_SUCCESS)
        return status;
    for (i = 0; i < 11; i++)
        data[i] &= ~0xe1;
    status = bus->transfer(bus->ctx, address, 0x31, data, 11, false);
    if (status != FWK_SUCCESS)
        return status;
    status = bus->transfer(bus->ctx, address, 0x31, verify, 11, true);
    if (status != FWK_SUCCESS)
        return status;
    for (i = 0; i < 11; i++)
        if (data[i] != verify[i])
            return FWK_E_DEVICE;
    return FWK_SUCCESS;
}
