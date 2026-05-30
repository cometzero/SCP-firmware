/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "platform_core.h"
#include "si0_irq.h"
#include "si0_mmap.h"
#include "smcf_utils.h"

#include <si0_smcf.h>

#include <mod_smcf_client.h>

#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

#include <stdio.h>

static const struct fwk_element element_table[SI0_SMCF_MGI_IDX_COUNT+1] = {
    [SI0_SMCF_MGI_IDX_SMD_SMCF_EXPANSION_MGI] = {
        .name = "SMD_MGI",
        .sub_element_count = SI0_SMCF_SMD_SMCF_EXPANSION_MGI_MLI_IDX_COUNT,
        .data = &((struct mod_smcf_client_mgi_conf) {
            .smcf_mgi_id = FWK_ID_ELEMENT(
                FWK_MODULE_IDX_SMCF,
                SI0_SMCF_MGI_IDX_SMD_SMCF_EXPANSION_MGI),
            .mlis = (struct mod_smcf_client_mli_conf[]) {
                [SI0_SMCF_SMD_SMCF_EXPANSION_MGI_MLI_IDX_0] =
                    { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_SENSOR },
            },
        }),
    },
    [SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_0] = {
        .name = "AP_CLUSTER_MGI_0",
        .sub_element_count = SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_COUNT,
        .data = &((struct mod_smcf_client_mgi_conf) {
            .smcf_mgi_id = FWK_ID_ELEMENT(
                FWK_MODULE_IDX_SMCF,
                SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_0),
            .mlis = (struct mod_smcf_client_mli_conf[]) {
                [SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_0] =
                    { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_AMU },
            },
        }),
    },
    [SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_1] = {
        .name = "AP_CLUSTER_MGI_1",
        .sub_element_count = SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_COUNT,
        .data = &((struct mod_smcf_client_mgi_conf) {
            .smcf_mgi_id = FWK_ID_ELEMENT(
                FWK_MODULE_IDX_SMCF,
                SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_1),
            .mlis = (struct mod_smcf_client_mli_conf[]) {
                [SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_0] =
                    { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_AMU },
            },
        }),
    },
    [SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_2] = {
        .name = "AP_CLUSTER_MGI_2",
        .sub_element_count = SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_COUNT,
        .data = &((struct mod_smcf_client_mgi_conf) {
            .smcf_mgi_id = FWK_ID_ELEMENT(
                FWK_MODULE_IDX_SMCF,
                SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_2),
            .mlis = (struct mod_smcf_client_mli_conf[]) {
                [SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_0] =
                    { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_AMU },
            },
        }),
    },
    [SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_3] = {
        .name = "AP_CLUSTER_MGI_3",
        .sub_element_count = SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_COUNT,
        .data = &((struct mod_smcf_client_mgi_conf) {
            .smcf_mgi_id = FWK_ID_ELEMENT(
                FWK_MODULE_IDX_SMCF,
                SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_3),
            .mlis = (struct mod_smcf_client_mli_conf[]) {
                [SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_0] =
                    { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_AMU },
            },
        }),
    },
    [SI0_SMCF_MGI_IDX_COUNT] = { 0 },
};

struct fwk_module_config config_smcf_client = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(element_table),
};
