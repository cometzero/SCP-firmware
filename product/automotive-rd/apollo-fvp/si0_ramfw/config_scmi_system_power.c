/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'scmi_system_power'.
 */

#include "si0_cfgd_timer.h"

#include <internal/scmi_system_power.h>

#include <mod_scmi_system_power.h>
#include <mod_system_power.h>

#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

const struct fwk_module_config config_scmi_system_power = {
    .data = &((struct mod_scmi_system_power_config){
        .system_view = MOD_SCMI_SYSTEM_VIEW_FULL,
#if APOLLO_FVP_TEST_SUSPEND_WAKE_US > 0
        .system_suspend_state = MOD_SYSTEM_POWER_POWER_STATE_SLEEP0,
        .disable_system_suspend = false,
#else
        .system_suspend_state = MOD_PD_STATE_OFF,
        /* SYSTOP remains ON-only until wake and context restore are ready. */
        .disable_system_suspend = true,
#endif
#ifdef BUILD_HAS_SCMI_NOTIFICATIONS
        .alarm_id = FWK_ID_SUB_ELEMENT_INIT(
            FWK_MODULE_IDX_TIMER,
            SI0_SI0_TIMER_ALARM_ELEMENT_IDX,
            SI0_CFGD_SCMI_NOTIFICATION_ALARM_IDX),
        .graceful_timeout = 1000000, /* us */
#endif
    }),
};

int scmi_sys_power_state_set_policy(
    enum mod_scmi_sys_power_policy_status *policy_status,
    const uint32_t *state,
    fwk_id_t service_id,
    bool graceful)
{
    /*
     * PSCI has already quiesced the AP before requesting system suspend.
     * The notification-only graceful path never executes SUSPEND: its
     * timeout callback handles SHUTDOWN only. Dispatch suspend to the power
     * domain handler so an unsupported state is rejected, not acknowledged
     * while the AP remains asleep with no power transition.
     */
    if (graceful && (*state != SCMI_SYSTEM_STATE_WARM_RESET) &&
        (*state != SCMI_SYSTEM_STATE_SUSPEND)) {
        *policy_status = MOD_SCMI_SYS_POWER_SKIP_MESSAGE_HANDLER;
    } else {
        *policy_status = MOD_SCMI_SYS_POWER_EXECUTE_MESSAGE_HANDLER;
    }
#ifdef BUILD_HAS_SCMI_NOTIFICATIONS
    FWK_LOG_INFO(
        "[SI0 PLATFORM][SCMI] System state %u: %s",
        (unsigned int)*state,
        (*policy_status == MOD_SCMI_SYS_POWER_SKIP_MESSAGE_HANDLER) ?
            "notification request" : "execute handler");
#endif
    return FWK_SUCCESS;
}
