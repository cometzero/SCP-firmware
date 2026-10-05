/* SPDX-License-Identifier: BSD-3-Clause */
#include <assert.h>
#include <stdio.h>
#include "../../pfdi_monitor/src/mod_pfdi_monitor.c"
static struct fwk_event queued;
int __fwk_put_event(struct fwk_event *event) { queued = *event; return 0; }
const char *fwk_module_get_element_name(fwk_id_t id) { (void)id; return "test"; }
static int stop(fwk_id_t id) { (void)id; return 0; }
static int alarm_start(fwk_id_t id, unsigned int us, enum mod_timer_alarm_type type,
    void (*callback)(uintptr_t), uintptr_t arg)
{ (void)id; (void)us; (void)type; (void)callback; (void)arg; return 0; }
static const struct mod_timer_alarm_api alarms = { .start = alarm_start, .stop = stop };
void fwk_log_printf(const char *format, ...) { (void)format; }
int main(void)
{
    struct mod_pfdi_monitor_core_config config = { .pd_source_id = FWK_ID_NONE_INIT };
    struct pfdi_monitor_core_context core = { .core_cfg = &config, .alarm_api = &alarms };
    struct mod_pfdi_monitor_status state;
    fwk_id_t id = FWK_ID_ELEMENT(FWK_MODULE_IDX_PFDI_MONITOR, 0);
    ctx.core_ctx_table = &core; ctx.core_count = 1;
    assert(pfdi_monitor_get_status(id, &state) == 0 && !state.online && !state.faults);
    assert(pfdi_monitor_oor_status(id, 0x100) == 0);
    assert(((struct pfdi_monitor_event_params *)queued.params)->status == 0x100);
    assert(pfdi_monitor_process_event(&queued, NULL) == 0);
    assert(pfdi_monitor_get_status(id, &state) == 0);
    assert(state.last_status == 0x100 && state.faults == MOD_PFDI_FAULT_OOR);
    core.powered_off = true;
    assert(pfdi_monitor_prepare_restart(config.pd_source_id) == 0);
    assert(core.generation == 1 && core.faults == 0);
    core.powered_off = false;
    assert(pfdi_monitor_oor_status(id, 0) == 0);
    assert(pfdi_monitor_process_event(&queued, NULL) == 0);
    assert(pfdi_monitor_onl_status(id, 0) == 0);
    assert(pfdi_monitor_process_event(&queued, NULL) == 0);
    assert(pfdi_monitor_get_status(id, &state) == 0 && state.online);
    /* Hotplug preserves the PFDI state machine, but online evidence must be
     * renewed after the power transition before the safety snapshot is READY. */
    struct fwk_event power = { .id = pd_transition_notification_id,
        .target_id = id };
    struct mod_pd_power_state_transition_notification_params *params =
        (void *)power.params;
    params->state = MOD_PD_STATE_OFF;
    assert(pfdi_monitor_process_notificiation(&power, NULL) == 0);
    assert(pfdi_monitor_get_status(id, &state) == 0 && !state.online && state.powered_off);
    params->state = MOD_PD_STATE_ON;
    assert(pfdi_monitor_process_notificiation(&power, NULL) == 0);
    assert(pfdi_monitor_get_status(id, &state) == 0 && !state.online && !state.powered_off);
    assert(core.core_state == PFDI_MONITOR_STATE_WAIT_FOR_ONL);
    assert(pfdi_monitor_onl_status(id, 0) == 0);
    assert(pfdi_monitor_process_event(&queued, NULL) == 0);
    assert(pfdi_monitor_get_status(id, &state) == 0 && state.online && !state.faults);
    pfdi_monitor_timeout(0);
    assert(pfdi_monitor_process_event(&queued, NULL) == 0);
    assert(core.faults == MOD_PFDI_FAULT_TIMEOUT);
    assert(pfdi_monitor_onl_status(id, 0) == 0);
    assert(pfdi_monitor_process_event(&queued, NULL) == 0);
    assert(core.faults == MOD_PFDI_FAULT_TIMEOUT); /* Success cannot erase history. */
    core.powered_off = true;
    assert(pfdi_monitor_get_status(id, &state) == 0 && !state.online && state.faults);
    assert(pfdi_monitor_prepare_restart(config.pd_source_id) == 0);
    assert(pfdi_monitor_process_event(&queued, NULL) == 0); /* stale generation */
    assert(core.core_state == PFDI_MONITOR_STATE_WAIT_FOR_OOR && core.faults == 0);
    assert(pfdi_monitor_get_status(FWK_ID_ELEMENT(FWK_MODULE_IDX_PFDI_MONITOR, 1), &state) == FWK_E_PARAM);
    puts("PFDI snapshot PASS: 32-bit status, state, timeout latch, off/on freshness, restart, stale epoch");
    return 0;
}
