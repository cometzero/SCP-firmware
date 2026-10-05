/* SPDX-License-Identifier: BSD-3-Clause */
#include <assert.h>
#include <stdio.h>
#include "../../si0_platform/src/platform_rse.c"
static uint64_t ticks, alarm_due;
static unsigned int starts, events, stops;
static bool armed, fail_queue, fail_schedule;
static struct fwk_event queued;
int __fwk_put_event(struct fwk_event *event)
{
    if (fail_queue) return FWK_E_NOMEM;
    queued = *event; events++; return 0;
}
void fwk_log_printf(const char *format, ...) { (void)format; }
static int counter(fwk_id_t id, uint64_t *value)
{ (void)id; *value = ticks; return 0; }
static int duration(fwk_id_t id, uint32_t us, uint64_t *value)
{ (void)id; *value = us; return 0; }
static int schedule(fwk_id_t id, unsigned int us, enum mod_timer_alarm_type type,
    void (*callback)(uintptr_t), uintptr_t arg)
{
    (void)id; (void)arg;
    assert(type == MOD_TIMER_ALARM_TYPE_ONCE && us == 10000 && callback == recovery_alarm);
    if (fail_schedule) return FWK_E_DEVICE;
    starts++; armed = true; alarm_due = ticks + us; return 0;
}
static int stop(fwk_id_t id)
{ (void)id; assert(armed); armed = false; stops++; return 0; }
static int notify(void) { return 0; }
static const struct mod_timer_api timer = { .get_counter = counter, .time_to_timestamp = duration };
static const struct mod_timer_alarm_api alarms = { .start = schedule, .stop = stop };
static const struct mod_scmi_system_power_platform_api power = { .notify_warm_reset = notify };
static void fire(void)
{ assert(armed); armed = false; recovery_alarm(ctx.generation); }
int main(void)
{
    struct mod_si0_platform_config config = { .rse_recovery_timeout_us = 10000000 };
    ctx.config = &config; ctx.timer_api = &timer; ctx.alarm_api = &alarms; ctx.sys_power_api = &power;
    assert(start_rse_recovery() == 0 && starts == 1);
    ticks = 4000000; fire();
    assert(events == 1 && starts == 1 && !armed);
    assert(fwk_id_get_event_idx(queued.id) == MOD_SI0_PLATFORM_RSE_RECOVERY_POLL);
    ticks = 5000000; poll_rse_recovery(*(unsigned int *)queued.params);
    assert(starts == 2 && armed && alarm_due == 5010000 && events == 1);
    ticks = 6000000; fire();
    ctx.rse_doorbell_received = true; ctx.ack_counter = ticks;
    poll_rse_recovery(ctx.generation);
    assert(events == 3 && !armed && stops == 0);
    assert(fwk_id_get_event_idx(queued.id) == MOD_SI0_PLATFORM_RSE_RECOVERY_DONE);
    assert(complete_rse_recovery(&queued) == 0);
    unsigned int old_generation = ctx.generation;
    assert(start_rse_recovery() == 0);
    unsigned int before = starts;
    poll_rse_recovery(old_generation);
    assert(starts == before && ctx.recovery_pending);
    ticks = ctx.deadline + 1; fire();
    ctx.rse_doorbell_received = true; ctx.ack_counter = ticks;
    poll_rse_recovery(ctx.generation);
    assert(complete_rse_recovery(&queued) == FWK_E_TIMEOUT && !armed);
    assert(start_rse_recovery() == 0);
    ticks += 10000; fire(); fail_schedule = true;
    poll_rse_recovery(ctx.generation);
    assert(complete_rse_recovery(&queued) == FWK_E_DEVICE && !ctx.recovery_pending);
    fail_schedule = false;
    assert(start_rse_recovery() == 0);
    fail_queue = true; fire();
    assert(!ctx.recovery_pending && !ctx.alarm_armed && !armed);
    puts("RSE polling PASS: one-shot delayed dispatch, no tick replay, ACK/deadline, stale generation, queue/rearm failure");
    return 0;
}
