/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef TPS6594_H
#define TPS6594_H

#include <mod_tps6594.h>

struct mod_i2c_api;

struct tps6594_bus {
    void *ctx;
    int (*transfer)(void *ctx, uint8_t address, uint8_t reg,
        uint8_t *data, size_t count, bool read);
};

struct tps6594_i2c {
    fwk_id_t id;
    const struct mod_i2c_api *api;
};

struct tps6594_status {
    uint8_t buck_ctrl[10];
    uint8_t buck_vout[10];
    uint8_t ldo_ctrl[4];
    uint8_t ldo_vout[4];
    uint8_t gpio_conf[11];
    uint8_t gpio_out[2];
    uint8_t gpio_in[2];
    uint8_t faults[11];
};

int tps6594_read_status(const struct tps6594_bus *bus, uint8_t address,
    struct tps6594_status *snapshot);

int tps6594_i2c_transfer(void *ctx, uint8_t address, uint8_t reg,
    uint8_t *data, size_t count, bool read);
int tps6594_probe(const struct tps6594_bus *bus, uint8_t address);
int tps6594_init(const struct tps6594_bus *bus, uint8_t address,
    const uint32_t rail_uv[TPS6594_RAIL_COUNT]);
int tps6594_voltage(const struct tps6594_bus *bus, uint8_t address,
    unsigned int rail, uint32_t uv);
int tps6594_enable(const struct tps6594_bus *bus, uint8_t address,
    unsigned int rail, bool enabled);
int tps6594_get_voltage(const struct tps6594_bus *bus, uint8_t address,
    unsigned int rail, uint32_t *uv);
int tps6594_get_enabled(const struct tps6594_bus *bus, uint8_t address,
    unsigned int rail, bool *enabled);
int tps6594_gpio_direction(const struct tps6594_bus *bus, uint8_t address,
    unsigned int pin, bool output);
int tps6594_gpio_write(const struct tps6594_bus *bus, uint8_t address,
    unsigned int pin, bool value);
int tps6594_gpio_read(const struct tps6594_bus *bus, uint8_t address,
    unsigned int pin, bool *value);

#endif
