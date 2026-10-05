/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     PFDI Monitor
 */

#ifndef MOD_PFDI_MONITOR_H
#define MOD_PFDI_MONITOR_H

#include <fwk_id.h>

#include <stdint.h>
#include <stdbool.h>

/*!
 * \brief API indices
 */
enum mod_pfdi_monitor_api_idx {
    MOD_PFDI_MONITOR_API_IDX_PFDI_MONITOR,
    MOD_PFDI_MONITOR_API_IDX_RESTART,
    MOD_PFDI_MONITOR_API_IDX_STATUS,
    MOD_PFDI_MONITOR_API_IDX_COUNT
};

/* Read-only diagnostic state; failures remain latched until an explicit restart. */
enum mod_pfdi_monitor_fault {
    MOD_PFDI_FAULT_OOR = 1U << 0,
    MOD_PFDI_FAULT_ONLINE = 1U << 1,
    MOD_PFDI_FAULT_TIMEOUT = 1U << 2,
    MOD_PFDI_FAULT_PROTOCOL = 1U << 3,
};

struct mod_pfdi_monitor_status {
    uint32_t generation;
    uint32_t last_status;
    uint32_t faults;
    bool online;
    bool powered_off;
};

struct mod_pfdi_monitor_status_api {
    int (*get)(fwk_id_t core_id, struct mod_pfdi_monitor_status *status);
};

/*! SI0-only firmware restart boundary; ordinary hotplug must not use it. */
struct mod_pfdi_monitor_restart_api {
    int (*prepare)(fwk_id_t power_domain_id);
};

/*!
 * \brief PFDI monitor API.
 */
struct mod_pfdi_monitor_api {
    /*!
     * \brief Report the Out-of-Reset PFDI status for a core
     *
     * \param id PFDI monitor core element ID
     * \param status PFDI tests status
     *
     * \retval ::FWK_SUCCESS The operation succeeded.
     * \retval ::FWK_E_PARAM One or more parameters were incorrect.
     * \retval ::FWK_E_INIT The core framework component is not initialized.
     * \retval ::FWK_E_OS Operating system error.
     *
     * \return One of the standard framework error codes.
     */
    int (*oor_status)(fwk_id_t id, uint32_t status);
    /*!
     * \brief Report the Online PFDI status for a core
     *
     * \param id PFDI monitor core element ID
     * \param status PFDI tests status
     *
     * \retval ::FWK_SUCCESS The operation succeeded.
     * \retval ::FWK_E_PARAM One or more parameters were incorrect.
     * \retval ::FWK_E_INIT The core framework component is not initialized.
     * \retval ::FWK_E_OS Operating system error.
     *
     * \return One of the standard framework error codes.
     */
    int (*onl_status)(fwk_id_t id, uint32_t status);
};

/*!
 * \brief PFDI monitor core configuration data.
 */
struct mod_pfdi_monitor_core_config {
    /*! Alarm identifier */
    fwk_id_t alarm_id;
    /*! Power domain id to subscribe to for power state change notifications */
    fwk_id_t pd_source_id;
    /*! The out-of-reset PFDI alarm interval in microseconds */
    unsigned int oor_pfdi_period_us;
    /*! The timeout for the first online PFDI in microseconds */
    unsigned int boot_timeout_us;
    /*! The online PFDI alarm interval in microseconds */
    unsigned int onl_pfdi_period_us;
};

#endif /* MOD_PFDI_MONITOR_H */
