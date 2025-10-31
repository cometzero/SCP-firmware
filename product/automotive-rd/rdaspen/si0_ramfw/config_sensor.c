/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'sensor'.
 */

#include "platform_core.h"
#include "si0_irq.h"
#include "si0_mmap.h"
#include "smcf_utils.h"

#include <si0_smcf.h>

#include <mod_sensor.h>
#include <mod_sensor_smcf_drv.h>

#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

#include <stdio.h>

static const struct fwk_element element_table[2] = {
    [SI0_SMCF_MLI_IDX_SMD_SENSOR_0] = {
        .name = "MGI_SMD_EXPANSION_SENSOR_DEV",
        .sub_element_count = 1,
        .data = &((struct mod_sensor_dev_config) {
            .driver_id = FWK_ID_MODULE(FWK_MODULE_IDX_SENSOR_SMCF_DRV),
            .driver_api_id = FWK_ID_API(
                FWK_MODULE_IDX_SENSOR_SMCF_DRV,
                MOD_SENSOR_SMCF_DRV_API_IDX_GET_VALUE),
        }),
    },
    [1] = { 0 },
};

struct fwk_module_config config_sensor = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(element_table),
};
