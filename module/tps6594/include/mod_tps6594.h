/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef MOD_TPS6594_H
#define MOD_TPS6594_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TPS6594_RAIL_COUNT 9
#define TPS6594_GPIO_COUNT 11

/* Single firmware-thread API. No ISR callers or concurrent bus masters. */
struct mod_tps6594_config {
    uintptr_t i2c_base;
    uint64_t (*time_us)(void);
    uint32_t transfer_timeout_us;
    const uint8_t *addresses;
    unsigned int count;
    /* Default false: one read-only presence probe preserves all PMIC state. */
    bool configure_registers;
    /* Diagnostic QVP loopback test; requires configure_registers. */
    bool gpio_self_test;
    uint32_t rail_uv[TPS6594_RAIL_COUNT];
};

struct mod_tps6594_api {
    /* Rail indices: BUCK1..5 = 0..4, LDO1..4 = 5..8. */
    int (*set_voltage)(unsigned int pmic, unsigned int rail, uint32_t uv);
    int (*set_enabled)(unsigned int pmic, unsigned int rail, bool enabled);
    int (*gpio_direction)(unsigned int pmic, unsigned int pin, bool output);
    int (*gpio_write)(unsigned int pmic, unsigned int pin, bool value);
    int (*gpio_read)(unsigned int pmic, unsigned int pin, bool *value);
    /* Live non-RTC STAT_BUCK1_2..STAT_READBACK_ERR snapshot, not IRQ service. */
    int (*read_faults)(unsigned int pmic, uint8_t status[11]);
};

#endif
