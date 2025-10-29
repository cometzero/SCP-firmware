/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'amu_smcf_drv'.
 */

#include "platform_core.h"
#include "si0_smcf.h"
#include "smcf_utils.h"

#include <mod_amu_smcf_drv.h>

#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

#include <stdio.h>

static uint32_t counter_offsets[NUM_OF_AMU_COUNTERS] = {
    0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88
};

static const struct fwk_element element_table[SI0_SMCF_MLI_IDX_COUNT+1] = {
    [SI0_SMCF_MLI_IDX_SMD_SENSOR_0] = {
        .name = "UNUSED", /* Dummy to align with mod_smcf sub-elements */
        .data = &((struct amu_smcf_drv_element_config) {
            .smcf_mli_id = FWK_ID_SUB_ELEMENT(
                FWK_MODULE_IDX_SMCF,
                SI0_SMCF_MGI_IDX_SMD_SMCF_EXPANSION_MGI,
                SI0_SMCF_SMD_SMCF_EXPANSION_MGI_MLI_IDX_0),
            .counter_offsets = counter_offsets,
            .amu_tag_buffer_size = 0,
            .counter_size = sizeof(uint32_t),
        }),
        .sub_element_count = NUM_OF_AMU_COUNTERS,
    },
    [SI0_SMCF_MLI_IDX_AP_CLUSTER_0_AMU_0] = {
        .name = "MGI_AP_CLUSTER_0_AMU_0_DEV",
        .data = &((struct amu_smcf_drv_element_config) {
            .smcf_mli_id = FWK_ID_SUB_ELEMENT(
                FWK_MODULE_IDX_SMCF,
                SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_0,
                SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_0),
            .counter_offsets = counter_offsets,
            .amu_tag_buffer_size = 0,
            .counter_size = sizeof(uint32_t),
        }),
        .sub_element_count = NUM_OF_AMU_COUNTERS,
    },
    [SI0_SMCF_MLI_IDX_AP_CLUSTER_1_AMU_0] = {
        .name = "MGI_AP_CLUSTER_1_AMU_0_DEV",
        .data = &((struct amu_smcf_drv_element_config) {
            .smcf_mli_id = FWK_ID_SUB_ELEMENT(
                FWK_MODULE_IDX_SMCF,
                SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_1,
                SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_0),
            .counter_offsets = counter_offsets,
            .amu_tag_buffer_size = 0,
            .counter_size = sizeof(uint32_t),
        }),
        .sub_element_count = NUM_OF_AMU_COUNTERS,
    },
    [SI0_SMCF_MLI_IDX_AP_CLUSTER_2_AMU_0] = {
        .name = "MGI_AP_CLUSTER_2_AMU_0_DEV",
        .data = &((struct amu_smcf_drv_element_config) {
            .smcf_mli_id = FWK_ID_SUB_ELEMENT(
                FWK_MODULE_IDX_SMCF,
                SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_2,
                SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_0),
            .counter_offsets = counter_offsets,
            .amu_tag_buffer_size = 0,
            .counter_size = sizeof(uint32_t),
        }),
        .sub_element_count = NUM_OF_AMU_COUNTERS,
    },
    [SI0_SMCF_MLI_IDX_AP_CLUSTER_3_AMU_0] = {
        .name = "MGI_AP_CLUSTER_3_AMU_0_DEV",
        .data = &((struct amu_smcf_drv_element_config) {
            .smcf_mli_id = FWK_ID_SUB_ELEMENT(
                FWK_MODULE_IDX_SMCF,
                SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_3,
                SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_0),
            .counter_offsets = counter_offsets,
            .amu_tag_buffer_size = 0,
            .counter_size = sizeof(uint32_t),
        }),
        .sub_element_count = NUM_OF_AMU_COUNTERS,
    },
    [SI0_SMCF_MLI_IDX_COUNT] = { 0 },
};

struct fwk_module_config config_amu_smcf_drv = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(element_table),
};
