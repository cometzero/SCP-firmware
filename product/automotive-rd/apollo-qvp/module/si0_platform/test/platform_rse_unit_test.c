/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "unity.h"
#include <internal/Mockfwk_core_internal.h>
#include UNIT_TEST_SRC

static unsigned int notifications;
static unsigned int waits;
static int notify_status;
static bool immediate_ack;
static uint64_t clock_ticks;
static unsigned int starts, stops, queued;
static struct fwk_event completion;

static int counter(fwk_id_t id, uint64_t *value)
{ *value = clock_ticks; return FWK_SUCCESS; }
static int ticks(fwk_id_t id, uint32_t us, uint64_t *value)
{ *value = us; return FWK_SUCCESS; }
static int alarm_start(fwk_id_t id, unsigned int us,
    enum mod_timer_alarm_type type, void (*callback)(uintptr_t), uintptr_t param)
{
    TEST_ASSERT_EQUAL(10000, us);
    TEST_ASSERT_EQUAL(MOD_TIMER_ALARM_TYPE_PERIODIC, type);
    starts++;
    return FWK_SUCCESS;
}
static int alarm_stop(fwk_id_t id) { stops++; return FWK_SUCCESS; }
static int release_channel(fwk_id_t id) { return FWK_SUCCESS; }
static int enqueue(struct fwk_event *event, int call_count)
{
    const fwk_id_t source = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM);
    TEST_ASSERT_EQUAL(source.value, event->source_id.value);
    TEST_ASSERT_EQUAL(source.value, event->target_id.value);
    completion = *event;
    queued++;
    return FWK_SUCCESS;
}

static int notify_warm_reset(void)
{
    TEST_ASSERT_FALSE(ctx.rse_doorbell_received);
    notifications++;
    /* Model an immediate ACK: it must not be cleared after notification. */
    ctx.rse_doorbell_received = immediate_ack;
    ctx.ack_counter = clock_ticks;
    return notify_status;
}

static int wait_for_ack(fwk_id_t id, unsigned int timeout,
    bool (*condition)(void *), void *data)
{
    (void)id;
    TEST_ASSERT_EQUAL(800000, timeout);
    waits++;
    return condition(data) ? FWK_SUCCESS : FWK_E_TIMEOUT;
}

void setUp(void)
{
    static const struct mod_si0_platform_config config = {
        .rse_sync_wait_us = 800000,
        .rse_recovery_timeout_us = 10000000,
    };
    static const struct mod_scmi_system_power_platform_api power_api = {
        .notify_warm_reset = notify_warm_reset,
    };
    static const struct mod_timer_api timer_api = {
        .wait = wait_for_ack, .get_counter = counter, .time_to_timestamp = ticks,
    };
    static const struct mod_timer_alarm_api alarm_api = {
        .start = alarm_start, .stop = alarm_stop,
    };
    static const struct mod_transport_firmware_api transport_api = {
        .release_transport_channel_lock = release_channel,
    };
    ctx.config = &config;
    ctx.sys_power_api = &power_api;
    ctx.timer_api = &timer_api;
    ctx.alarm_api = &alarm_api;
    ctx.transport_api = &transport_api;
    ctx.recovery_pending = ctx.completion_ready = false;
    ctx.rse_doorbell_received = true;
    notifications = waits = 0;
    notify_status = FWK_SUCCESS;
    immediate_ack = true;
    clock_ticks = starts = stops = queued = 0;
    __fwk_put_event_StubWithCallback(enqueue);
}

void tearDown(void) {}

static void test_platform_notification_precedes_ack_wait(void)
{
    TEST_ASSERT_EQUAL(FWK_SUCCESS, notify_rse_and_wait_for_response(true));
    TEST_ASSERT_EQUAL(1, notifications);
    TEST_ASSERT_EQUAL(1, waits);
}

static void test_notification_error_does_not_wait(void)
{
    notify_status = FWK_E_DEVICE;
    TEST_ASSERT_EQUAL(FWK_E_DEVICE, notify_rse_and_wait_for_response(true));
    TEST_ASSERT_EQUAL(1, notifications);
    TEST_ASSERT_EQUAL(0, waits);
}

static void test_scmi_origin_does_not_duplicate_notification(void)
{
    TEST_ASSERT_EQUAL(FWK_E_TIMEOUT, notify_rse_and_wait_for_response(false));
    TEST_ASSERT_EQUAL(0, notifications);
    TEST_ASSERT_EQUAL(1, waits);
}

static void test_async_immediate_ack_and_deduplicated_completion(void)
{
    TEST_ASSERT_EQUAL(FWK_SUCCESS, start_rse_recovery());
    TEST_ASSERT_EQUAL(0, waits);
    recovery_poll(ctx.generation);
    TEST_ASSERT_EQUAL(1, queued);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, complete_rse_recovery(&completion));
    TEST_ASSERT_EQUAL(FWK_E_STATE, complete_rse_recovery(&completion));
    recovery_poll(ctx.generation);
    TEST_ASSERT_EQUAL(1, queued);
}

static void test_async_delayed_ack_and_repeated_generation(void)
{
    unsigned int old_generation;
    immediate_ack = false;
    TEST_ASSERT_EQUAL(FWK_SUCCESS, start_rse_recovery());
    old_generation = ctx.generation;
    clock_ticks = 3442000;
    recovery_poll(ctx.generation);
    TEST_ASSERT_EQUAL(0, queued);
    ctx.rse_doorbell_received = true;
    recovery_poll(ctx.generation);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, complete_rse_recovery(&completion));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, start_rse_recovery());
    recovery_poll(old_generation);
    TEST_ASSERT_EQUAL(1, queued);
    TEST_ASSERT_EQUAL(FWK_E_STATE, complete_rse_recovery(&completion));
    ctx.rse_doorbell_received = true;
    recovery_poll(ctx.generation);
    TEST_ASSERT_EQUAL(2, queued);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, complete_rse_recovery(&completion));
    TEST_ASSERT_EQUAL(0, waits);
}

static void test_async_deadline_and_late_ack_fail_closed(void)
{
    immediate_ack = false;
    TEST_ASSERT_EQUAL(FWK_SUCCESS, start_rse_recovery());
    clock_ticks = 10000000;
    recovery_poll(ctx.generation);
    TEST_ASSERT_EQUAL(FWK_E_TIMEOUT, complete_rse_recovery(&completion));
    ctx.rse_doorbell_received = true;
    recovery_poll(ctx.generation);
    TEST_ASSERT_EQUAL(1, queued);
    TEST_ASSERT_EQUAL(FWK_E_STATE, complete_rse_recovery(&completion));
    TEST_ASSERT_EQUAL(1, stops);
}

static void test_async_notification_failure_cancels_alarm(void)
{
    notify_status = FWK_E_STATE;
    TEST_ASSERT_EQUAL(FWK_E_STATE, start_rse_recovery());
    TEST_ASSERT_FALSE(ctx.recovery_pending);
    TEST_ASSERT_EQUAL(1, stops);
    recovery_poll(ctx.generation);
    TEST_ASSERT_EQUAL(0, queued);
}

static void test_ack_after_deadline_before_poll_is_rejected(void)
{
    immediate_ack = false;
    TEST_ASSERT_EQUAL(FWK_SUCCESS, start_rse_recovery());
    clock_ticks = 10000001;
    TEST_ASSERT_EQUAL(FWK_SUCCESS, signal_message(FWK_ID_NONE));
    TEST_ASSERT_EQUAL(clock_ticks, ctx.ack_counter);
    recovery_poll(ctx.generation);
    TEST_ASSERT_EQUAL(FWK_E_TIMEOUT, complete_rse_recovery(&completion));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_platform_notification_precedes_ack_wait);
    RUN_TEST(test_notification_error_does_not_wait);
    RUN_TEST(test_scmi_origin_does_not_duplicate_notification);
    RUN_TEST(test_async_immediate_ack_and_deduplicated_completion);
    RUN_TEST(test_async_delayed_ack_and_repeated_generation);
    RUN_TEST(test_async_deadline_and_late_ack_fail_closed);
    RUN_TEST(test_async_notification_failure_cancels_alarm);
    RUN_TEST(test_ack_after_deadline_before_poll_is_rejected);
    return UNITY_END();
}
