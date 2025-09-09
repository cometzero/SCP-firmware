/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mod_gicx00.h"
#include "si0_mmap.h"
#include "si_scr_info.h"

#include <fwk_element.h>
#include <fwk_module.h>

#include <stdint.h>

static struct mod_gicx00_config gicx00_cfg;

static const struct fwk_element gicx00_no_elems[] = {
    { 0 } /* name == NULL sentinel */
};

/* Dynamic-elements supplier */
static const struct fwk_element *gicx00_get_element_table(fwk_id_t module_id)
{
    (void)module_id;

    if (si_cl1_present()) {
        gicx00_cfg.gicd_base = SI0_GICD_BASE_VIEW1_0_0;
        gicx00_cfg.gicr_base = SI0_GICR_BASE_VIEW1_0_0;
    } else {
        gicx00_cfg.gicd_base = SI0_GICD_BASE;
        gicx00_cfg.gicr_base = SI0_GICR_BASE;
    }

    return gicx00_no_elems;
}

const struct fwk_module_config config_gicx00 = {
    .elements = FWK_MODULE_DYNAMIC_ELEMENTS(gicx00_get_element_table),
    .data = &gicx00_cfg,
};
