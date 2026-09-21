/* SPDX-License-Identifier: BSD-3-Clause */
#include <mod_dw_apb_i2c.h>
#include <fwk_module.h>

static const struct fwk_element devices[] = {
    [0] = {
        .name = "PMIC_I2C",
        .data = &(const struct mod_dw_apb_i2c_dev_config){
            .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
            /* QVP-only SI CL0 peripheral extension. */
            .reg = 0x2a800000UL,
            .polled = true,
            /* Allow QVP quantum scheduling jitter; not a fixed delay. */
            .transfer_timeout_us = 100000,
        },
    },
    [1] = { 0 },
};

const struct fwk_module_config config_dw_apb_i2c = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(devices),
};
