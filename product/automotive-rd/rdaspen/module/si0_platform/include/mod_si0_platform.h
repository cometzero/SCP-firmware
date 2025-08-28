/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     SCP Platform Support
 */

#ifndef MOD_SI0_PLATFORM_H
#define MOD_SI0_PLATFORM_H

#include <fwk_id.h>
#include <fwk_module_idx.h>

#include <stdint.h>

#define WARM_RESET_MAX_RETRIES 10

/*!
 * \addtogroup GroupPLATFORMModule PLATFORM Product Modules
 * @{
 */

/*!
 * \defgroup GroupSI0Platform SI0 Platform Support
 * @{
 */

/*!
 * \brief Indices of the interfaces exposed by the module.
 */
enum mod_si0_platform_api_idx {
    /*! API index for the powerdown interface of SCMI module */
    MOD_SI0_PLATFORM_API_IDX_SCMI_POWER_DOWN,
    /*! API index for the driver interface of the SYSTEM POWER module */
    MOD_SI0_PLATFORM_API_IDX_SYSTEM_POWER_DRIVER,
    /*! Number of exposed interfaces */
    MOD_SI0_PLATFORM_API_COUNT
};

/*!
 * \brief Events used by platform system module.
 */
enum mod_si0_platform_event_idx {
    /*! Event requesting check for power domain OFF */
    MOD_SI0_PLATFORM_CHECK_PD_OFF,

    /*! Number of defined events */
    MOD_SI0_PLATFORM_EVENT_COUNT
};

/*!
 * \brief Event to check all CPUs are powered off.
 */
static const fwk_id_t mod_si0_platform_event_check_ppu_off =
    FWK_ID_EVENT(FWK_MODULE_IDX_SI0_PLATFORM, MOD_SI0_PLATFORM_CHECK_PD_OFF);

/*!
 * \brief Notification indices.
 */
enum mod_si0_platform_notification_idx {
    /*! SI0 subsystem initialization completion notification */
    MOD_SI0_PLATFORM_NOTIFICATION_IDX_SUBSYS_INITIALIZED,

    /*! Number of notifications defined by the module */
    MOD_SI0_PLATFORM_NOTIFICATION_COUNT,
};

/*!
 * \brief Identifier for the
 * ::MOD_SI0_PLATFORM_NOTIFICATION_IDX_SUBSYS_INITIALIZED notification.
 */
static const fwk_id_t mod_si0_platform_notification_subsys_init =
    FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_SI0_PLATFORM,
        MOD_SI0_PLATFORM_NOTIFICATION_IDX_SUBSYS_INITIALIZED);

/*!
 * \brief List of isolated CPU MPIDs.
 */
struct mod_si0_platform_isolated_cpu_info {
    /*! Number of isolated CPUs */
    uint64_t isolated_cpu_count;

    /*!
     * MPID of Isolated CPUs represented as a list. Value of each MPID
     * specifies the affinity values as per by the MPIDR register format
     *    Bits 63:40 - should be zero
     *    Bits 39:32 - Affinity level 3
     *    Bits 31:24 - should be zero
     *    Bits 23:16 - Affinity level 2
     *    Bits 15:8  - Affinity level 1
     *    Bits 7:0   - Affinity level 0
     */
    uint64_t *isolated_cpu_mpid_list;
};

/*!
 * \brief Module configuration.
 */
struct mod_si0_platform_config {
    /*! MPID number of the CPU to be used as primary CPU */
    uint64_t primary_cpu_mpid;

    /*! List of isolated CPUs MPID. */
    struct mod_si0_platform_isolated_cpu_info isolated_cpu_info;
};

/*!
 * \brief Event to check all CPUs are powered off.
 */
static const fwk_id_t mod_platform_system_event_check_ppu_off =
    FWK_ID_EVENT(FWK_MODULE_IDX_SI0_PLATFORM, MOD_SI0_PLATFORM_CHECK_PD_OFF);

/*!
 * @}
 */

/*!
 * @}
 */

#endif /* MOD_SI0_PLATFORM_H */
