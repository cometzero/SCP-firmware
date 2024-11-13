/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MOD_GICX00_MULTIVIEW_H
#define MOD_GICX00_MULTIVIEW_H

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
    /*! \brief View 0 */
    MOD_GICX00_MULTIVIEW_VIEW_0,
    /*! \brief View 1 */
    MOD_GICX00_MULTIVIEW_VIEW_1,
    /*! \brief View 2 */
    MOD_GICX00_MULTIVIEW_VIEW_2,
    /*! \brief View 3 */
    MOD_GICX00_MULTIVIEW_VIEW_3,
    /*! \brief View count */
    MOD_GICX00_MULTIVIEW_VIEW_COUNT,
};

/*!
 * \brief Structure for GIC Multiple View PE to View mapping
 */
struct mod_gicx00_multiview_redistributor_map {
    /*! \brief View 0 GIC redistributor base address */
    uintptr_t gicr_base;
    /*! \brief View number */
    enum mod_gicx00_multiview_view view;
};

/*!
 * \brief Structure for GIC Multiple View SPI to View mapping
 */
struct mod_gicx00_multiview_spi_map {
    /*! \brief SPI interrupt ID */
    uint16_t spi;
    /*! \brief View number */
    enum mod_gicx00_multiview_view view;
};

/*!
 * \brief Structure for GIC multiple view device
 */
struct mod_gicx00_multiview_config {
    /*! \brief View 0 GIC distributor base address */
    uintptr_t gicd_base;
    /*! \brief Map of view 0 redistributors to view numbers */
    const struct mod_gicx00_multiview_redistributor_map *redistributor_map;
    /*! \brief Number of redistributors */
    unsigned int redistributor_map_count;
    /*! \brief Map of interrupt IDs to view numbers */
    const struct mod_gicx00_multiview_spi_map *spi_map;
    /*! \brief Length of interrupt map */
    unsigned int spi_map_count;
};

/*!
 * \}
 */

#endif /* MOD_GICX00_MULTIVIEW_H */
