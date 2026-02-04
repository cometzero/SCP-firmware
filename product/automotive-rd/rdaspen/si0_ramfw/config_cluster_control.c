/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'cluster_control'.
 */

#include "platform_core.h"
#include "si0_mmap.h"

#include <mod_cluster_control.h>
#include <mod_si0_platform.h>

#include <fwk_id.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

#define RDASPEN_AP_RVBAR             0x82000
#define CLUSTER_CONTROL_REGION_COUNT NUMBER_OF_CLUSTERS

#define CLUSTER_CONTROL_REGION_ADDR(cluster_idx) \
    SI0_ATW1_CLUSTER_UTILITY_BASE + (cluster_idx * SI0_CLUSTER_UTILITY_SIZE) + \
        SI0_CLUSTER_UTILITY_CLUSTER_CONTROL_OFFSET

static const struct mod_cluster_control_config cluster_control_config = {
    .platform_notification = {
        .notification_id = FWK_ID_NOTIFICATION_INIT(
            FWK_MODULE_IDX_SI0_PLATFORM,
            MOD_SI0_PLATFORM_NOTIFICATION_IDX_SUBSYS_INITIALIZED),
        .source_id = FWK_ID_MODULE_INIT(
            FWK_MODULE_IDX_SI0_PLATFORM),
    },
};

struct fwk_module_config config_cluster_control = {
    .data = &cluster_control_config,
    .elements = FWK_MODULE_STATIC_ELEMENTS({
        [0] = {
            .name = "AP Cluster 0",
            .data =
                &(struct mod_cluster_control_element_config){
                    .region = CLUSTER_CONTROL_REGION_ADDR(0),
                    .rvbar = RDASPEN_AP_RVBAR,
                    .astart = { 0x1401, 0, 0, 0},
                    .aend = { 0x1402, 0, 0, 0},
                },
        },
        [1] = {
            .name = "AP Cluster 1",
            .data =
                &(struct mod_cluster_control_element_config){
                    .region = CLUSTER_CONTROL_REGION_ADDR(1),
                    .rvbar = RDASPEN_AP_RVBAR,
                    .astart = { 0x1441, 0, 0, 0},
                    .aend = { 0x1442, 0, 0, 0},
                },
        },
        [2] = {
            .name = "AP Cluster 2",
            .data =
                &(struct mod_cluster_control_element_config){
                    .region = CLUSTER_CONTROL_REGION_ADDR(2),
                    .rvbar = RDASPEN_AP_RVBAR,
                    .astart = { 0x1481, 0, 0, 0},
                    .aend = { 0x1482, 0, 0, 0},
                },
        },
        [3] = {
            .name = "AP Cluster 3",
            .data =
                &(struct mod_cluster_control_element_config){
                    .region = CLUSTER_CONTROL_REGION_ADDR(3),
                    .rvbar = RDASPEN_AP_RVBAR,
                    .astart = { 0x14C1, 0, 0, 0},
                    .aend = { 0x14C2, 0, 0, 0},
                },
        },
        [4] = { 0 },
    }),
};
