/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Definitions for MHUv3 module configuration data in SCP firmware.
 */

#ifndef SI0_CFGD_MHU3_H
#define SI0_CFGD_MHU3_H

/* MHUv3 device indices */
enum scp_cfgd_mod_mhu3_device_idx {
    SI0_CFGD_MOD_MHU3_EIDX_SI0_RSE,
    SI0_CFGD_MOD_MHU3_EIDX_SI0_AP_S,
    SI0_CFGD_MOD_MHU3_EIDX_SI0_AP_S_RAS,
    SI0_CFGD_MOD_MHU3_EIDX_COUNT
};

#endif /* SI0_CFGD_MHU3_H */
