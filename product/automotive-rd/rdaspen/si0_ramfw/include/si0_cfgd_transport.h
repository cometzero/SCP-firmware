/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Definitions for transport module configuration data in SCP
 *     firmware.
 */

#ifndef SI0_CFGD_TRANSPORT_H
#define SI0_CFGD_TRANSPORT_H

/* Module 'transport' element indexes */
enum scp_cfgd_mod_transport_element_idx {
    SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_RSE,
    SI0_CFGD_MOD_TRANSPORT_EIDX_PSCI,
#ifdef BUILD_HAS_SCMI_NOTIFICATIONS
    SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_RSE_P2A, /* SCP to RSE notification */
#endif
    SI0_CFGD_MOD_TRANSPORT_EIDX_RAS,
    SI0_CFGD_MOD_TRANSPORT_EIDX_COUNT,
};

#endif /* SI0_CFGD_TRANSPORT_H */
