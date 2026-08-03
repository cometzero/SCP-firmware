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

#define MOD_NAME                    "[GICX00-MULTIVIEW] "
#define GICX00_MULTIVIEW_POLL_LIMIT 100000U

static int poll_register(uintptr_t address, uint32_t mask, uint32_t expected)
{
    unsigned int poll_count;

    for (poll_count = 0; poll_count < GICX00_MULTIVIEW_POLL_LIMIT;
         poll_count++) {
        if ((fwk_mmio_read_32(address) & mask) == expected) {
            return FWK_SUCCESS;
        }
    }

    return FWK_E_TIMEOUT;
}

static int wait_for_power_group(uintptr_t gicr_base)
{
    unsigned int poll_count;
    uint32_t reg;

    for (poll_count = 0; poll_count < GICX00_MULTIVIEW_POLL_LIMIT;
         poll_count++) {
        reg = fwk_mmio_read_32(gicr_base + GICR_PWRR);
        if (((reg & GICR_PWRR_RDGPD) != 0U) ==
            ((reg & GICR_PWRR_RDGPO) != 0U)) {
            return FWK_SUCCESS;
        }
    }

    return FWK_E_TIMEOUT;
}

static int set_redistributor_power(
    uintptr_t gicr_base,
    enum mod_gicx00_multiview_redistributor_power_state state)
{
    int status;
    uint32_t expected;
    uint32_t reg;

    if (state >= MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_COUNT) {
        return FWK_E_PARAM;
    }

    status = wait_for_power_group(gicr_base);
    if (status != FWK_SUCCESS) {
        return status;
    }

    expected = (state == MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_OFF) ?
        GICR_PWRR_RDPD :
        0U;
    reg = fwk_mmio_read_32(gicr_base + GICR_PWRR);
    if (state == MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_OFF) {
        reg |= GICR_PWRR_RDPD;
    } else {
        reg &= ~GICR_PWRR_RDPD;
    }
    fwk_mmio_write_32(gicr_base + GICR_PWRR, reg);

    return poll_register(gicr_base + GICR_PWRR, GICR_PWRR_RDPD, expected);
}

static int get_redistributor(
    fwk_id_t element_id,
    unsigned int redistributor_idx,
    uintptr_t *gicr_base)
{
    const struct mod_gicx00_multiview_config *config;

    if ((!fwk_module_is_valid_element_id(element_id)) ||
        (fwk_id_get_module_idx(element_id) !=
         FWK_MODULE_IDX_GICX00_MULTIVIEW) ||
        (gicr_base == NULL)) {
        return FWK_E_PARAM;
    }

    config = fwk_module_get_data(element_id);
    if ((config == NULL) ||
        (redistributor_idx >= config->redistributor_map_count)) {
        return FWK_E_PARAM;
    }

    *gicr_base = config->redistributor_map[redistributor_idx].gicr_base;
    return FWK_SUCCESS;
}

static int get_config(
    fwk_id_t element_id,
    const struct mod_gicx00_multiview_config **config)
{
    if ((!fwk_module_is_valid_element_id(element_id)) ||
        (fwk_id_get_module_idx(element_id) !=
         FWK_MODULE_IDX_GICX00_MULTIVIEW) ||
        (config == NULL)) {
        return FWK_E_PARAM;
    }

    *config = fwk_module_get_data(element_id);
    return (*config == NULL) ? FWK_E_PARAM : FWK_SUCCESS;
}

static int set_redistributor_power_state(
    fwk_id_t element_id,
    unsigned int redistributor_idx,
    enum mod_gicx00_multiview_redistributor_power_state state)
{
    int status;
    uintptr_t gicr_base;

    if (state >= MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_COUNT) {
        return FWK_E_PARAM;
    }

    status = get_redistributor(element_id, redistributor_idx, &gicr_base);
    if (status != FWK_SUCCESS) {
        return status;
    }

    return set_redistributor_power(gicr_base, state);
}

static int set_processor_sleep(
    fwk_id_t element_id,
    unsigned int redistributor_idx,
    bool sleep)
{
    int status;
    uintptr_t gicr_base;
    uint32_t reg;

    status = get_redistributor(element_id, redistributor_idx, &gicr_base);
    if (status != FWK_SUCCESS) {
        return status;
    }

    reg = fwk_mmio_read_32(gicr_base + GICR_WAKER);
    if ((!sleep) && ((reg & GICR_WAKER_PROCESSOR_SLEEP) != 0U)) {
        status = poll_register(
            gicr_base + GICR_WAKER,
            GICR_WAKER_CHILDREN_ASLEEP,
            GICR_WAKER_CHILDREN_ASLEEP);
        if (status != FWK_SUCCESS) {
            return status;
        }
        reg = fwk_mmio_read_32(gicr_base + GICR_WAKER);
    }

    if (sleep) {
        reg |= GICR_WAKER_PROCESSOR_SLEEP;
    } else {
        reg &= ~GICR_WAKER_PROCESSOR_SLEEP;
    }
    fwk_mmio_write_32(gicr_base + GICR_WAKER, reg);

    return poll_register(
        gicr_base + GICR_WAKER,
        GICR_WAKER_CHILDREN_ASLEEP,
        sleep ? GICR_WAKER_CHILDREN_ASLEEP : 0U);
}

static int read_redistributor_power_state(
    fwk_id_t element_id,
    unsigned int redistributor_idx,
    enum mod_gicx00_multiview_redistributor_power_state *state)
{
    int status;
    uintptr_t gicr_base;
    uint32_t reg;

    if (state == NULL) {
        return FWK_E_PARAM;
    }

    status = get_redistributor(element_id, redistributor_idx, &gicr_base);
    if (status != FWK_SUCCESS) {
        return status;
    }

    reg = fwk_mmio_read_32(gicr_base + GICR_PWRR);
    *state = ((reg & GICR_PWRR_RDPD) != 0U) ?
        MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_OFF :
        MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_ON;

    return FWK_SUCCESS;
}

static int read_redistributor_state(
    fwk_id_t element_id,
    unsigned int redistributor_idx,
    struct mod_gicx00_multiview_redistributor_state *state)
{
    int status;
    uintptr_t gicr_base;

    if (state == NULL) {
        return FWK_E_PARAM;
    }

    status = get_redistributor(element_id, redistributor_idx, &gicr_base);
    if (status != FWK_SUCCESS) {
        return status;
    }

    state->pwrr = fwk_mmio_read_32(gicr_base + GICR_PWRR);
    state->waker = fwk_mmio_read_32(gicr_base + GICR_WAKER);
    state->powered_down = (state->pwrr & GICR_PWRR_RDPD) != 0U;
    state->processor_sleep = (state->waker & GICR_WAKER_PROCESSOR_SLEEP) != 0U;
    state->children_asleep = (state->waker & GICR_WAKER_CHILDREN_ASLEEP) != 0U;

    return FWK_SUCCESS;
}

static void read_interrupt_state_from_config(
    const struct mod_gicx00_multiview_config *config,
    unsigned int interrupt_id,
    struct mod_gicx00_multiview_interrupt_state *state)
{
    uint32_t mask;
    unsigned int register_index;

    register_index = interrupt_id / 32U;
    mask = UINT32_C(1) << (interrupt_id % 32U);
    state->pending_raw = fwk_mmio_read_32(
        config->gicd_base + GICX00_GICD_ISPENDR(register_index));
    state->active_raw = fwk_mmio_read_32(
        config->gicd_base + GICX00_GICD_ISACTIVER(register_index));
    state->pending = (state->pending_raw & mask) != 0U;
    state->active = (state->active_raw & mask) != 0U;
}

static int read_interrupt_state(
    fwk_id_t element_id,
    unsigned int interrupt_id,
    struct mod_gicx00_multiview_interrupt_state *state)
{
    int status;
    const struct mod_gicx00_multiview_config *config;

    if ((interrupt_id < INTERRUPT_ID_SPI_MIN) ||
        (interrupt_id >= INTERRUPT_ID_SPI_LIMIT) || (state == NULL)) {
        return FWK_E_PARAM;
    }

    status = get_config(element_id, &config);
    if (status != FWK_SUCCESS) {
        return status;
    }

    read_interrupt_state_from_config(config, interrupt_id, state);

    return FWK_SUCCESS;
}

static int set_interrupt_state(
    fwk_id_t element_id,
    unsigned int interrupt_id,
    bool pending,
    bool active)
{
    int status;
    uint32_t mask;
    unsigned int register_index;
    unsigned int poll_count;
    const struct mod_gicx00_multiview_config *config;
    struct mod_gicx00_multiview_interrupt_state observed;

    if ((interrupt_id < INTERRUPT_ID_SPI_MIN) ||
        (interrupt_id >= INTERRUPT_ID_SPI_LIMIT)) {
        return FWK_E_PARAM;
    }

    status = get_config(element_id, &config);
    if (status != FWK_SUCCESS) {
        return status;
    }

    register_index = interrupt_id / 32U;
    mask = UINT32_C(1) << (interrupt_id % 32U);
    fwk_mmio_write_32(
        config->gicd_base +
            (pending ? GICX00_GICD_ISPENDR(register_index) :
                       GICX00_GICD_ICPENDR(register_index)),
        mask);
    fwk_mmio_write_32(
        config->gicd_base +
            (active ? GICX00_GICD_ISACTIVER(register_index) :
                      GICX00_GICD_ICACTIVER(register_index)),
        mask);

    for (poll_count = 0U; poll_count < GICX00_MULTIVIEW_POLL_LIMIT;
         poll_count++) {
        read_interrupt_state_from_config(config, interrupt_id, &observed);
        if ((observed.pending == pending) && (observed.active == active)) {
            return FWK_SUCCESS;
        }
    }

    return FWK_E_TIMEOUT;
}

static const struct mod_gicx00_multiview_power_api power_api = {
    .set_redistributor_power_state = set_redistributor_power_state,
    .set_processor_sleep = set_processor_sleep,
    .read_redistributor_power_state = read_redistributor_power_state,
    .read_redistributor_state = read_redistributor_state,
    .set_interrupt_state = set_interrupt_state,
    .read_interrupt_state = read_interrupt_state,
};

static int assign_redistributor_to_view(uintptr_t gicr_base, uint8_t view)
{
    int status;

    if (view >= MOD_GICX00_MULTIVIEW_VIEW_COUNT) {
        return FWK_E_PARAM;
    }

    status = set_redistributor_power(
        gicr_base, MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_ON);
    if (status != FWK_SUCCESS) {
        return status;
    }

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

static int gicx00_multiview_process_bind_request(
    fwk_id_t requester_id,
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void **api)
{
    if ((api == NULL) || (!fwk_module_is_valid_element_id(target_id)) ||
        (fwk_id_get_module_idx(target_id) != FWK_MODULE_IDX_GICX00_MULTIVIEW) ||
        (fwk_id_get_api_idx(api_id) != MOD_GICX00_MULTIVIEW_API_IDX_POWER)) {
        return FWK_E_PARAM;
    }

    *api = &power_api;
    return FWK_SUCCESS;
}

/* Module description */
const struct fwk_module module_gicx00_multiview = {
    .type = FWK_MODULE_TYPE_DRIVER,
    .init = gicx00_multiview_init,
    .element_init = gicx00_multiview_element_init,
    .start = gicx00_multiview_start,
    .process_bind_request = gicx00_multiview_process_bind_request,
    .api_count = MOD_GICX00_MULTIVIEW_API_COUNT,
};
