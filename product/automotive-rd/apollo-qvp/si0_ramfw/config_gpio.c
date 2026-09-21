/* SPDX-License-Identifier: BSD-3-Clause */
#include <mod_gpio.h>
#include <mod_tps6594.h>

#include <fwk_module.h>
#include <fwk_module_idx.h>

#define PMIC_GPIO(n) \
    [n] = { \
        .name = "PMIC_GPIO" #n, \
        .data = &(const struct mod_gpio_dev_config){ \
            .driver_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TPS6594, n), \
            .driver_api_id = FWK_ID_API_INIT( \
                FWK_MODULE_IDX_TPS6594, MOD_TPS6594_API_IDX_GPIO), \
        }, \
    }

static const struct fwk_element gpio_table[] = {
    PMIC_GPIO(0), PMIC_GPIO(1), PMIC_GPIO(2), PMIC_GPIO(3),
    PMIC_GPIO(4), PMIC_GPIO(5), PMIC_GPIO(6), PMIC_GPIO(7),
    PMIC_GPIO(8), PMIC_GPIO(9), PMIC_GPIO(10),
    [TPS6594_GPIO_COUNT] = { 0 },
};

const struct fwk_module_config config_gpio = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(gpio_table),
};
