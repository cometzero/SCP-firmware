/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "si0_cfgd_scmi.h"
#include "unity.h"

#include <Mockfwk_id.h>
#include <Mockfwk_module.h>
#include <Mockfwk_notification.h>
#include <Mockfwk_interrupt.h>
#include <internal/Mockfwk_core_internal.h>

#include <mod_power_domain.h>
#include <mod_ppu_v1.h>
#include <mod_si0_platform.h>
#include <mod_transport.h>

#include <fwk_module_idx.h>

#include UNIT_TEST_SRC

#include "Mocksi0_platform.h"
#include "config_si0_platform.h"

static int mock_sds_struct_write_status = FWK_SUCCESS;
static uint32_t mock_sds_struct_write_structure_id;
static unsigned int mock_sds_struct_write_offset;
static uint32_t mock_sds_struct_write_data;
static size_t mock_sds_struct_write_size;

static int mock_sds_struct_write(
    uint32_t structure_id,
    unsigned int offset,
    const void *data,
    size_t size)
{
    mock_sds_struct_write_structure_id = structure_id;
    mock_sds_struct_write_offset = offset;
    mock_sds_struct_write_data = *(const uint32_t *)data;
    mock_sds_struct_write_size = size;

    return mock_sds_struct_write_status;
}

void setUp(void)
{
    mock_sds_struct_write_status = FWK_SUCCESS;
    mock_sds_struct_write_structure_id = 0U;
    mock_sds_struct_write_offset = 0U;
    mock_sds_struct_write_data = 0U;
    mock_sds_struct_write_size = 0U;
}

void tearDown(void)
{
    /* Do Nothing */
}

struct mod_si0_platform_config system_config = {
    .primary_cpu_mpid = 0,
    .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
};

const struct mod_transport_firmware_signal_api
    platform_rse_transport_signal_api = { 0 };

const void *get_rse_platform_transport_signal_api(void)
{
    return &platform_rse_transport_signal_api;
}

int platform_rse_bind(const struct mod_si0_platform_config *config)
{
    return FWK_SUCCESS;
}

int notify_rse_and_wait_for_response(bool platform_origin)
{
    return FWK_SUCCESS;
}

int start_rse_recovery(void) { return FWK_SUCCESS; }
int complete_rse_recovery(const struct fwk_event *event) { return FWK_E_TIMEOUT; }

void test_failed_rse_recovery_does_not_boot_or_rearm(void)
{
    struct fwk_event event = { .id = FWK_ID_EVENT_INIT(
        FWK_MODULE_IDX_SI0_PLATFORM, MOD_SI0_PLATFORM_RSE_RECOVERY_DONE) };

    ap_watchdog_recovery = true;
    fwk_id_get_event_idx_ExpectAndReturn(event.id, MOD_SI0_PLATFORM_RSE_RECOVERY_DONE);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, si0_platform_process_event(&event, NULL));
    TEST_ASSERT_TRUE(ap_watchdog_recovery);
    ap_watchdog_recovery = false;
}
/*!
 * \brief SI0 Platform unit test: si0_platform_mod_init(),
 *
 *  \details Test successful initialization of si0_platform module
 */
void test_si0_platform_mod_init_success(void)
{
    int status;

    fwk_id_type_is_valid_ExpectAndReturn(system_config.timer_id, true);
    fwk_id_type_is_valid_ExpectAndReturn(system_config.transport_id, true);
    status =
        si0_platform_mod_init(fwk_module_id_si0_platform, 0, &system_config);

    TEST_ASSERT_EQUAL(status, FWK_SUCCESS);
}

/*!
 * \brief SI0 Platform unit test: si0_platform_bind(),
 *
 *  \details Test successful bind of si0_platform module
 */
void test_si0_platform_bind_success(void)
{
    int status;

    platform_power_mgmt_bind_ExpectAndReturn(FWK_SUCCESS);
    fwk_module_bind_IgnoreAndReturn(FWK_SUCCESS);

    status = si0_platform_bind(fwk_module_id_si0_platform, 0);

    TEST_ASSERT_EQUAL(status, FWK_SUCCESS);
}

/*!
 * \brief SI0 Platform unit test: si0_platform_bind(),
 *
 *  \details Test failure in bind of si0_platform module
 */
void test_si0_platform_bind_fail(void)
{
    int status;

    platform_power_mgmt_bind_ExpectAndReturn(FWK_E_DATA);
    fwk_module_bind_IgnoreAndReturn(FWK_E_DATA);

    status = si0_platform_bind(fwk_module_id_si0_platform, 0);

    TEST_ASSERT_EQUAL(status, FWK_E_DATA);
}

/*!
 * \brief SI0 Platform unit test: si0_platform_start(),
 *
 *  \details Test successful start of si0_platform module
 */
void test_si0_platform_mod_start_success(void)
{
    int status;
    struct fwk_event event = { 0 };
    event.id = mod_si0_platform_notification_subsys_init;
    event.source_id = fwk_module_id_si0_platform;
    unsigned int count = 0U;

    fwk_notification_notify_ExpectAndReturn(&event, &count, FWK_SUCCESS);

    fwk_id_get_element_idx_IgnoreAndReturn(0);
    fwk_module_get_element_name_IgnoreAndReturn("Test");

    fwk_id_t pd_transition_source_id =
        FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, 0);
    fwk_id_build_element_id_ExpectAndReturn(
        fwk_module_id_power_domain, 0, pd_transition_source_id);

    fwk_id_t pd_transition_notification_id = FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN,
        MOD_PD_NOTIFICATION_IDX_POWER_STATE_TRANSITION);

    fwk_notification_subscribe_ExpectAndReturn(
        pd_transition_notification_id,
        pd_transition_source_id,
        fwk_module_id_si0_platform,
        FWK_SUCCESS);

    fwk_id_t mod_pd_notification_id_pre_warmreset = FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN, MOD_PD_NOTIFICATION_IDX_PRE_WARM_RESET);

    fwk_notification_subscribe_ExpectAndReturn(
        mod_pd_notification_id_pre_warmreset,
        FWK_ID_MODULE(FWK_MODULE_IDX_POWER_DOMAIN),
        fwk_module_id_si0_platform,
        FWK_SUCCESS);

    status = si0_platform_start(fwk_module_id_si0_platform);
    TEST_ASSERT_EQUAL(status, FWK_SUCCESS);
}

/*!
 * \brief SI0 Platform unit test: si0_platform_start(),
 *
 *  \details Test failure starting the si0_platform module
 */
void test_si0_platform_mod_start_pre_warmreset_fail_notification(void)
{
    int status;
    struct fwk_event event = { 0 };
    event.id = mod_si0_platform_notification_subsys_init;
    event.source_id = fwk_module_id_si0_platform;
    unsigned int count = 0U;

    fwk_notification_notify_ExpectAndReturn(&event, &count, FWK_E_DATA);

    fwk_id_get_element_idx_IgnoreAndReturn(0);
    fwk_module_get_element_name_IgnoreAndReturn("Test");

    fwk_id_t pd_transition_source_id =
        FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, 0);
    fwk_id_build_element_id_ExpectAndReturn(
        fwk_module_id_power_domain, 0, pd_transition_source_id);

    fwk_id_t pd_transition_notification_id = FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN,
        MOD_PD_NOTIFICATION_IDX_POWER_STATE_TRANSITION);
    fwk_notification_subscribe_ExpectAndReturn(
        pd_transition_notification_id,
        pd_transition_source_id,
        fwk_module_id_si0_platform,
        FWK_E_DATA);

    status = si0_platform_start(fwk_module_id_si0_platform);
    TEST_ASSERT_EQUAL(status, FWK_E_PANIC);
}

/*!
 * \brief SI0 Platform unit test: si0_platform_start(),
 *
 *  \details Test failure starting the si0_platform module
 */
void test_si0_platform_mod_start_pd_trans_fail_notification(void)
{
    int status;
    struct fwk_event event = { 0 };
    event.id = mod_si0_platform_notification_subsys_init;
    event.source_id = fwk_module_id_si0_platform;
    unsigned int count = 0U;

    fwk_notification_notify_ExpectAndReturn(&event, &count, FWK_E_DATA);

    fwk_id_get_element_idx_IgnoreAndReturn(0);
    fwk_module_get_element_name_IgnoreAndReturn("Test");

    fwk_id_t pd_transition_source_id =
        FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, 0);
    fwk_id_build_element_id_ExpectAndReturn(
        fwk_module_id_power_domain, 0, pd_transition_source_id);

    fwk_id_t pd_transition_notification_id = FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN,
        MOD_PD_NOTIFICATION_IDX_POWER_STATE_TRANSITION);
    fwk_notification_subscribe_ExpectAndReturn(
        pd_transition_notification_id,
        pd_transition_source_id,
        fwk_module_id_si0_platform,
        FWK_SUCCESS);

    fwk_id_t mod_pd_notification_id_pre_warmreset = FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN, MOD_PD_NOTIFICATION_IDX_PRE_WARM_RESET);

    fwk_notification_subscribe_ExpectAndReturn(
        mod_pd_notification_id_pre_warmreset,
        FWK_ID_MODULE(FWK_MODULE_IDX_POWER_DOMAIN),
        fwk_module_id_si0_platform,
        FWK_E_DATA);

    status = si0_platform_start(fwk_module_id_si0_platform);
    TEST_ASSERT_EQUAL(status, FWK_E_PANIC);
}

/*!
 * \brief SI0 Platform unit test: update_sds_reset_syndrome(),
 *
 *  \details Test successful reset syndrome write to SDS
 */
void test_update_sds_reset_syndrome_success(void)
{
    int status;
    const uint32_t reset_syndrome = 0x8U;
    const struct mod_sds_structure_desc sds_structure_desc = {
        .id = SDS_RESET_SYNDROME_STRUCT_ID,
        .size = sizeof(reset_syndrome),
    };
    static const struct mod_sds_api sds_api = {
        .struct_write = mock_sds_struct_write,
    };

    si0_platform_ctx.sds_api = &sds_api;
    fwk_module_get_data_ExpectAndReturn(
        sds_reset_syndrome_id, &sds_structure_desc);

    status = update_sds_reset_syndrome(reset_syndrome);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(
        sds_structure_desc.id, mock_sds_struct_write_structure_id);
    TEST_ASSERT_EQUAL(0U, mock_sds_struct_write_offset);
    TEST_ASSERT_EQUAL(reset_syndrome, mock_sds_struct_write_data);
    TEST_ASSERT_EQUAL(sizeof(reset_syndrome), mock_sds_struct_write_size);
}

/*!
 * \brief SI0 Platform unit test: update_sds_reset_syndrome(),
 *
 *  \details Test failure when SDS structure descriptor is unavailable
 */
void test_update_sds_reset_syndrome_fail_null_sds_desc(void)
{
    int status;
    static const struct mod_sds_api sds_api = {
        .struct_write = mock_sds_struct_write,
    };

    si0_platform_ctx.sds_api = &sds_api;
    fwk_module_get_data_ExpectAndReturn(sds_reset_syndrome_id, NULL);

    status = update_sds_reset_syndrome(0x8U);

    TEST_ASSERT_EQUAL(FWK_E_DATA, status);
}

/*!
 * \brief SI0 Platform unit test: update_sds_reset_syndrome(),
 *
 *  \details Test failure when SDS write operation fails
 */
void test_update_sds_reset_syndrome_fail_struct_write(void)
{
    int status;
    const uint32_t reset_syndrome = 0x8U;
    const struct mod_sds_structure_desc sds_structure_desc = {
        .id = SDS_RESET_SYNDROME_STRUCT_ID,
        .size = sizeof(reset_syndrome),
    };
    static const struct mod_sds_api sds_api = {
        .struct_write = mock_sds_struct_write,
    };

    mock_sds_struct_write_status = FWK_E_DEVICE;
    si0_platform_ctx.sds_api = &sds_api;
    fwk_module_get_data_ExpectAndReturn(
        sds_reset_syndrome_id, &sds_structure_desc);

    status = update_sds_reset_syndrome(reset_syndrome);

    TEST_ASSERT_EQUAL(FWK_E_DEVICE, status);
}

static unsigned int watchdog_enqueue_count;

static int watchdog_capture_event(struct fwk_event_light *event, int call_count)
{
    const fwk_id_t module_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM);
    const fwk_id_t event_id = FWK_ID_EVENT_INIT(
        FWK_MODULE_IDX_SI0_PLATFORM, MOD_SI0_PLATFORM_AP_WATCHDOG);

    (void)call_count;
    TEST_ASSERT_EQUAL(module_id.value, event->source_id.value);
    TEST_ASSERT_EQUAL(module_id.value, event->target_id.value);
    TEST_ASSERT_EQUAL(event_id.value, event->id.value);
    watchdog_enqueue_count++;
    return FWK_SUCCESS;
}

void test_watchdog_isr_has_explicit_source_and_enqueues_once(void)
{
    static const struct mod_si0_platform_config config = { .ap_watchdog_irq = 321 };

    si0_platform_ctx.config = &config;
    ap_watchdog_recovery = false;
    watchdog_enqueue_count = 0;
    __fwk_put_event_light_StubWithCallback(watchdog_capture_event);
    fwk_interrupt_disable_ExpectAndReturn(321, FWK_SUCCESS);
    ap_watchdog_isr();
    TEST_ASSERT_TRUE(ap_watchdog_recovery);
    TEST_ASSERT_EQUAL(1, watchdog_enqueue_count);

    fwk_interrupt_disable_ExpectAndReturn(321, FWK_SUCCESS);
    ap_watchdog_isr();
    TEST_ASSERT_EQUAL(1, watchdog_enqueue_count);
    Mockfwk_interrupt_Verify();
    __fwk_put_event_light_StubWithCallback(NULL);
    ap_watchdog_recovery = false;
}

static int watchdog_shutdown_status;
static int watchdog_shutdown(enum mod_pd_system_shutdown state)
{
    TEST_ASSERT_EQUAL(MOD_PD_SYSTEM_WARM_RESET, state);
    return watchdog_shutdown_status;
}

void test_watchdog_event_requests_existing_warm_reset(void)
{
    static struct mod_pd_restricted_api api = {
        .system_shutdown = watchdog_shutdown,
    };
    struct fwk_event event = { .id = FWK_ID_EVENT_INIT(
        FWK_MODULE_IDX_SI0_PLATFORM, MOD_SI0_PLATFORM_AP_WATCHDOG) };

    si0_platform_ctx.mod_pd_restricted_api = &api;
    fwk_id_get_event_idx_ExpectAndReturn(event.id, MOD_SI0_PLATFORM_AP_WATCHDOG);
    watchdog_shutdown_status = FWK_PENDING;
    TEST_ASSERT_EQUAL(FWK_SUCCESS, si0_platform_process_event(&event, NULL));
    fwk_id_get_event_idx_ExpectAndReturn(event.id, MOD_SI0_PLATFORM_AP_WATCHDOG);
    watchdog_shutdown_status = FWK_E_DEVICE;
    TEST_ASSERT_EQUAL(FWK_E_DEVICE, si0_platform_process_event(&event, NULL));
}

void test_watchdog_rearm_requires_cleared_level_and_is_repeatable(void)
{
    static const struct mod_si0_platform_config config = { .ap_watchdog_irq = 321 };
    bool pending = false;
    bool enabled = true;
    unsigned int cycle;

    si0_platform_ctx.config = &config;
    for (cycle = 0; cycle < 2; cycle++) {
        ap_watchdog_recovery = true;
        fwk_interrupt_clear_pending_ExpectAndReturn(321, FWK_SUCCESS);
        fwk_interrupt_is_pending_ExpectAnyArgsAndReturn(FWK_SUCCESS);
        fwk_interrupt_is_pending_ReturnThruPtr_pending(&pending);
        fwk_interrupt_enable_ExpectAndReturn(321, FWK_SUCCESS);
        fwk_interrupt_is_enabled_ExpectAnyArgsAndReturn(FWK_SUCCESS);
        fwk_interrupt_is_enabled_ReturnThruPtr_enabled(&enabled);
        fwk_interrupt_is_pending_ExpectAnyArgsAndReturn(FWK_SUCCESS);
        fwk_interrupt_is_pending_ReturnThruPtr_pending(&pending);
        TEST_ASSERT_EQUAL(FWK_SUCCESS, ap_watchdog_rearm());
        TEST_ASSERT_FALSE(ap_watchdog_recovery);
    }
    ap_watchdog_recovery = true;
    pending = true;
    fwk_interrupt_clear_pending_ExpectAndReturn(321, FWK_SUCCESS);
    fwk_interrupt_is_pending_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_interrupt_is_pending_ReturnThruPtr_pending(&pending);
    TEST_ASSERT_EQUAL(FWK_PENDING, ap_watchdog_rearm());
    TEST_ASSERT_TRUE(ap_watchdog_recovery);
}

static unsigned int context_reset_count;
static uint64_t rearm_clock;
static unsigned int rearm_alarms, rearm_events;
static int rearm_counter(fwk_id_t id, uint64_t *counter)
{ *counter = rearm_clock; return FWK_SUCCESS; }
static int rearm_ticks(fwk_id_t id, uint32_t us, uint64_t *ticks)
{ *ticks = us; return FWK_SUCCESS; }
static int rearm_start_alarm(fwk_id_t id, uint32_t us,
    enum mod_timer_alarm_type type, void (*callback)(uintptr_t), uintptr_t generation)
{
    TEST_ASSERT_EQUAL(10000, us);
    TEST_ASSERT_EQUAL(MOD_TIMER_ALARM_TYPE_ONCE, type);
    rearm_alarms++;
    return FWK_SUCCESS;
}
static int capture_rearm_event(struct fwk_event *event, int calls)
{
    fwk_id_t source = FWK_ID_MODULE(FWK_MODULE_IDX_SI0_PLATFORM);
    TEST_ASSERT_EQUAL(source.value, event->source_id.value);
    TEST_ASSERT_EQUAL(rearm_ctx.generation, *(unsigned int *)event->params);
    rearm_events++;
    return FWK_SUCCESS;
}
static void setup_rearm_poll(void)
{
    static const struct mod_si0_platform_config config = {
        .ap_watchdog_irq = 321, .watchdog_rearm_timeout_us = 100000,
    };
    static const struct mod_timer_api timer = {
        .get_counter = rearm_counter, .time_to_timestamp = rearm_ticks,
    };
    static const struct mod_timer_alarm_api alarm = { .start = rearm_start_alarm };
    si0_platform_ctx.config = &config;
    si0_platform_ctx.rearm_timer_api = &timer;
    si0_platform_ctx.rearm_alarm_api = &alarm;
    rearm_ctx.pending = false;
    rearm_clock = rearm_alarms = rearm_events = 0;
    ap_watchdog_recovery = true;
}
static void expect_rearm_poll_pending(bool remains_pending)
{
    static bool pending_set = true;
    static bool pending_clear = false;
    static bool enabled = true;
    bool *after = remains_pending ? &pending_set : &pending_clear;
    fwk_interrupt_is_pending_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_interrupt_is_pending_ReturnThruPtr_pending(&pending_set);
    fwk_interrupt_clear_pending_ExpectAndReturn(321, FWK_SUCCESS);
    fwk_interrupt_is_pending_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_interrupt_is_pending_ReturnThruPtr_pending(after);
    if (!remains_pending) {
        fwk_interrupt_enable_ExpectAndReturn(321, FWK_SUCCESS);
        fwk_interrupt_is_enabled_ExpectAnyArgsAndReturn(FWK_SUCCESS);
        fwk_interrupt_is_enabled_ReturnThruPtr_enabled(&enabled);
        fwk_interrupt_is_pending_ExpectAnyArgsAndReturn(FWK_SUCCESS);
        fwk_interrupt_is_pending_ReturnThruPtr_pending(after);
    }
}
void test_async_rearm_pending_then_clear_and_stale_callback(void)
{
    unsigned int generation;
    setup_rearm_poll();
    expect_rearm_poll_pending(true);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, start_watchdog_rearm());
    TEST_ASSERT_TRUE(ap_watchdog_recovery);
    TEST_ASSERT_TRUE(rearm_ctx.pending);
    TEST_ASSERT_EQUAL(1, rearm_alarms);
    generation = rearm_ctx.generation;
    __fwk_put_event_StubWithCallback(capture_rearm_event);
    rearm_alarm_callback(generation - 1);
    TEST_ASSERT_EQUAL(0, rearm_events);
    rearm_alarm_callback(generation);
    TEST_ASSERT_EQUAL(1, rearm_events);
    rearm_clock = 10000;
    expect_rearm_poll_pending(false);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, watchdog_rearm_poll(generation));
    TEST_ASSERT_FALSE(rearm_ctx.pending);
    TEST_ASSERT_FALSE(ap_watchdog_recovery);
    TEST_ASSERT_EQUAL(2, rearm_ctx.attempts);
    rearm_alarm_callback(generation);
    TEST_ASSERT_EQUAL(1, rearm_events);
    __fwk_put_event_StubWithCallback(NULL);
    Mockfwk_interrupt_Verify();
}
void test_async_rearm_stuck_pending_times_out_without_unmask(void)
{
    setup_rearm_poll();
    expect_rearm_poll_pending(true);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, start_watchdog_rearm());
    for (unsigned int i = 1; i < 10; i++) {
        rearm_clock = i * 10000;
        expect_rearm_poll_pending(true);
        TEST_ASSERT_EQUAL(FWK_SUCCESS, watchdog_rearm_poll(rearm_ctx.generation));
    }
    rearm_clock = 100000;
    TEST_ASSERT_EQUAL(FWK_E_TIMEOUT, watchdog_rearm_poll(rearm_ctx.generation));
    TEST_ASSERT_EQUAL(10, rearm_ctx.attempts);
    TEST_ASSERT_TRUE(ap_watchdog_recovery);
    TEST_ASSERT_FALSE(rearm_ctx.pending);
    Mockfwk_interrupt_Verify();
    ap_watchdog_recovery = false;
}
void test_async_rearm_clear_failure_stays_masked(void)
{
    bool pending = true;
    setup_rearm_poll();
    fwk_interrupt_is_pending_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_interrupt_is_pending_ReturnThruPtr_pending(&pending);
    fwk_interrupt_clear_pending_ExpectAndReturn(321, FWK_E_DEVICE);
    TEST_ASSERT_EQUAL(FWK_E_DEVICE, start_watchdog_rearm());
    TEST_ASSERT_EQUAL(0, rearm_alarms);
    TEST_ASSERT_TRUE(ap_watchdog_recovery);
    TEST_ASSERT_FALSE(rearm_ctx.pending);
    Mockfwk_interrupt_Verify();
    ap_watchdog_recovery = false;
}
void test_rearm_readback_error_and_mask_failure_are_reported(void)
{
    bool pending = false;
    setup_rearm_poll();
    fwk_interrupt_clear_pending_ExpectAndReturn(321, FWK_SUCCESS);
    fwk_interrupt_is_pending_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_interrupt_is_pending_ReturnThruPtr_pending(&pending);
    fwk_interrupt_enable_ExpectAndReturn(321, FWK_SUCCESS);
    fwk_interrupt_is_enabled_ExpectAnyArgsAndReturn(FWK_E_DEVICE);
    fwk_interrupt_disable_ExpectAndReturn(321, FWK_E_SUPPORT);
    TEST_ASSERT_EQUAL(FWK_E_SUPPORT, ap_watchdog_rearm());
    TEST_ASSERT_TRUE(ap_watchdog_recovery);
    Mockfwk_interrupt_Verify();
    ap_watchdog_recovery = false;
}
static int enable_with_immediate_irq(unsigned int irq, int call_count)
{
    TEST_ASSERT_FALSE(ap_watchdog_recovery);
    ap_watchdog_isr();
    return FWK_SUCCESS;
}

void test_rearm_preserves_immediate_new_irq_and_enable_failure(void)
{
    static const struct mod_si0_platform_config config = { .ap_watchdog_irq = 321 };
    bool pending = false;
    bool enabled = false; /* Immediate ISR has masked the new recovery. */
    si0_platform_ctx.config = &config;
    ap_watchdog_recovery = true;
    watchdog_enqueue_count = 0;
    __fwk_put_event_light_StubWithCallback(watchdog_capture_event);
    fwk_interrupt_clear_pending_ExpectAndReturn(321, FWK_SUCCESS);
    fwk_interrupt_is_pending_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_interrupt_is_pending_ReturnThruPtr_pending(&pending);
    fwk_interrupt_disable_ExpectAndReturn(321, FWK_SUCCESS);
    fwk_interrupt_enable_StubWithCallback(enable_with_immediate_irq);
    fwk_interrupt_is_enabled_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_interrupt_is_enabled_ReturnThruPtr_enabled(&enabled);
    fwk_interrupt_is_pending_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_interrupt_is_pending_ReturnThruPtr_pending(&pending);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ap_watchdog_rearm());
    TEST_ASSERT_TRUE(ap_watchdog_recovery);
    TEST_ASSERT_EQUAL(1, watchdog_enqueue_count);
    fwk_interrupt_enable_StubWithCallback(NULL);
    __fwk_put_event_light_StubWithCallback(NULL);

    fwk_interrupt_clear_pending_ExpectAndReturn(321, FWK_SUCCESS);
    fwk_interrupt_is_pending_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_interrupt_is_pending_ReturnThruPtr_pending(&pending);
    fwk_interrupt_enable_ExpectAndReturn(321, FWK_E_DEVICE);
    TEST_ASSERT_EQUAL(FWK_E_DEVICE, ap_watchdog_rearm());
    TEST_ASSERT_TRUE(ap_watchdog_recovery);
    Mockfwk_interrupt_Verify();
    ap_watchdog_recovery = false;
}

static unsigned int prepared_core_count;
static int prepare_pfdi_restart(fwk_id_t pd_id)
{
    fwk_id_t expected = FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, prepared_core_count);
    TEST_ASSERT_LESS_THAN(4, prepared_core_count);
    TEST_ASSERT_EQUAL(expected.value, pd_id.value);
    prepared_core_count++;
    return FWK_SUCCESS;
}
static int context_reset_status;
static int context_reset(void)
{
    context_reset_count++;
    return context_reset_status;
}
static int boot_after_context_reset(fwk_id_t id, bool response, uint32_t state)
{
    TEST_ASSERT_EQUAL(1, context_reset_count);
    return FWK_SUCCESS;
}

void test_context_reset_precedes_boot_and_failure_blocks_boot(void)
{
    static const struct mod_si0_platform_config config = { .ap_pfdi_core_count = 4 };
    static const struct mod_apcontext_reset_api context_api = { .reset = context_reset };
    static const struct mod_pfdi_monitor_restart_api pfdi_api = { .prepare = prepare_pfdi_restart };
    static struct mod_pd_restricted_api pd_api = { .set_state = boot_after_context_reset };
    static const struct mod_sds_api sds_api = { .struct_write = mock_sds_struct_write };
    const struct mod_sds_structure_desc desc = { .id = SDS_RESET_SYNDROME_STRUCT_ID };
    size_t count = 0;

    si0_platform_ctx.apcontext_api = &context_api;
    si0_platform_ctx.config = &config;
    si0_platform_ctx.pfdi_restart_api = &pfdi_api;
    si0_platform_ctx.mod_pd_restricted_api = &pd_api;
    si0_platform_ctx.sds_api = &sds_api;
    ap_watchdog_recovery = true;
    TEST_ASSERT_EQUAL(16, platform_get_core_count());
    TEST_ASSERT_EQUAL(4, si0_platform_ctx.config->ap_pfdi_core_count);
    prepared_core_count = 0;
    context_reset_count = 0;
    context_reset_status = FWK_E_STATE;
    TEST_ASSERT_EQUAL(FWK_E_STATE, finish_warm_reset());
    TEST_ASSERT_TRUE(ap_watchdog_recovery);
    TEST_ASSERT_EQUAL(1, context_reset_count);
    TEST_ASSERT_EQUAL(4, prepared_core_count);

    prepared_core_count = 0;
    context_reset_count = 0;
    context_reset_status = FWK_SUCCESS;
    fwk_module_get_element_count_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&count);
    fwk_module_get_data_ExpectAndReturn(sds_reset_syndrome_id, &desc);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, finish_warm_reset());
    TEST_ASSERT_EQUAL(1, context_reset_count);
    TEST_ASSERT_EQUAL(4, prepared_core_count);
    ap_watchdog_recovery = false;
}

int si0_platform_test_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_si0_platform_mod_init_success);
    RUN_TEST(test_si0_platform_bind_success);
    RUN_TEST(test_si0_platform_bind_fail);
    RUN_TEST(test_si0_platform_mod_start_success);
    RUN_TEST(test_si0_platform_mod_start_pre_warmreset_fail_notification);
    RUN_TEST(test_si0_platform_mod_start_pd_trans_fail_notification);
    RUN_TEST(test_update_sds_reset_syndrome_success);
    RUN_TEST(test_update_sds_reset_syndrome_fail_null_sds_desc);
    RUN_TEST(test_update_sds_reset_syndrome_fail_struct_write);
    RUN_TEST(test_watchdog_event_requests_existing_warm_reset);
    RUN_TEST(test_watchdog_isr_has_explicit_source_and_enqueues_once);
    RUN_TEST(test_failed_rse_recovery_does_not_boot_or_rearm);
    RUN_TEST(test_watchdog_rearm_requires_cleared_level_and_is_repeatable);
    RUN_TEST(test_rearm_preserves_immediate_new_irq_and_enable_failure);
    RUN_TEST(test_async_rearm_pending_then_clear_and_stale_callback);
    RUN_TEST(test_async_rearm_stuck_pending_times_out_without_unmask);
    RUN_TEST(test_async_rearm_clear_failure_stays_masked);
    RUN_TEST(test_rearm_readback_error_and_mask_failure_are_reported);
    RUN_TEST(test_context_reset_precedes_boot_and_failure_blocks_boot);

    return UNITY_END();
}

int pd_transition_ap_platform_hook(unsigned int pd_state)
{
    return FWK_SUCCESS;
}

int main(void)
{
    return si0_platform_test_main();
}
