/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEST_PCIE_SETUP_FWK_MODULE_IDX_H
#define TEST_PCIE_SETUP_FWK_MODULE_IDX_H

#include <fwk_id.h>

enum fwk_module_idx {
    FWK_MODULE_IDX_PCIE_SETUP,
    FWK_MODULE_IDX_PCIE_DISCOVERY,
    FWK_MODULE_IDX_SDS,
    FWK_MODULE_IDX_SYSTEM_INFO,
    FWK_MODULE_IDX_COUNT,
};

static const fwk_id_t fwk_module_id_pcie_setup =
    FWK_ID_MODULE_INIT(FWK_MODULE_IDX_PCIE_SETUP);

#endif
