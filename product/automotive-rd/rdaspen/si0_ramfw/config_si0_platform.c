/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'si0_platform'.
 */

#include "si0_cfgd_transport.h"

#include <mod_si0_platform.h>

#include <fwk_id.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

#define RSE_SYNC_WAIT_TIMEOUT_US (800 * 1000)

struct mod_si0_platform_config system_config = {
    .primary_cpu_mpid = 0,
    .isolated_cpu_info = { .isolated_cpu_mpid_list = NULL,
                           .isolated_cpu_count = 0 },
    .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
    .rse_sync_wait_us = RSE_SYNC_WAIT_TIMEOUT_US,
    .transport_id = FWK_ID_ELEMENT_INIT(
        FWK_MODULE_IDX_TRANSPORT,
        SI0_CFGD_MOD_TRANSPORT_EIDX_RSE_WARM_SYNC),
};

struct fwk_module_config config_si0_platform = {
    .data = &system_config,
};
