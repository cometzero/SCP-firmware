/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'sensor_smcf_drv'.
 */

#include "platform_core.h"
#include "smcf_utils.h"

#include <si0_smcf.h>

#include <mod_sensor_smcf_drv.h>

#include <fwk_module.h>
#include <fwk_module_idx.h>

#define MGI_SMD_EXP_SENSOR_MAX_SAMPLES_SIZE UINT8_C(48)

static const struct fwk_element element_table[2] = {
    [SI0_SMCF_MLI_IDX_SMD_SENSOR_0] = {
        .name = "MGI_SMD_EXPANSION_SENSOR_SMCF_DRV_DEV",
        .data = &((struct sensor_smcf_drv_element_config) {
            .smcf_mli_id = FWK_ID_SUB_ELEMENT(
                FWK_MODULE_IDX_SMCF,
                SI0_SMCF_MGI_IDX_SMD_SMCF_EXPANSION_MGI,
                SI0_SMCF_SMD_SMCF_EXPANSION_MGI_MLI_IDX_0),
            .max_samples_size = MGI_SMD_EXP_SENSOR_MAX_SAMPLES_SIZE,
            .sensor_tag_buffer_size = 0U,
        }),
        .sub_element_count = 1,
    },
    [1] = { 0 },
};

struct fwk_module_config config_sensor_smcf_drv = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(element_table),
};
