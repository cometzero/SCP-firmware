/* SPDX-License-Identifier: BSD-3-Clause */
#include "si0_cfgd_timer.h"
#include "si0_cfgd_pfdi_monitor.h"
#include "si0_mmap.h"
#include <mod_vmcu_safety.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
static const struct mod_vmcu_safety_config cfg = {
    .uart_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PL011, 1),
    .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
    .alarm_id = FWK_ID_SUB_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0,
        SI0_CFGD_VMCU_SAFETY_ALARM_IDX),
    .first_ap = SI0_CFGD_MOD_PFDI_MONITOR_EIDX_AP_CLUSTER_0_CORE_0,
    .ap_count = PC_CONFIGURED_CORES_COUNT,
    .gpio_base = SI0_VMCU_GPIO_BASE,
};
const struct fwk_module_config config_vmcu_safety = { .data = &cfg };
