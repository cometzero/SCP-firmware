/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mod_gicx00_multiview.h"
#include "si0_irq.h"
#include "si0_mmap.h"

#include <fwk_module.h>

#define AP_SPI_FIRST 32U
#define AP_SPI_COUNT 960U

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
    { CL0_MHU3_AP2SI0_DOMAIN1_S_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_MHU3_AP2SI0_DOMAIN3_S_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_MHU3_CL12CL0_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_MHU3_RSE2SI0_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_FMU_CRITICAL, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_FMU_NON_CRITICAL, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_AP_NS_WDOG_WS1_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_AP_ERR_CLUSTER0_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_AP_ERR_CLUSTER1_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_AP_ERR_CLUSTER2_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_AP_ERR_CLUSTER3_IRQ, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_SMCF_SMD_MGI_IRQ_OUT, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_SMCF_AP_MGI_CLUSTER_0_IRQ_OUT, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_SMCF_AP_MGI_CLUSTER_1_IRQ_OUT, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_SMCF_AP_MGI_CLUSTER_2_IRQ_OUT, MOD_GICX00_MULTIVIEW_VIEW_1 },
    { CL0_SMCF_AP_MGI_CLUSTER_3_IRQ_OUT, MOD_GICX00_MULTIVIEW_VIEW_1 },

    /* Safety Island CL1 SPI for view 2 */
    { CL1_SYSTEM_TIMER_IRQ, MOD_GICX00_MULTIVIEW_VIEW_2 },
    { CL1_SYSTEM_WDT_IRQ, MOD_GICX00_MULTIVIEW_VIEW_2 },
    { CL1_UART_IRQ, MOD_GICX00_MULTIVIEW_VIEW_2 },
    { CL1_MHU3_CL1_PC_TX_IRQ, MOD_GICX00_MULTIVIEW_VIEW_2 },
    { CL1_MHU3_PC_CL1_RX_IRQ, MOD_GICX00_MULTIVIEW_VIEW_2 },
    { CL1_MHU3_CL1_CL0_TX_IRQ, MOD_GICX00_MULTIVIEW_VIEW_2 },
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

/* QVP's AP GIC implements 960 SPIs, all owned by its single AP view. */
static struct mod_gicx00_multiview_spi_map ap_spi_map[AP_SPI_COUNT];

static const struct fwk_element element_table[] = {
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
                /* AP GIC configuration must follow CMN configuration. */
                .delayed = true,
            },
    },
    [2] = { 0 },
};

static const struct fwk_element *gicx00_multiview_get_element_table(
    fwk_id_t module_id)
{
    unsigned int index;

    (void)module_id;

    for (index = 0; index < FWK_ARRAY_SIZE(ap_spi_map); index++) {
        ap_spi_map[index].spi = AP_SPI_FIRST + index;
        ap_spi_map[index].view = MOD_GICX00_MULTIVIEW_VIEW_1;
    }

    return element_table;
}

const struct fwk_module_config config_gicx00_multiview = {
    .elements = FWK_MODULE_DYNAMIC_ELEMENTS(gicx00_multiview_get_element_table),
};
