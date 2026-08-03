/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MOD_GICX00_MULTIVIEW_H
#define MOD_GICX00_MULTIVIEW_H

#include <fwk_id.h>

#include <stdbool.h>
#include <stdint.h>

/*!
 * \ingroup GroupModules
 * \addtogroup GroupGICx00MultiView Arm GICx00 Multi View
 * \{
 */

/*!
 * \brief Enumeration of the GIC views
 */
enum mod_gicx00_multiview_view {
    /*! View 0 */
    MOD_GICX00_MULTIVIEW_VIEW_0,
    /*! View 1 */
    MOD_GICX00_MULTIVIEW_VIEW_1,
    /*! View 2 */
    MOD_GICX00_MULTIVIEW_VIEW_2,
    /*! View 3 */
    MOD_GICX00_MULTIVIEW_VIEW_3,
    /*! View count */
    MOD_GICX00_MULTIVIEW_VIEW_COUNT,
};

/*!
 * \brief GIC redistributor power states.
 */
enum mod_gicx00_multiview_redistributor_power_state {
    /*! Redistributor is powered down. */
    MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_OFF,
    /*! Redistributor is powered up. */
    MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_ON,
    /*! Number of redistributor power states. */
    MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_COUNT,
};

/*!
 * \brief APIs exposed by the GIC multi-view module.
 */
enum mod_gicx00_multiview_api_idx {
    /*! Redistributor power-control API. */
    MOD_GICX00_MULTIVIEW_API_IDX_POWER,
    /*! Number of APIs exposed by the module. */
    MOD_GICX00_MULTIVIEW_API_COUNT,
};

/*! GIC redistributor state observed from the hardware registers. */
struct mod_gicx00_multiview_redistributor_state {
    uint32_t pwrr;
    uint32_t waker;
    bool powered_down;
    bool processor_sleep;
    bool children_asleep;
};

/*! GIC interrupt state observed from the Distributor registers. */
struct mod_gicx00_multiview_interrupt_state {
    uint32_t pending_raw;
    uint32_t active_raw;
    bool pending;
    bool active;
};

/*!
 * \brief Structure for GIC Multiple View PE to View mapping
 */
struct mod_gicx00_multiview_redistributor_map {
    /*! View 0 GIC redistributor base address */
    uintptr_t gicr_base;
    /*! View number */
    enum mod_gicx00_multiview_view view;
};

/*!
 * \brief Structure for GIC Multiple View SPI to View mapping
 */
struct mod_gicx00_multiview_spi_map {
    /*! SPI interrupt ID */
    uint16_t spi;
    /*! View number */
    enum mod_gicx00_multiview_view view;
};

/*!
 * \brief Structure for GIC multiple view device
 */
struct mod_gicx00_multiview_config {
    /*! View 0 GIC distributor base address */
    uintptr_t gicd_base;
    /*! Map of view 0 redistributors to view numbers */
    const struct mod_gicx00_multiview_redistributor_map *redistributor_map;
    /*! Number of redistributors */
    unsigned int redistributor_map_count;
    /*! Map of interrupt IDs to view numbers */
    const struct mod_gicx00_multiview_spi_map *spi_map;
    /*! Length of interrupt map */
    unsigned int spi_map_count;
    /*! Whether to do the initialization during element_init */
    bool delayed;
};

/*!
 * \brief GIC redistributor power-control API.
 */
struct mod_gicx00_multiview_power_api {
    /*!
     * \brief Set a redistributor power state and wait for completion.
     *
     * \retval ::FWK_SUCCESS The requested state was observed.
     * \retval ::FWK_E_PARAM An argument is invalid.
     * \retval ::FWK_E_TIMEOUT The state transition did not complete.
     */
    int (*set_redistributor_power_state)(
        fwk_id_t element_id,
        unsigned int redistributor_idx,
        enum mod_gicx00_multiview_redistributor_power_state state);

    /*!
     * \brief Set ProcessorSleep and wait for ChildrenAsleep to match.
     *
     * \retval ::FWK_SUCCESS The requested state was observed.
     * \retval ::FWK_E_PARAM An argument is invalid.
     * \retval ::FWK_E_TIMEOUT The state transition did not complete.
     */
    int (*set_processor_sleep)(
        fwk_id_t element_id,
        unsigned int redistributor_idx,
        bool sleep);

    /*!
     * \brief Read the current redistributor power state.
     *
     * \retval ::FWK_SUCCESS The state was returned.
     * \retval ::FWK_E_PARAM An argument is invalid.
     */
    int (*read_redistributor_power_state)(
        fwk_id_t element_id,
        unsigned int redistributor_idx,
        enum mod_gicx00_multiview_redistributor_power_state *state);

    /*! Read raw and decoded PWRR/WAKER state from a redistributor. */
    int (*read_redistributor_state)(
        fwk_id_t element_id,
        unsigned int redistributor_idx,
        struct mod_gicx00_multiview_redistributor_state *state);

    /*!
     * Set pending and active state for an SPI and wait for hardware readback.
     *
     * This diagnostic operation uses the architected write-one set/clear
     * registers. It is intended for guarded integration tests.
     */
    int (*set_interrupt_state)(
        fwk_id_t element_id,
        unsigned int interrupt_id,
        bool pending,
        bool active);

    /*! Read raw and decoded pending/active state for an SPI. */
    int (*read_interrupt_state)(
        fwk_id_t element_id,
        unsigned int interrupt_id,
        struct mod_gicx00_multiview_interrupt_state *state);
};

/*!
 * \}
 */

#endif /* MOD_GICX00_MULTIVIEW_H */
