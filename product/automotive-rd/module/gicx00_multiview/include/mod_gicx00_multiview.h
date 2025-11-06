/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MOD_GICX00_MULTIVIEW_H
#define MOD_GICX00_MULTIVIEW_H

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
 * \}
 */

#endif /* MOD_GICX00_MULTIVIEW_H */
