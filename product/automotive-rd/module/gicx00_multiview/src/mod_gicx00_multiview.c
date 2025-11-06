/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "internal/gicx00_multiview_reg.h"

#include <mod_gicx00_multiview.h>

#include <fwk_assert.h>
#include <fwk_id.h>
#include <fwk_interrupt.h>
#include <fwk_log.h>
#include <fwk_mmio.h>
#include <fwk_module.h>
#include <fwk_status.h>

#define MOD_NAME "[GICX00-MULTIVIEW] "

static int assign_redistributor_to_view(uintptr_t gicr_base, uint8_t view)
{
    uint32_t reg;

    if (view >= MOD_GICX00_MULTIVIEW_VIEW_COUNT) {
        return FWK_E_PARAM;
    }

    /* Power on redistributor */
    fwk_mmio_write_32(gicr_base + GICR_PWRR, 0);
    do {
        reg = fwk_mmio_read_32(gicr_base + GICR_PWRR);
    } while ((reg & GICR_PWRR_RDPD) != 0);

    /* Update MPID corresponding GICR_VIEWR to view-id */
    fwk_mmio_write_32(gicr_base + GICR_VIEWR, view);

    return FWK_SUCCESS;
}

static int assign_spi_to_view(
    const struct mod_gicx00_multiview_config *config,
    uint16_t spi,
    uint8_t view)
{
    uint32_t val, reg_offset, bit_index;

    if (view >= MOD_GICX00_MULTIVIEW_VIEW_COUNT) {
        return FWK_E_PARAM;
    }

    if ((spi < INTERRUPT_ID_SPI_MIN) || (spi >= INTERRUPT_ID_SPI_LIMIT)) {
        return FWK_E_PARAM;
    }

    /* GICD_IVIEWR is banked 16 fields of 2 bits per register */
    reg_offset = spi / 16;
    bit_index = (spi % 16) * 2;

    /* Update SPI corresponding GICD_IVIEWR to view-id */
    val = fwk_mmio_read_32(config->gicd_base + GICD_IVIEWR(reg_offset));
    val &= (~(GICR_VIEWR_MASK << bit_index));
    val |= (view << bit_index);
    fwk_mmio_write_32(config->gicd_base + GICD_IVIEWR(reg_offset), val);

    return FWK_SUCCESS;
}

static int check_multiview_support(
    const struct mod_gicx00_multiview_config *config)
{
    uint64_t reg;

    reg = fwk_mmio_read_64(config->gicd_base + GICD_CFGID);
    if ((reg & GICD_CFGID_VIEW) == 0) {
        return FWK_E_SUPPORT;
    }
    return FWK_SUCCESS;
}

static int configure_multiview_redistributors(
    const struct mod_gicx00_multiview_config *config)
{
    int status;
    unsigned int index;

    for (index = 0u; index < config->redistributor_map_count; index++) {
        status = assign_redistributor_to_view(
            config->redistributor_map[index].gicr_base,
            config->redistributor_map[index].view);
        if (status != FWK_SUCCESS)
            return status;
    }

    return FWK_SUCCESS;
}

static int configure_multiview_spi(
    const struct mod_gicx00_multiview_config *config)
{
    int ret;
    unsigned int index;

    for (index = 0u; index < config->spi_map_count; index++) {
        ret = assign_spi_to_view(
            config, config->spi_map[index].spi, config->spi_map[index].view);
        if (ret != FWK_SUCCESS)
            return ret;
    }

    return FWK_SUCCESS;
}

static int configure_multiview(
    fwk_id_t element_id,
    const struct mod_gicx00_multiview_config *config)
{
    int status;

    fwk_assert(config != NULL);

    /* Verify multiview support. If unsupported, skip remaining
     * configuration steps and exit silently with a warning.
     */
    status = check_multiview_support(config);
    if (status == FWK_E_SUPPORT) {
        FWK_LOG_WARN(
            MOD_NAME "%s GIC-multiview is not supported in current HW variant",
            fwk_module_get_element_name(element_id));
        return FWK_SUCCESS;
    }

    status = configure_multiview_redistributors(config);
    if (status != FWK_SUCCESS) {
        return status;
    }
    status = configure_multiview_spi(config);
    if (status != FWK_SUCCESS) {
        return status;
    }

    /* Initialize GICD_CTLR */
    /*
     * The final GICD_CTLR result of each view is the LOGICAL AND of
     * the GICD_CTLR from view-0 and the GICD_CTLR from each view.
     * Therefore in the multiview initialization, set all relevant control
     * bits in GICD_CTLR so that the software in each view can control the
     * value for their view.
     */
    fwk_mmio_write_32(
        config->gicd_base + GICD_CTLR,
        GICD_CTLR_ENABLE_GROUP_0 | GICD_CTLR_ENABLE_GROUP_1NS |
            GICD_CTLR_ENABLE_GROUP_1S);

    FWK_LOG_INFO(
        MOD_NAME "%s GIC-multiview configured successfully",
        fwk_module_get_element_name(element_id));

    return FWK_SUCCESS;
}

static int gicx00_multiview_init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *data)
{
    if (element_count == 0U) {
        /* No element to configure */
        return FWK_E_PARAM;
    }

    fwk_assert(data == NULL);

    return FWK_SUCCESS;
}

static int gicx00_multiview_element_init(
    fwk_id_t element_id,
    unsigned int element_count,
    const void *data)
{
    const struct mod_gicx00_multiview_config *config;

    if (data == NULL)
        return FWK_E_PARAM;

    config = data;

    /* Check if the element depends on other parts of the platform being
     * initialized */
    if (config->delayed) {
        return FWK_SUCCESS;
    }

    /* Perform multiview configuration */
    return configure_multiview(element_id, config);
}

static int gicx00_multiview_start(fwk_id_t id)
{
    const struct mod_gicx00_multiview_config *config;

    if (fwk_id_is_type(id, FWK_ID_TYPE_MODULE)) {
        return FWK_SUCCESS;
    }

    config = fwk_module_get_data(id);

    fwk_assert(config != NULL);

    if (!config->delayed) {
        return FWK_SUCCESS;
    }

    /* Perform multiview configuration */
    return configure_multiview(id, config);
}

/* Module description */
const struct fwk_module module_gicx00_multiview = {
    .type = FWK_MODULE_TYPE_DRIVER,
    .init = gicx00_multiview_init,
    .element_init = gicx00_multiview_element_init,
    .start = gicx00_multiview_start,
};
