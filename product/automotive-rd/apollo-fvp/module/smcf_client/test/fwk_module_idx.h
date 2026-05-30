/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEST_FWK_MODULE_IDX_H
#define TEST_FWK_MODULE_IDX_H

#include <fwk_id.h>

enum fwk_module_idx {
    FWK_MODULE_IDX_SMCF_CLIENT,
    FWK_MODULE_IDX_PLATFORM_SMCF,
    FWK_MODULE_IDX_AMU_SMCF_DRV,
    FWK_MODULE_IDX_SENSOR_SMCF_DRV,
    FWK_MODULE_IDX_SENSOR,
    FWK_MODULE_IDX_SMCF,
    FWK_MODULE_IDX_PL011,
    FWK_MODULE_IDX_POWER_DOMAIN,
    FWK_MODULE_IDX_COUNT,
};

static const fwk_id_t fwk_module_id_smcf_client =
    FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SMCF_CLIENT);

static const fwk_id_t fwk_module_id_power_domain =
    FWK_ID_MODULE_INIT(FWK_MODULE_IDX_POWER_DOMAIN);

#endif /* TEST_FWK_MODULE_IDX_H */
