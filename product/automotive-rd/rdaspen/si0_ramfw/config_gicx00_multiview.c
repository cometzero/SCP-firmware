/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mod_gicx00_multiview.h"
#include "si0_irq.h"
#include "si0_mmap.h"

#include <fwk_module.h>

/* Safety Island Cluster redistributor to view mapping table */
static const struct mod_gicx00_multiview_redistributor_map
    redistributor_map[] = {
        { SI0_GICR_BASE_VIEW0_0_0, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { SI0_GICR_BASE_VIEW0_1_0, MOD_GICX00_MULTIVIEW_VIEW_2 },
        { SI0_GICR_BASE_VIEW0_1_1, MOD_GICX00_MULTIVIEW_VIEW_2 },
        { SI0_GICR_BASE_VIEW0_1_2, MOD_GICX00_MULTIVIEW_VIEW_2 },
        { SI0_GICR_BASE_VIEW0_1_3, MOD_GICX00_MULTIVIEW_VIEW_2 },
    };

/* Safety Island Cluster SPI to view mapping table */
static const struct mod_gicx00_multiview_spi_map spi_map[] = {
    /* Safety Island CL0 SPI for view 1 */
    { CL0_SYSTEM_TIMER_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_SYSTEM_WDT_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_UART_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_MHU3_AP2SI0_NS_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_MHU3_AP2SI0_S_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_MHU3_RSE2SI0_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_FMU_CRITICAL, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_FMU_NON_CRITICAL, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_AP_ERR_CLUSTER0_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_AP_ERR_CLUSTER1_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_AP_ERR_CLUSTER2_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_AP_ERR_CLUSTER3_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },

    /* Safety Island CL1 SPI for view 2 */
    { CL1_SYSTEM_TIMER_IRQ, MOD_GICX00_MULTIVIEW_VIEW_2 },
    { CL1_SYSTEM_WDT_IRQ, MOD_GICX00_MULTIVIEW_VIEW_2 },
    { CL1_UART_IRQ, MOD_GICX00_MULTIVIEW_VIEW_2 },
};

const struct fwk_module_config config_gicx00_multiview = {
    .elements = FWK_MODULE_STATIC_ELEMENTS({
        [0] = {
            .name = "SI",
            .data =
                &(struct mod_gicx00_multiview_config){
                    .gicd_base = SI0_GICD_BASE_VIEW0,
                    .redistributor_map = redistributor_map,
                    .redistributor_map_count = FWK_ARRAY_SIZE(redistributor_map),
                    .spi_map = spi_map,
                    .spi_map_count = FWK_ARRAY_SIZE(spi_map),
                    .delayed = false,
                },
        },
        [1] = { 0 },
    }),
};
