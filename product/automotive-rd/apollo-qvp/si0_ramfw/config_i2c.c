/* SPDX-License-Identifier: BSD-3-Clause */
#include <mod_i2c.h>
#include <mod_dw_apb_i2c.h>
#include <fwk_module.h>

static const struct fwk_element devices[] = {
    [0] = {
        .name = "PMIC_I2C",
        .data = &(const struct mod_i2c_dev_config){
            .driver_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_DW_APB_I2C, 0),
            .api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_DW_APB_I2C,
                MOD_DW_APB_I2C_API_IDX_DRIVER),
        },
    },
    [1] = { 0 },
};

const struct fwk_module_config config_i2c = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(devices),
};
