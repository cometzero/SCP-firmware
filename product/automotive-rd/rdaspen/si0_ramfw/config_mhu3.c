/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'mhu3'.
 */

#include "si0_cfgd_mhu3.h"
#include "si0_irq.h"
#include "si0_mmap.h"

#include <mod_mhu3.h>

#include <fwk_element.h>
#include <fwk_id.h>
#include <fwk_module.h>

/*
 * Timeout to wait for the receiver to clear the MHUv3 channel status
 * register so the channel can become available again.
 */
#define RESP_WAIT_TIMEOUT_US (30 * 1000)

/* SI0<-->RSE Secure MHUv3 Doorbell channel configuration */
struct mod_mhu3_channel_config si02rse_s_dbch_config[] = {
    /* PBX CH 0, FLAG 0, MBX CH 0, FLAG 0 not used */
    /* PBX CH 0, FLAG 1, MBX CH 0, FLAG 1 for RSE A2P SCP */
    [0] = MOD_MHU3_INIT_DBCH(0, 1, 0, 1),
#ifdef BUILD_HAS_SCMI_NOTIFICATIONS
    /* PBX CH 0, FLAG 2, MBX CH 0, FLAG 2 for SCP P2A RSE */
    [1] = MOD_MHU3_INIT_DBCH(0, 2, 0, 2),
#endif
};

/* SI0<->AP Secure MHUv3 doorbell channel configuration */
struct mod_mhu3_channel_config si02ap_s_dbch_config[] = {
    /* PBX CH 0, FLAG 0, MBX CH 0, FLAG 0 */
    [0] = MOD_MHU3_INIT_DBCH(0, 0, 0, 0),
};

/* SI0<->AP RAS Sync Secure MHUv3 doorbell channel configuration */
struct mod_mhu3_channel_config si02ap_s_ras_dbch_config[] = {
    /* PBX CH 0, FLAG 0, MBX CH 0, FLAG 0 is used for RAS sync */
    [0] = MOD_MHU3_INIT_DBCH(0, 1, 0, 1),
};

/* Module element table */
static const struct fwk_element mhu_element_table[]  = {
    [SI0_CFGD_MOD_MHU3_EIDX_SI0_RSE] = {
        .name = "SI02RSE_S_MHU_DBCH",
        .sub_element_count = FWK_ARRAY_SIZE(si02rse_s_dbch_config),
        .data = &(struct mod_mhu3_device_config) {
            .irq = (unsigned int) CL0_MHU3_RSE2SI0_IRQ,
            .in = SI0_RSE2SI0_MHUV3_RCV_BASE,
            .out = SI0_SI02RSE_MHUV3_SEND_BASE,
            .channels = si02rse_s_dbch_config,
            .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
            .resp_wait_timeout_us = RESP_WAIT_TIMEOUT_US,
        },
    },
    [SI0_CFGD_MOD_MHU3_EIDX_SI0_AP_S] = {
        .name = "SI02AP_S_MHU_DBCH",
        .sub_element_count = FWK_ARRAY_SIZE(si02ap_s_dbch_config),
        .data = &(struct mod_mhu3_device_config) {
            .irq = (unsigned int) CL0_MHU3_AP2SI0_S_IRQ,
            .in = SI0_AP2SI0_S_MHUV3_RCV_BASE,
            .out = SI0_SI02AP_S_MHUV3_SEND_BASE,
            .channels = si02ap_s_dbch_config,
            .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
            .resp_wait_timeout_us = RESP_WAIT_TIMEOUT_US,
        },
    },
    [SI0_CFGD_MOD_MHU3_EIDX_SI0_AP_S_RAS] = {
        .name = "SI02AP_RAS_S_MHU_DBCH",
        .sub_element_count = FWK_ARRAY_SIZE(si02ap_s_ras_dbch_config),
        .data = &(struct mod_mhu3_device_config) {
            .irq = (unsigned int) CL0_MHU3_AP2SI0_NS_IRQ,
            .in = SI0_AP2SI0_NS_MHUV3_RCV_BASE,
            .out = SI0_SI02AP_NS_MHUV3_SEND_BASE,
            .channels = si02ap_s_ras_dbch_config,
            .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
            .resp_wait_timeout_us = RESP_WAIT_TIMEOUT_US,
        },
    },
    [SI0_CFGD_MOD_MHU3_EIDX_COUNT] = { 0 },
};

struct fwk_module_config config_mhu3 = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(mhu_element_table),
};
