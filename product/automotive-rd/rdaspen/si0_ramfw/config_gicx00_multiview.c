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

#define AP_NS_TIMER_IRQ         81
#define AP_NS_WDOG_IRQ          82
#define AP_NS_UART_IRQ          84
#define AP_MHU3_AP2SI0_NS_IRQ   144
#define AP_MHU3_SI02AP_NS_IRQ   145
#define AP_CLUSTER0_DSU_PMU_IRQ 248
#define AP_CLUSTER1_DSU_PMU_IRQ 249
#define AP_CLUSTER2_DSU_PMU_IRQ 250
#define AP_CLUSTER3_DSU_PMU_IRQ 251
#define AP_VIRTIO_BLOCK_0_IRQ   289
#define AP_VIRTIO_BLOCK_1_IRQ   290
#define AP_VIRTIO_BLOCK_2_IRQ   291
#define AP_VIRTIO_BLOCK_3_IRQ   292
#define AP_VIRTIO_NET_IRQ       293
#define AP_VIRTIO_RNG_IRQ       295
#define AP_RTC_IRQ              300

/* Safety Island Cluster redistributor to view mapping table */
static const struct mod_gicx00_multiview_redistributor_map
    si_redistributor_map[] = {
        { SI0_GICR_BASE_VIEW0_0_0, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { SI0_GICR_BASE_VIEW0_1_0, MOD_GICX00_MULTIVIEW_VIEW_2 },
        { SI0_GICR_BASE_VIEW0_1_1, MOD_GICX00_MULTIVIEW_VIEW_2 },
        { SI0_GICR_BASE_VIEW0_1_2, MOD_GICX00_MULTIVIEW_VIEW_2 },
        { SI0_GICR_BASE_VIEW0_1_3, MOD_GICX00_MULTIVIEW_VIEW_2 },
    };

/* Safety Island Cluster SPI to view mapping table */
static const struct mod_gicx00_multiview_spi_map si_spi_map[] = {
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

/* Application Processor redistributor to view mapping table */
static const struct mod_gicx00_multiview_redistributor_map
    ap_redistributor_map[] = {
        { AP_GICR_BASE_VIEW0_0_0, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_0_1, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_0_2, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_0_3, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_1_0, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_1_1, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_1_2, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_1_3, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_2_0, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_2_1, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_2_2, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_2_3, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_3_0, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_3_1, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_3_2, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { AP_GICR_BASE_VIEW0_3_3, MOD_GICX00_MULTIVIEW_VIEW_1 },
    };

/* Application Processor SPI to view mapping table */
static const struct mod_gicx00_multiview_spi_map ap_spi_map[] = {
    /* Application Processor SPI for view 1 */
    { AP_NS_TIMER_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_NS_WDOG_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_NS_UART_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_MHU3_AP2SI0_NS_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_MHU3_SI02AP_NS_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_CLUSTER0_DSU_PMU_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_CLUSTER1_DSU_PMU_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_CLUSTER2_DSU_PMU_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_CLUSTER3_DSU_PMU_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_VIRTIO_BLOCK_0_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_VIRTIO_BLOCK_1_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_VIRTIO_BLOCK_2_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_VIRTIO_BLOCK_3_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_VIRTIO_NET_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_VIRTIO_RNG_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { AP_RTC_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
};

const struct fwk_module_config config_gicx00_multiview = {
    .elements = FWK_MODULE_STATIC_ELEMENTS({
        [0] = {
            .name = "SI",
            .data =
                &(struct mod_gicx00_multiview_config){
                    .gicd_base = SI0_GICD_BASE_VIEW0,
                    .redistributor_map = si_redistributor_map,
                    .redistributor_map_count = FWK_ARRAY_SIZE(si_redistributor_map),
                    .spi_map = si_spi_map,
                    .spi_map_count = FWK_ARRAY_SIZE(si_spi_map),
                    .delayed = false,
                },
        },
        [1] = {
            .name = "AP",
            .data =
                &(struct mod_gicx00_multiview_config){
                    .gicd_base = AP_GICD_BASE_VIEW0,
                    .redistributor_map = ap_redistributor_map,
                    .redistributor_map_count = FWK_ARRAY_SIZE(ap_redistributor_map),
                    .spi_map = ap_spi_map,
                    .spi_map_count = FWK_ARRAY_SIZE(ap_spi_map),
                    /* AP GIC multiview needs to be configured after the CMN configuration */
                    .delayed = true,
                },
        },
        [2] = { 0 },
    }),
};
