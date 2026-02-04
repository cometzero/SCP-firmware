/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'cluster_control'.
 */

#include <mod_cluster_control.h>

#include <fwk_element.h>
#include <fwk_id.h>
#include <fwk_macros.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

static uint8_t cluster_control_reg[2][64 * FWK_KIB] = { 0 };

static struct mod_cluster_control_config cluster_control_config_direct = { 0 };

static struct mod_cluster_control_config
    cluster_control_config_notification = { .platform_notification = {
                                                .notification_id =
                                                    test_module_notification_test,
                                                .source_id =
                                                    fwk_module_id_test_module,
                                            } };

struct mod_cluster_control_element_config config_cluster_control_element[] = {
    {
        .region = (uintptr_t)cluster_control_reg[0],
        .rvbar = 0xCDCDCDCDABABABAB,
        .astart = { 0x0, 0x2, 0x4, 0x6 },
        .aend = { 0x1, 0x3, 0x5, 0x7 },
    },
    {
        .region = (uintptr_t)cluster_control_reg[1],
        .rvbar = 0xCDCDCDCDABABABAB,
        .astart = { 0x0, 0x2, 0x4, 0x6 },
        .aend = { 0x1, 0x3, 0x5, 0x7 },
    },
};
