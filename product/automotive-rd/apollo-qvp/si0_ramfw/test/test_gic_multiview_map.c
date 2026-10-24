/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../config_gicx00_multiview.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const unsigned int cl1_interrupts[] = {
        CL1_SYSTEM_TIMER_IRQ,
        CL1_SYSTEM_WDT_IRQ,
        CL1_UART_IRQ,
        CL1_MHU3_CL1_PC_TX_IRQ,
        CL1_MHU3_PC_CL1_RX_IRQ,
        CL1_MHU3_CL1_CL0_TX_IRQ,
    };
    const struct fwk_element *elements;
    const struct mod_gicx00_multiview_config *si;
    const struct mod_gicx00_multiview_config *ap;
    unsigned int index, other, cl1_count = 0;
    bool is_cl1;

    assert(config_gicx00_multiview.elements.type ==
        FWK_MODULE_ELEMENTS_TYPE_DYNAMIC);
    elements = config_gicx00_multiview.elements.generator(FWK_ID_NONE);
    assert(strcmp(elements[0].name, "SI") == 0);
    assert(strcmp(elements[1].name, "AP") == 0);
    assert(elements[2].name == NULL);
    si = elements[0].data;
    ap = elements[1].data;

    assert(ap->gicd_base == AP_GICD_BASE_VIEW0);
    assert(ap->delayed);
    assert(ap->spi_map_count == 960);
    for (index = 0; index < ap->spi_map_count; index++) {
        assert(ap->spi_map[index].spi == 32 + index);
        assert(ap->spi_map[index].view == MOD_GICX00_MULTIVIEW_VIEW_1);
    }
    assert(ap->spi_map[959].spi == 991);
    assert(ap->redistributor_map_count == 16);
    for (index = 0; index < ap->redistributor_map_count; index++)
        assert(ap->redistributor_map[index].view == MOD_GICX00_MULTIVIEW_VIEW_1);

    assert(si->gicd_base == SI0_GICD_BASE_VIEW0);
    assert(!si->delayed);
    assert(si->spi_map_count == 26);
    for (index = 0; index < si->spi_map_count; index++) {
        is_cl1 = false;
        for (other = 0; other < FWK_ARRAY_SIZE(cl1_interrupts); other++)
            is_cl1 |= si->spi_map[index].spi == cl1_interrupts[other];
        assert(si->spi_map[index].view ==
            (is_cl1 ? MOD_GICX00_MULTIVIEW_VIEW_2 : MOD_GICX00_MULTIVIEW_VIEW_1));
        cl1_count += is_cl1;
        for (other = index + 1; other < si->spi_map_count; other++)
            assert(si->spi_map[index].spi != si->spi_map[other].spi);
    }
    assert(cl1_count == FWK_ARRAY_SIZE(cl1_interrupts));
    assert(si->redistributor_map_count == 5);
    for (index = 0; index < si->redistributor_map_count; index++)
        assert(si->redistributor_map[index].view ==
            (index == 0 ? MOD_GICX00_MULTIVIEW_VIEW_1 : MOD_GICX00_MULTIVIEW_VIEW_2));

    printf("PASS: AP INTIDs 32..991 own View1; SI View1/View2 preserved\n");
    printf("Host AP map size: %zu bytes (%zu per SPI)\n",
        sizeof(ap_spi_map), sizeof(ap_spi_map[0]));
    return 0;
}
