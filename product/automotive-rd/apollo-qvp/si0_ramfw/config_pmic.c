/* SPDX-License-Identifier: BSD-3-Clause */
#include <mod_pmic.h>
#include <mod_tps6594.h>

#include <fwk_module.h>
#include <fwk_module_idx.h>

static const struct fwk_element devices[] = {
    [0] = {
        .name = "BOARD_PMIC",
        .data = &(const struct mod_pmic_dev_config){
            .driver_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TPS6594,
                TPS6594_GPIO_COUNT),
            .driver_api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_TPS6594,
                MOD_TPS6594_API_IDX_PMIC_DRIVER),
            .rail_count = TPS6594_RAIL_COUNT,
        },
    },
    [1] = { 0 },
};

const struct fwk_module_config config_pmic = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(devices),
};
