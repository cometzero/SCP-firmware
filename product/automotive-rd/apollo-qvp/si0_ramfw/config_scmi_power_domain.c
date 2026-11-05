/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'scmi_power_domain'.
 */

#include "platform_core.h"
#include "si0_cfgd_scmi.h"

#include <mod_power_domain.h>
#include <mod_scmi_power_domain.h>
#include <mod_si0_platform.h>

#include <fwk_module.h>
#include <fwk_status.h>

/* No elements, no module configuration data */
struct fwk_module_config config_scmi_power_domain = { 0 };

int scmi_pd_prepare_async_response(
    uint32_t state, unsigned int agent_id, fwk_id_t pd_id)
{
    unsigned int index = fwk_id_get_element_idx(pd_id);

    if (agent_id == SI0_SCMI_AGENT_IDX_PSCI &&
        index < platform_get_core_count() &&
        state == MOD_PD_COMPOSITE_STATE(
            MOD_PD_LEVEL_0, 0, 0, 0, MOD_PD_STATE_OFF)) {
        /* Core requests are asynchronous. Arm OFF_LOCK before the immediate
         * SCMI response permits TF-A to execute its terminal WFI. Enqueue has
         * already succeeded; the PD event later observes the completion.
         * Cluster/system OFF and non-PSCI requests retain their normal policy. */
        return si0_prepare_core_off(index);
    }
    return FWK_SUCCESS;
}
