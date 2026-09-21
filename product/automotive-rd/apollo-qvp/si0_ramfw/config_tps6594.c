/* SPDX-License-Identifier: BSD-3-Clause */
#include <mod_tps6594.h>

#include <fwk_module.h>
#include <fwk_module_idx.h>

static const uint8_t addresses[] = { 0x48 };

#define PMIC_GPIO(n) \
    [n] = { \
        .name = "PMIC_GPIO" #n, \
        .data = &(const struct mod_tps6594_element_config){ .pmic = 0, .pin = n }, \
    }

static const struct fwk_element gpio_table[] = {
    PMIC_GPIO(0), PMIC_GPIO(1), PMIC_GPIO(2), PMIC_GPIO(3),
    PMIC_GPIO(4), PMIC_GPIO(5), PMIC_GPIO(6), PMIC_GPIO(7),
    PMIC_GPIO(8), PMIC_GPIO(9), PMIC_GPIO(10),
    [TPS6594_GPIO_COUNT] = {
        .name = "PMIC0",
        .data = &(const struct mod_tps6594_element_config){
            .pmic = 0,
            .type = MOD_TPS6594_ELEMENT_PMIC,
        },
    },
    [TPS6594_GPIO_COUNT + 1] = { 0 },
};

const struct fwk_module_config config_tps6594 = {
    .data = &(const struct mod_tps6594_config){
        .i2c_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_I2C, 0),
        .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
        .addresses = addresses,
        .count = sizeof(addresses) / sizeof(addresses[0]),
        .configure_registers = false,
        .gpio_self_test = false,
    },
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(gpio_table),
};
