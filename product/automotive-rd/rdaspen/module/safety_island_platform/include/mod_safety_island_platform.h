/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Safety Island Support
 */

#ifndef MOD_SAFETY_ISLAND_PLATFORM_H
#define MOD_SAFETY_ISLAND_PLATFORM_H

#include <mod_power_domain.h>

#include <fwk_id.h>

#include <stdint.h>

/*!
 * \brief Safety Island cluster layout.
 */
struct safety_island_platform_cluster_layout {
    /*! Safety Island cluster ID */
    uint32_t id;
    /*! Safety Island cluster number of cores */
    uint32_t num_cores;
    /*! Safety Island cluster core offset */
    uint32_t core_offset;
};

/*!
 * \brief Safety Island cluster configuration data.
 */
struct safety_island_cluster_config {
    /* Safety Island cluster layout */
    const struct safety_island_platform_cluster_layout cluster_layout;
};

/*!
 * \brief Safety Island cluster context data.
 */
struct safety_island_cluster_ctx {
    /*! Safety Island cluster configuration data */
    const struct safety_island_cluster_config *config;
    /* Power Domain restricted API pointer */
    const struct mod_pd_restricted_api *pd_restricted_api;
};

#endif /* MOD_SAFETY_ISLAND_PLATFORM_H */
