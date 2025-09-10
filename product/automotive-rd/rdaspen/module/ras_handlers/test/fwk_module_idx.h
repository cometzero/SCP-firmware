/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEST_FWK_MODULE_MODULE_IDX_H
#define TEST_FWK_MODULE_MODULE_IDX_H

#include <fwk_id.h>

enum fwk_module_idx {
    FWK_MODULE_IDX_RAS_HANDLERS,
    FWK_MODULE_IDX_TEST_MODULE,
    FWK_MODULE_IDX_SSU,
    FWK_MODULE_IDX_TRANSPORT,
    FWK_MODULE_IDX_TIMER,
    FWK_MODULE_IDX_COUNT,
};

static const fwk_id_t fwk_module_id_ras_handlers =
    FWK_ID_MODULE_INIT(FWK_MODULE_IDX_RAS_HANDLERS);

static const fwk_id_t fwk_module_id_ssu =
    FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SSU);

static const fwk_id_t fwk_module_id_transport =
    FWK_ID_MODULE_INIT(FWK_MODULE_IDX_TRANSPORT);

static const fwk_id_t fwk_module_id_timer =
    FWK_ID_MODULE_INIT(FWK_MODULE_IDX_TIMER);

static const fwk_id_t fwk_module_id_test_module =
    FWK_ID_MODULE_INIT(FWK_MODULE_IDX_TEST_MODULE);

static const fwk_id_t fwk_element_id_ras_handlers[] = {
    FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TEST_MODULE, 0),
    FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TEST_MODULE, 1),
    FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TEST_MODULE, 2),
    FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TEST_MODULE, 3),
};

#endif /* TEST_FWK_MODULE_MODULE_IDX_H */
