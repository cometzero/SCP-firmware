/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MOD_FMU_H
#define MOD_FMU_H

#include <fwk_id.h>
#include <fwk_module_idx.h>

#include <stdint.h>

/*!
 * \ingroup GroupModules
 * \defgroup GroupFMU Fault Management Unit
 * \{
 */

/*!
 * \brief A description of a fault.
 *
 * A fault is described by its device index, "node" index and a safety mechanism
 * ID
 */
struct mod_fmu_fault {
    /*! Device index, an index into the element array */
    uint8_t device_idx;

    /*! Sub-node index, the meaning of which is FMU-specific */
    uint16_t node_idx;

    /*! Safety mechanism index, the possible values of which are FMU-specific */
    uint8_t sm_idx;

    /*! GIC/MHU block instance within node_idx (BLKTYPE); zero for other FMUs. */
    uint16_t block_id;

    /*! GIC/MHU ERRSTATUS.INJ: true for software insertion, false for detectors.
     * System/NI have no origin flag; false there means unavailable. */
    bool injected;
};

struct mod_fmu_drain_status {
    bool masked;
    int last_error;
    unsigned int drained;
};

/*!
 * \brief Module API indices.
 */
enum mod_fmu_api_idx { MOD_FMU_DEVICE_API_IDX, MOD_FMU_API_COUNT };

/*!
 * \brief Module API definition.
 */
struct mod_fmu_api {
    /*! Inject a fault */
    int (*inject)(const struct mod_fmu_fault *fault);

    /*! Get the enabled status for the given id and node_id */
    int (*get_enabled)(const struct mod_fmu_fault *fault, bool *enabled);

    /*! Enable or disable fault reporting for the given id and node_id */
    int (*set_enabled)(const struct mod_fmu_fault *fault, bool enabled);

    /*! Get the current fault count for the given id and node_id */
    int (*get_count)(fwk_id_t id, uint16_t node_id, uint8_t *count);

    /*! Set the current fault count for the given id and node_id */
    int (*set_count)(fwk_id_t id, uint16_t node_id, uint8_t count);

    /*! Get the fault upgrade threshold for the given id and node_id */
    int (*get_threshold)(fwk_id_t id, uint16_t node_id, uint8_t *threshold);

    /*! Set the fault upgrade threshold for the given id and node_id */
    int (*set_threshold)(fwk_id_t id, uint16_t node_id, uint8_t threshold);

    /* Get the status of the fault upgrade feature */
    int (*get_upgrade_enabled)(fwk_id_t id, uint16_t node_id, bool *enabled);

    /*! Enable or disable whether a fault is upgraded to critical when the
     * threshold is reached */
    int (*set_upgrade_enabled)(fwk_id_t id, uint16_t node_id, bool enabled);

    /*! Configure the criticality of a safety mechanism */
    int (*set_critical)(const struct mod_fmu_fault *fault, bool critical);

    /*! Retry a failed drain after the platform has repaired the fault source. */
    int (*recover)(bool critical);

    int (*get_drain_status)(bool critical, struct mod_fmu_drain_status *status);
};

#ifdef BUILD_HAS_NOTIFICATION
/*!
 * \brief Parameters of an FMU module event.
 */
struct mod_fmu_fault_notification_params {
    /*! Criticality */
    bool critical;

    /*! Fault details */
    struct mod_fmu_fault fault;
};
#endif

/*!
 * \brief FMU module configuration
 */
struct mod_fmu_config {
    uint32_t irq_critical;
    uint32_t irq_non_critical;
    fwk_id_t timer_id;
};

/*!
 * \brief FMU implementation types
 */
enum mod_fmu_implementation {
    MOD_FMU_SYSTEM_IMPL,
    MOD_FMU_GIC_MHU_IMPL,
    MOD_FMU_NI710AE_IMPL,
    MOD_FMU_IMPL_COUNT,
};

/*!
 * \brief FMU device configuration
 */
#define MOD_FMU_HAS_PARENT_SOURCES 1

enum mod_fmu_parent_source {
    MOD_FMU_SOURCE_ERI,
    MOD_FMU_SOURCE_FHI,
    MOD_FMU_SOURCE_CRI,
    MOD_FMU_SOURCE_COUNT,
};

struct mod_fmu_dev_config {
    /*! Base address */
    uintptr_t base;

    /*! Element index of the FMU's parent */
    uint32_t parent;

    enum mod_fmu_implementation implementation;

    /*! Node index to which the critical fault signal is connected */
    uint32_t parent_cr_index;

    /*! Node index to which the non-critical fault signal is connected */
    uint32_t parent_ncr_index;

    /*! NI has independent ERI/FHI/CRI outputs, not two severity outputs. */
    bool has_parent_sources;
    uint32_t parent_source[MOD_FMU_SOURCE_COUNT];
};

/**
 * @brief A constant to indicate that the FMU does not have a parent (root FMU).
 */
#define MOD_FMU_PARENT_NONE UINT32_MAX

/**
 * A constant to indicate that a function call targets all supported safety
 * mechanisms
 */
#define MOD_FMU_SM_ALL UINT8_MAX

/*!
 * \brief FMU notification indices.
 */
enum mod_scmi_notification_idx {
    /*! The SCMI service has been initialized */
    MOD_FMU_NOTIFICATION_IDX_FAULT,

    /*! Number of defined notifications */
    MOD_FMU_NOTIFICATION_IDX_COUNT
};

/*!
 * \brief Identifier for the MOD_SCMI_NOTIFICATION_IDX_INITIALIZED
 *     notification.
 */
static const fwk_id_t mod_fmu_notification_id_fault = FWK_ID_NOTIFICATION_INIT(
    FWK_MODULE_IDX_FMU,
    MOD_FMU_NOTIFICATION_IDX_FAULT);

/*!
 * \brief Constants for System FMU safety mechanisms
 */
#define MOD_FMU_SM_SYSTEM_INPUT_ERROR        FWK_BIT(4)
#define MOD_FMU_SM_SYSTEM_INPUT_PARITY_ERROR FWK_BIT(3)

#endif
