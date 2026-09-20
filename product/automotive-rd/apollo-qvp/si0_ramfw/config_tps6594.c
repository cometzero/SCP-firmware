/* SPDX-License-Identifier: BSD-3-Clause */
#include <mod_tps6594.h>

#include <fwk_module.h>

/* QVP extension in the SI CL0 peripheral MPU region, not a physical IP map. */
#define SI0_QVP_PMIC_I2C_BASE 0x2a800000UL

static uint64_t time_us(void)
{
    uint64_t ticks, frequency;

    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(ticks));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
    return (ticks / frequency) * 1000000 +
        ((ticks % frequency) * 1000000) / frequency;
}

static const uint8_t addresses[] = { 0x48 };

const struct fwk_module_config config_tps6594 = {
    .data = &(const struct mod_tps6594_config){
        .i2c_base = SI0_QVP_PMIC_I2C_BASE,
        .time_us = time_us,
        /* Allow QVP quantum scheduling jitter; this is not a fixed delay. */
        .transfer_timeout_us = 100000,
        .addresses = addresses,
        .count = sizeof(addresses) / sizeof(addresses[0]),
        .configure_registers = false,
        .gpio_self_test = false,
    },
};
