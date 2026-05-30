/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Platform SMCF module support
 */

#ifndef MOD_PLATFORM_SMCF_H
#define MOD_PLATFORM_SMCF_H

/*!
 * \brief Indices of the interfaces exposed by the module.
 */
enum mod_platform_smcf_api_idx {
    /*! API index for the data api of the SMCF module */
    MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API,

    /*! Number of exposed interfaces */
    MOD_SMCF_PLATFORM_API_COUNT
};

#endif /* MOD_PLATFORM_SMCF_H */
