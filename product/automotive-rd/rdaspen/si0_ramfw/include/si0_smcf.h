/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Definitions for SMCF-related configuration.
 */
#ifndef SI0_SMCF_H
#define SI0_SMCF_H

#include <fwk_id.h>

#include <stdint.h>

/*!
 * \brief Indices of the MGIs (elements)
 */
enum si0_smcf_client_mgi_idx {
    SI0_SMCF_MGI_IDX_SMD_SMCF_EXPANSION_MGI,
    SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_0,
    SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_1,
    SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_2,
    SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_3,
    SI0_SMCF_MGI_IDX_COUNT
};

/*!
 * \brief Indices of the MLIs (sub-elements) connected to AP Cluster MGI
 *        Assumption: Each AP Cluster MGI has one MLI
 */
enum si0_smcf_client_ap_cluster_mgi_mli_idx {
    SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_0,
    SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_COUNT,
};

/*!
 * \brief Indices of the MLIs (sub-elements) connected to SMD SMCF Expansion
 *        MGI
 */
enum si0_smcf_client_smd_smcf_expansion_mgi_mli_idx {
    SI0_SMCF_SMD_SMCF_EXPANSION_MGI_MLI_IDX_0,
    SI0_SMCF_SMD_SMCF_EXPANSION_MGI_MLI_IDX_COUNT,
};

/*!
 * \brief Indices connected to all MGIs
 */
enum si0_smcf_mli_idx {
    SI0_SMCF_MLI_IDX_SMD_SENSOR_0,
    SI0_SMCF_MLI_IDX_AP_CLUSTER_0_AMU_0,
    SI0_SMCF_MLI_IDX_AP_CLUSTER_1_AMU_0,
    SI0_SMCF_MLI_IDX_AP_CLUSTER_2_AMU_0,
    SI0_SMCF_MLI_IDX_AP_CLUSTER_3_AMU_0,
    SI0_SMCF_MLI_IDX_COUNT,
};

/*!
 * \brief Number of counter values per AMU
 */
#define NUM_OF_AMU_COUNTERS UINT8_C(12U)

/*!
 * \brief Number of data values per sensor
 */
#define NUM_OF_SENSOR_VALUES UINT8_C(12U)

#endif /* SI0_SMCF_H */
