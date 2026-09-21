/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MOD_PMIC_H
#define MOD_PMIC_H

#include <fwk_id.h>

#include <stdbool.h>
#include <stdint.h>

/*!
 * \addtogroup GroupModules Modules
 * \{
 */

/*!
 * \defgroup GroupPMIC PMIC HAL
 * \brief Synchronous regulator access with one element per PMIC.
 * \{
 */

/*! PMIC element configuration. */
struct mod_pmic_dev_config {
    /*! Identifier of the PMIC in the driver module. */
    fwk_id_t driver_id;

    /*! Identifier of the PMIC driver API. */
    fwk_id_t driver_api_id;

    /*! Number of regulator rails, indexed from zero. */
    unsigned int rail_count;
};

/*! PMIC HAL API indices. */
enum mod_pmic_api_idx {
    MOD_PMIC_API_IDX_PMIC,
    MOD_PMIC_API_IDX_COUNT,
};

/*!
 * \brief PMIC client API.
 * \details Operations are synchronous and return framework status codes.
 *     IDs identify PMIC HAL elements; rail indices are driver-defined and
 *     must be less than the configured rail_count. Missing driver callbacks
 *     return FWK_E_SUPPORT. Output arguments are valid on success only.
 */
struct mod_pmic_api {
    /*! Program the regulator voltage in microvolts. */
    int (*set_voltage)(fwk_id_t id, unsigned int rail, uint32_t uv);

    /*! Read the programmed voltage in microvolts, not a measured voltage. */
    int (*get_voltage)(fwk_id_t id, unsigned int rail, uint32_t *uv);

    /*! Enable or disable the regulator. */
    int (*set_enabled)(fwk_id_t id, unsigned int rail, bool enabled);

    /*! Read the programmed enable state, not power-good status. */
    int (*get_enabled)(fwk_id_t id, unsigned int rail, bool *enabled);
};

/*!
 * \brief PMIC driver API.
 * \details Same semantics as mod_pmic_api, but IDs identify driver devices.
 *     Each callback may be NULL for an unsupported operation. The driver
 *     validates voltage constraints and reports hardware or transport errors.
 */
struct mod_pmic_driver_api {
    /*! Program the regulator voltage in microvolts. */
    int (*set_voltage)(fwk_id_t id, unsigned int rail, uint32_t uv);

    /*! Read the programmed voltage in microvolts. */
    int (*get_voltage)(fwk_id_t id, unsigned int rail, uint32_t *uv);

    /*! Enable or disable the regulator. */
    int (*set_enabled)(fwk_id_t id, unsigned int rail, bool enabled);

    /*! Read the programmed regulator enable state. */
    int (*get_enabled)(fwk_id_t id, unsigned int rail, bool *enabled);
};

/*! \} */
/*! \} */

#endif /* MOD_PMIC_H */
