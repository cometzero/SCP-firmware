/* SPDX-License-Identifier: BSD-3-Clause */
#include <assert.h>
#include <stdio.h>
#include "../../si0_platform/src/mod_si0_platform.c"
static uint64_t ticks;
static unsigned int requests, reloads, irq_enables, completions;
static bool cores_off, fail_state, retrigger_on_enable;
static struct fwk_event queued;
static unsigned int alarm_starts;
static uint64_t alarm_due;
int __fwk_put_event(struct fwk_event *event) { queued = *event; return 0; }
int __fwk_put_event_light(struct fwk_event_light *event)
{
    queued = (struct fwk_event) {
        .id = event->id, .source_id = event->source_id, .target_id = event->target_id
    };
    return 0;
}
const void *fwk_module_get_data(fwk_id_t id)
{ (void)id; assert(false); return NULL; }
int fwk_module_get_element_count(fwk_id_t id, size_t *count)
{ (void)id; (void)count; assert(false); return FWK_E_STATE; }
int fwk_interrupt_disable(unsigned int irq) { (void)irq; return 0; }
int fwk_interrupt_enable(unsigned int irq)
{
    (void)irq; irq_enables++;
    if (retrigger_on_enable) { ap_watchdog_isr(); }
    return 0;
}
int fwk_interrupt_clear_pending(unsigned int irq) { (void)irq; return 0; }
int fwk_interrupt_is_pending(unsigned int irq, bool *pending)
{ (void)irq; *pending = false; return 0; }
int fwk_interrupt_is_enabled(unsigned int irq, bool *enabled)
{ (void)irq; *enabled = true; return 0; }
const char *fwk_module_get_element_name(fwk_id_t id) { (void)id; return "cpu"; }
void fwk_log_printf(const char *format, ...) { (void)format; }
int start_rse_recovery(void) { reloads++; return 0; }
void poll_rse_recovery(unsigned int generation) { (void)generation; }
int complete_rse_recovery(const struct fwk_event *event)
{ (void)event; completions++; return 0; }
int notify_rse_and_wait_for_response(bool shutdown) { (void)shutdown; return 0; }
const void *get_platform_scmi_power_down_api(void) { return NULL; }
const void *get_platform_system_power_driver_api(void) { return NULL; }
const void *get_rse_platform_transport_signal_api(void) { return NULL; }
static int counter(fwk_id_t id, uint64_t *value) { (void)id; *value = ticks; return 0; }
static int duration(fwk_id_t id, unsigned int us, uint64_t *value)
{ (void)id; *value = us; return 0; }
static int alarm_start(fwk_id_t id, unsigned int us, enum mod_timer_alarm_type type,
    void (*callback)(uintptr_t), uintptr_t arg)
{
    (void)id; (void)callback; (void)arg;
    assert(type == MOD_TIMER_ALARM_TYPE_ONCE && us == 10000);
    alarm_starts++; alarm_due = ticks + us; return 0;
}
static int stop(fwk_id_t id) { (void)id; return 0; }
static int set_state(fwk_id_t id, bool response, unsigned int state)
{ (void)id; (void)response; (void)state; requests++; return fail_state ? FWK_E_DEVICE : 0; }
static int get_state(fwk_id_t id, unsigned int *state)
{ (void)id; *state = cores_off ? MOD_PD_STATE_OFF : MOD_PD_STATE_ON; return 0; }
static const struct mod_timer_api timer = { .get_counter = counter, .time_to_timestamp = duration };
static const struct mod_timer_alarm_api alarms = { .start = alarm_start, .stop = stop };
static struct mod_pd_restricted_api pd = { .get_state = get_state, .set_state = set_state };
int main(void)
{
    const void *api;
    struct mod_si0_platform_config config = { .ap_watchdog_irq = 321 };
    si0_platform_ctx.config = &config;
    si0_platform_ctx.rearm_timer_api = &timer;
    si0_platform_ctx.mod_pd_restricted_api = &pd;
    control.alarm = &alarms;
    assert(si0_platform_process_bind_request(FWK_ID_MODULE(FWK_MODULE_IDX_TIMER),
        FWK_ID_MODULE(FWK_MODULE_IDX_SI0_PLATFORM),
        FWK_ID_API(FWK_MODULE_IDX_SI0_PLATFORM, MOD_SI0_PLATFORM_API_IDX_VMCU_CONTROL), &api) == FWK_E_ACCESS);
    assert(si0_platform_process_bind_request(FWK_ID_MODULE(FWK_MODULE_IDX_VMCU_SAFETY),
        FWK_ID_MODULE(FWK_MODULE_IDX_SI0_PLATFORM),
        FWK_ID_API(FWK_MODULE_IDX_SI0_PLATFORM, MOD_SI0_PLATFORM_API_IDX_VMCU_CONTROL), &api) == 0);
    assert(api == &vmcu_control_api);
    assert(si0_vmcu_accept_shutdown() == FWK_E_STATE);
    assert(wake_ap() == FWK_E_STATE);
    assert(power_arm(true) == 0 && ap_power_state == SI0_AP_ARMED);
    assert(request_recovery() == FWK_E_BUSY);
    assert(power_arm(false) == 0 && ap_power_state == SI0_AP_RUN);
    assert(power_arm(true) == 0);
    assert(si0_vmcu_accept_shutdown() == 0 && requests == platform_get_core_count());
    assert(ap_power_state == SI0_AP_QUIESCING);
    assert(control_poll(control.generation) == 0 && ap_power_state == SI0_AP_QUIESCING);
    cores_off = true;
    assert(control_poll(control.generation) == 0 && ap_power_state == SI0_AP_OFF);
    assert(!control.active && recovery_state == SI0_RECOVERY_OFF);
    assert(wake_ap() == 0 && recovery_state == SI0_RECOVERY_PENDING);
    assert(fwk_id_get_event_idx(queued.id) == MOD_SI0_PLATFORM_AP_WATCHDOG);
    assert(request_recovery() == FWK_E_BUSY);
    assert(control_start(30000000) == 0);
    assert(control_poll(control.generation) == 0 && reloads == 1);
    assert(recovery_state == SI0_RECOVERY_RELOAD);
    ticks = control.deadline;
    assert(control_poll(control.generation) == 0);
    assert(ap_power_state == SI0_AP_FAILED && recovery_state == SI0_RECOVERY_FAILED);
    ap_watchdog_recovery = false; ap_power_state = SI0_AP_RUN;
    assert(power_arm(true) == 0);
    fail_state = true;
    assert(si0_vmcu_accept_shutdown() == FWK_E_DEVICE);
    assert(ap_power_state == SI0_AP_FAILED && !control.active);
    /* A late, successful RSE completion must be consumed without releasing AP. */
    struct fwk_event done = { .id = FWK_ID_EVENT_INIT(FWK_MODULE_IDX_SI0_PLATFORM,
        MOD_SI0_PLATFORM_RSE_RECOVERY_DONE) };
    unsigned int saved_requests = requests;
    assert(si0_platform_process_event(&done, NULL) == 0 && completions == 1);
    assert(ap_power_state == SI0_AP_FAILED && recovery_state == SI0_RECOVERY_FAILED);
    assert(requests == saved_requests && cores_off);
    /* Check the deadline even if its timer event has not yet been processed. */
    assert(control_start(30000000) == 0);
    recovery_state = SI0_RECOVERY_RELOAD; ap_power_state = SI0_AP_WAKING;
    ticks = control.deadline;
    assert(si0_platform_process_event(&done, NULL) == 0 && completions == 2);
    assert(ap_power_state == SI0_AP_FAILED && !control.active);
    assert(requests == saved_requests && cores_off);
    /* A queued or newly requested rearm cannot turn a timed-out transaction
     * into COMPLETE or unmask WS1; a normal BOOT rearm still completes. */
    si0_platform_ctx.rearm_alarm_api = &alarms;
    assert(control_start(30000000) == 0);
    recovery_state = SI0_RECOVERY_BOOT; ap_power_state = SI0_AP_WAKING;
    rearm_ctx.pending = true; rearm_ctx.generation++;
    rearm_ctx.deadline = control.deadline + 100000;
    unsigned int old_generation = rearm_ctx.generation;
    unsigned int saved_enables = irq_enables;
    ticks = control.deadline;
    assert(watchdog_rearm_poll(old_generation) == 0);
    assert(ap_power_state == SI0_AP_FAILED && !rearm_ctx.pending);
    assert(watchdog_rearm_poll(old_generation) == 0);
    assert(start_watchdog_rearm() == 0 && !rearm_ctx.pending);
    assert(irq_enables == saved_enables && cores_off);
    assert(control_start(30000000) == 0);
    recovery_state = SI0_RECOVERY_BOOT; ap_power_state = SI0_AP_WAKING;
    ap_watchdog_recovery = true;
    config.watchdog_rearm_timeout_us = 100000;
    assert(start_watchdog_rearm() == 0);
    assert(recovery_state == SI0_RECOVERY_COMPLETE && ap_power_state == SI0_AP_RUN);
    assert(!control.active && irq_enables == saved_enables + 1);
    assert(control_start(30000000) == 0);
    recovery_state = SI0_RECOVERY_BOOT; ap_power_state = SI0_AP_WAKING;
    ap_watchdog_recovery = true; retrigger_on_enable = true;
    assert(start_watchdog_rearm() == 0);
    assert(recovery_state == SI0_RECOVERY_PENDING && ap_power_state == SI0_AP_WAKING);
    assert(ap_watchdog_recovery && control.active);
    retrigger_on_enable = false;
    control_stop_alarm(); control.active = false; ap_watchdog_recovery = false;
    ap_power_state = SI0_AP_RUN;
    assert(power_arm(true) == 0);
    unsigned int before = alarm_starts;
    ticks += 5000000; control_callback(control.generation);
    assert(!control.alarm_armed && alarm_starts == before);
    ticks += 5000000;
    assert(si0_platform_process_event(&queued, NULL) == 0);
    assert(alarm_starts == before + 1 && alarm_due == ticks + 10000);
    assert(control.callback_time == ticks - 5000000);
    /* Actual cores OFF cannot turn an already expired deadline into success. */
    control.stopping_ap = true; ap_power_state = SI0_AP_QUIESCING;
    ticks = control.deadline;
    control_callback(control.generation);
    before = alarm_starts;
    assert(si0_platform_process_event(&queued, NULL) == 0);
    assert(ap_power_state == SI0_AP_FAILED && !control.active && cores_off);
    assert(alarm_starts == before);
    puts("SI control PASS: caller gate, arm/cancel, verified AP OFF, wake/recovery, bounded failure, late completion/rearm rejection");
    return 0;
}
