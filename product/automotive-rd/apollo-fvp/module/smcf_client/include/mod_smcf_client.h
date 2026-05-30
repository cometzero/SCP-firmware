/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     SMCF Client Support
 */

#ifndef MOD_SMCF_CLIENT_H
#define MOD_SMCF_CLIENT_H

#include <fwk_id.h>

#include <stdint.h>

/*!
 * \brief Indices of the interfaces exposed by the module.
 */
enum mod_smcf_client_api_idx {
    MOD_SMCF_CLIENT_API_IDX_CONTROL,
    MOD_SMCF_CLIENT_API_COUNT,
};

/*!
 * \brief Indices of the Client monitor type.
 */
enum mod_smcf_client_monitor_type_idx {
    MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_AMU,
    MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_SENSOR,
    MOD_SMCF_CLIENT_MONITOR_TYPE_COUNT,
};

/*!
 *  \brief Control API
 */
struct mod_smcf_client_control_api {
    /*! Function to start sampling on all MGIs */
    int (*start_sampling_all_mgis)(void);
    /*! Function to stop sampling on all MGIs */
    int (*stop_sampling_all_mgis)(void);
    /*! Function to toggle the print flag, required for integration test */
    int (*toggle_print)(void);
};

/*!
 * \brief Element = MGI
 */
struct mod_smcf_client_mgi_conf {
    /*! Element ID in smcf module */
    fwk_id_t smcf_mgi_id;
    /*! Array of MLIs */
    struct mod_smcf_client_mli_conf *mlis;
};

/*!
 * \brief Sub-Element = MLI
 */
struct mod_smcf_client_mli_conf {
    enum mod_smcf_client_monitor_type_idx type;
};

#endif /* MOD_SMCF_CLIENT_H */
