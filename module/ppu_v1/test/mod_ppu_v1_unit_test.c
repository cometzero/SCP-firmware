/*
 * Arm SCP/MCP Software
 * Copyright (c) 2022-2024, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config_ppu_v1.h"
#include "scp_unity.h"
#include "unity.h"

#include <Mockfwk_id.h>
#include <Mockfwk_mm.h>
#include <Mockfwk_module.h>
#include <Mockfwk_notification.h>
#include <Mockmod_ppu_v1_extra.h>
#include <internal/Mockfwk_core_internal.h>

#include <mod_ppu_v1_extra.h>

#include <fwk_element.h>
#include <fwk_macros.h>
#include <fwk_notification.h>
#include UNIT_TEST_SRC

#define CORES_PER_CLUSTER 2
#define PD_COUNT          2

static struct ppu_v1_pd_ctx pd_table[PD_COUNT];

static struct mod_timer_alarm_api alarm_api_driver = {
    .start = start_alarm_api,
    .stop = stop_alarm_api,
};

static unsigned int event_index(fwk_id_t id, int call_count)
{
    (void)call_count;
    return id.event.event_idx;
}

void setUp(void)
{
    Mockfwk_id_Init();
    Mockfwk_mm_Init();
    Mockfwk_module_Init();
    Mockfwk_notification_Init();
    Mockmod_ppu_v1_extra_Init();
    Mockfwk_core_internal_Init();

    memset(pd_table, 0, sizeof(pd_table));
    fwk_id_get_event_idx_StubWithCallback(event_index);
    memset(&ppu_v1_ctx, 0, sizeof(ppu_v1_ctx));
    ppu_v1_ctx.pd_ctx_table_size = PD_COUNT;

    ppu_v1_ctx.pd_ctx_table = pd_table;

    ppu_v1_ctx.max_num_cores_per_cluster =
        ppu_v1_config_data_ut.num_of_cores_in_cluster;

    ppu_v1_ctx.pd_ctx_table->config = pd_ppu_ctx_config;

    for (int i = 0; i < PD_COUNT; i++) {
        ppu_v1_ctx.pd_ctx_table[i].alarm_api = &alarm_api_driver;
    }
}

void tearDown(void)
{
    Mockfwk_id_Verify();
    Mockfwk_mm_Verify();
    Mockfwk_module_Verify();
    Mockfwk_notification_Verify();
    Mockmod_ppu_v1_extra_Verify();
    Mockfwk_core_internal_Verify();
}

void test_ppu_v1_pd_init_error(void)
{
    int status;
    fwk_id_t pd_id;
    unsigned int unused = 0;
    struct mod_ppu_v1_pd_config config = { 0 };

    config.pd_type = MOD_PD_TYPE_COUNT + 1;
    status = ppu_v1_pd_init(pd_id, unused, &config);
    TEST_ASSERT_EQUAL(status, FWK_E_DATA);
}

void test_ppu_v1_pd_init(void)
{
    int status;
    fwk_id_t pd_id;
    unsigned int unused = 0;
    struct mod_ppu_v1_pd_config config = { 0 };

    config.pd_type = MOD_PD_TYPE_CLUSTER;
    config.timer_config = NULL;
    config.ppu.irq = FWK_INTERRUPT_NONE;
    config.default_power_on = false;

    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
    fwk_optional_id_is_defined_ExpectAnyArgsAndReturn(false);

    struct ppu_v1_pd_ctx *core_pd_ctx_table_temp[CORES_PER_CLUSTER];
    static struct ppu_v1_pd_ctx p0;
    static struct ppu_v1_pd_ctx p1;

    core_pd_ctx_table_temp[0] = &p0;
    core_pd_ctx_table_temp[1] = &p1;

    /* Make a local one to get the size for the next malloc */
    struct ppu_v1_cluster_pd_ctx cluster_pd_ctx_temp;

    cluster_pd_ctx_temp.core_pd_ctx_table =
        (struct ppu_v1_pd_ctx **)&core_pd_ctx_table_temp;
    cluster_pd_ctx_temp.core_count = CORES_PER_CLUSTER;

    fwk_mm_calloc_ExpectAndReturn(
        1, sizeof(cluster_pd_ctx_temp), &ppu_v1_ctx.pd_ctx_table);

    fwk_mm_calloc_ExpectAndReturn(
        ppu_v1_ctx.max_num_cores_per_cluster,
        sizeof(core_pd_ctx_table_temp[0]),
        &core_pd_ctx_table_temp);

    status = ppu_v1_pd_init(pd_id, unused, &config);
    TEST_ASSERT_EQUAL(status, FWK_SUCCESS);
}

void test_ppu_v1_mod_init(void)
{
    fwk_id_t mod_id;
    int status;

    fwk_mm_calloc_ExpectAndReturn(
        PD_COUNT, sizeof(struct ppu_v1_pd_ctx), &pd_table);

    /* Clear to ensure it gets reset */
    ppu_v1_ctx.pd_ctx_table_size = 0;
    ppu_v1_ctx.max_num_cores_per_cluster = 0;

    status = ppu_v1_mod_init(mod_id, PD_COUNT, &ppu_v1_config_data_ut);
    TEST_ASSERT_EQUAL(status, FWK_SUCCESS);
    TEST_ASSERT_EQUAL(ppu_v1_ctx.pd_ctx_table_size, PD_COUNT);
    TEST_ASSERT_EQUAL(ppu_v1_ctx.max_num_cores_per_cluster, CORES_PER_CLUSTER);
}

void test_ppu_v1_core_pd_set_state_sleep(void)
{
    int status;
    fwk_id_t core_pd_id;
    struct ppu_v1_pd_ctx *pd_ctx_temp;

    pd_ctx_temp = &ppu_v1_ctx.pd_ctx_table[0];

    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
    ppu_v1_dynamic_enable_ExpectAnyArgs();
    ppu_v1_lock_off_enable_ExpectAnyArgs();
    ppu_v1_interrupt_unmask_ExpectAnyArgs();
    ppu_v1_set_input_edge_sensitivity_Expect(
        &pd_ctx_temp->ppu, PPU_V1_MODE_ON, PPU_V1_EDGE_SENSITIVITY_MASKED);
    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
    start_alarm_api_ExpectAndReturn(
        ppu_v1_ctx.pd_ctx_table[0].config->alarm_id,
        ppu_v1_ctx.pd_ctx_table[0].config->alarm_delay,
        MOD_TIMER_ALARM_TYPE_ONCE,
        deeper_locking_alarm_callback,
        0,
        FWK_SUCCESS);
    status = ppu_v1_core_pd_set_state(core_pd_id, MOD_PD_STATE_SLEEP);

    TEST_ASSERT_EQUAL(status, FWK_SUCCESS);
}

void test_start_deeper_locking_alarm(void)
{
    int status;
    fwk_id_t core_pd_id;

    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);

    start_alarm_api_ExpectAndReturn(
        ppu_v1_ctx.pd_ctx_table[0].config->alarm_id,
        ppu_v1_ctx.pd_ctx_table[0].config->alarm_delay,
        MOD_TIMER_ALARM_TYPE_ONCE,
        deeper_locking_alarm_callback,
        0,
        FWK_SUCCESS);

    status = start_deeper_locking_alarm(core_pd_id);

    TEST_ASSERT_EQUAL(status, FWK_SUCCESS);
}

void test_start_deeper_locking_alarm_null_api(void)
{
    int status;
    fwk_id_t core_pd_id;

    ppu_v1_ctx.pd_ctx_table[0].alarm_api = NULL;

    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);

    status = start_deeper_locking_alarm(core_pd_id);

    TEST_ASSERT_EQUAL(status, FWK_E_SUPPORT);
}

void test_deeper_locking_alarm_callback(void)
{
    uintptr_t param = (uintptr_t)0;
    struct ppu_v1_pd_ctx *pd_ctx_temp;
    pd_ctx_temp = &ppu_v1_ctx.pd_ctx_table[0];

    ppu_v1_lock_off_disable_Expect(&pd_ctx_temp->ppu);
    ppu_v1_off_unlock_ExpectAnyArgs();

    deeper_locking_alarm_callback(param);
}

static unsigned int suspend_off_reports;

static int capture_suspend_report(fwk_id_t id, unsigned int state)
{
    (void)id;
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, state);
    suspend_off_reports++;
    return FWK_SUCCESS;
}

void test_suspend_poll_reports_only_observed_off(void)
{
    struct mod_pd_driver_input_api input = {
        .report_power_state_transition = capture_suspend_report,
    };
    struct ppu_v1_pd_ctx pd = {
        .suspend_polls_left = 2,
        .pd_driver_input_api = &input,
    };

    suspend_off_reports = 0;
    ppu_v1_get_power_mode_ExpectAndReturn(&pd.ppu, PPU_V1_MODE_OFF);
    ppu_v1_is_dynamic_enabled_ExpectAndReturn(&pd.ppu, false);
    suspend_poll_callback((uintptr_t)&pd);
    TEST_ASSERT_EQUAL(1, suspend_off_reports);
    TEST_ASSERT_EQUAL(0, pd.suspend_polls_left);
    /* A stale callback cannot duplicate the completion. */
    suspend_poll_callback((uintptr_t)&pd);
    TEST_ASSERT_EQUAL(1, suspend_off_reports);
}

void test_suspend_poll_timeout_does_not_report_off(void)
{
    struct ppu_v1_ppu_reg ppu_registers = { 0 };
    struct mod_pd_driver_input_api input = {
        .report_power_state_transition = capture_suspend_report,
    };
    struct ppu_v1_pd_ctx pd = {
        .suspend_polls_left = 1,
        .pd_driver_input_api = &input,
        .ppu.ppu_reg = &ppu_registers,
    };

    suspend_off_reports = 0;
    ppu_v1_get_power_mode_ExpectAndReturn(&pd.ppu, PPU_V1_MODE_ON);
    suspend_poll_callback((uintptr_t)&pd);
    TEST_ASSERT_EQUAL(0, suspend_off_reports);
    TEST_ASSERT_EQUAL(0, pd.suspend_polls_left);
}

void test_suspend_poll_rearms_with_bounded_remaining_count(void)
{
    const struct mod_ppu_v1_pd_config config = {
        .suspend_poll_alarm_id = FWK_ID_NONE_INIT,
        .suspend_poll_interval_us = 1000,
    };
    struct ppu_v1_pd_ctx pd = {
        .config = &config,
        .suspend_polls_left = 2,
        .suspend_poll_alarm = &alarm_api_driver,
    };

    ppu_v1_get_power_mode_ExpectAndReturn(&pd.ppu, PPU_V1_MODE_ON);
    start_alarm_api_ExpectAndReturn(config.suspend_poll_alarm_id, 1000,
        MOD_TIMER_ALARM_TYPE_ONCE, suspend_poll_callback, (uintptr_t)&pd,
        FWK_SUCCESS);
    suspend_poll_callback((uintptr_t)&pd);
    TEST_ASSERT_EQUAL(1, pd.suspend_polls_left);
}

static unsigned int cluster_reports;
static unsigned int captured_state;
static unsigned int captured_states[8];

static int capture_cluster_report(fwk_id_t id, unsigned int state)
{
    (void)id;
    (void)state;
    if (cluster_reports < FWK_ARRAY_SIZE(captured_states))
        captured_states[cluster_reports] = state;
    cluster_reports++;
    captured_state = state;
    return FWK_SUCCESS;
}

static void check_cluster_transition_result(unsigned int state, int result)
{
    const struct mod_ppu_v1_pd_config config = { 0 };
    struct ppu_v1_cluster_pd_ctx cluster = { 0 };
    struct mod_pd_driver_input_api input = {
        .report_power_state_transition = capture_cluster_report,
    };
    struct ppu_v1_pd_ctx pd = {
        .config = &config, .data = &cluster, .pd_driver_input_api = &input,
    };

    ppu_v1_ctx.pd_ctx_table = &pd;
    cluster_reports = 0;
    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
    ppu_v1_set_input_edge_sensitivity_Expect(&pd.ppu, PPU_V1_MODE_ON,
        PPU_V1_EDGE_SENSITIVITY_MASKED);
    if (state == MOD_PD_STATE_ON)
        ppu_v1_request_operating_mode_ExpectAndReturn(
            &pd.ppu, config.opmode, FWK_SUCCESS);
    ppu_v1_set_power_mode_ExpectAndReturn(&pd.ppu,
        state == MOD_PD_STATE_ON ? PPU_V1_MODE_ON : PPU_V1_MODE_OFF,
        NULL, result);
    if (state == MOD_PD_STATE_ON && result == FWK_SUCCESS)
        ppu_v1_set_input_edge_sensitivity_Expect(&pd.ppu, PPU_V1_MODE_ON,
            PPU_V1_EDGE_SENSITIVITY_FALLING_EDGE);

    int status = ppu_v1_cluster_pd_set_state(FWK_ID_NONE, state);
    TEST_ASSERT_EQUAL(result == FWK_SUCCESS ? FWK_SUCCESS :
        (state == MOD_PD_STATE_OFF ? FWK_E_STATE : result), status);
    TEST_ASSERT_EQUAL(result == FWK_SUCCESS ? 1 : 0, cluster_reports);
}

void test_cluster_off_timeout_does_not_report_off(void)
{
    check_cluster_transition_result(MOD_PD_STATE_OFF, FWK_E_TIMEOUT);
}

void test_cluster_on_timeout_does_not_report_on(void)
{
    check_cluster_transition_result(MOD_PD_STATE_ON, FWK_E_TIMEOUT);
}

void test_cluster_success_reports_observed_transition(void)
{
    check_cluster_transition_result(MOD_PD_STATE_OFF, FWK_SUCCESS);
}

void test_cluster_on_arms_idle_detection_without_irq_caller(void)
{
    check_cluster_transition_result(MOD_PD_STATE_ON, FWK_SUCCESS);
}

static int lock_wait_timeout(
    fwk_id_t id, unsigned int timeout, bool (*condition)(void *), void *data)
{
    (void)id;
    TEST_ASSERT_EQUAL(1000, timeout);
    TEST_ASSERT_EQUAL_PTR(core_lock_completed_or_wake, condition);
    TEST_ASSERT_NOT_NULL(data);
    return FWK_E_TIMEOUT;
}

void test_dynamic_core_lock_wait_is_bounded_when_timer_configured(void)
{
    struct mod_timer_api timer_api = { .wait = lock_wait_timeout };
    struct ppu_v1_timer_ctx timer = { .timer_api = &timer_api, .delay_us = 1000 };
    struct ppu_v1_pd_ctx core = { 0 };
    struct ppu_v1_pd_ctx *cores[] = { &core };
    struct ppu_v1_cluster_pd_ctx cluster = {
        .core_count = 1, .core_pd_ctx_table = cores,
    };
    struct ppu_v1_pd_ctx pd = { .data = &cluster, .timer_ctx = &timer };

    ppu_v1_is_dynamic_enabled_ExpectAndReturn(&core.ppu, true);
    ppu_v1_lock_off_enable_Expect(&core.ppu);
    TEST_ASSERT_FALSE(lock_all_dynamic_cores(&pd));
}

static void check_core_transition_result(unsigned int state, int result)
{
    struct mod_pd_driver_input_api input = {
        .report_power_state_transition = capture_cluster_report,
    };
    const struct mod_ppu_v1_pd_config config = { 0 };
    struct ppu_v1_pd_ctx pd = {
        .config = &config, .pd_driver_input_api = &input,
    };

    ppu_v1_ctx.pd_ctx_table = &pd;
    cluster_reports = 0;
    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
    if (state == MOD_PD_STATE_ON)
        ppu_v1_interrupt_unmask_Expect(&pd.ppu, PPU_V1_IMR_DYN_POLICY_MIN_IRQ_MASK);
    ppu_v1_set_input_edge_sensitivity_Expect(&pd.ppu, PPU_V1_MODE_ON,
        PPU_V1_EDGE_SENSITIVITY_MASKED);
    if (state == MOD_PD_STATE_OFF) {
        ppu_v1_interrupt_mask_Expect(&pd.ppu, PPU_V1_IMR_DYN_POLICY_MIN_IRQ_MASK);
        ppu_v1_ack_interrupt_Expect(&pd.ppu, PPU_V1_ISR_DYN_POLICY_MIN_IRQ);
    }
    ppu_v1_set_power_mode_ExpectAndReturn(&pd.ppu,
        state == MOD_PD_STATE_ON ? PPU_V1_MODE_ON : PPU_V1_MODE_OFF,
        NULL, result);
    if (result == FWK_SUCCESS) {
        if (state == MOD_PD_STATE_ON) {
            ppu_v1_dynamic_enable_Expect(&pd.ppu, PPU_V1_MODE_OFF);
        } else {
            ppu_v1_lock_off_disable_Expect(&pd.ppu);
            ppu_v1_off_unlock_Expect(&pd.ppu);
        }
    }
    TEST_ASSERT_EQUAL(result, ppu_v1_core_pd_set_state(FWK_ID_NONE, state));
    TEST_ASSERT_EQUAL(result == FWK_SUCCESS ? 1 : 0, cluster_reports);
}

void test_core_off_timeout_does_not_report_off(void)
{
    check_core_transition_result(MOD_PD_STATE_OFF, FWK_E_TIMEOUT);
}

void test_core_on_timeout_does_not_report_on(void)
{
    check_core_transition_result(MOD_PD_STATE_ON, FWK_E_TIMEOUT);
}

void test_core_success_reports_observed_transition(void)
{
    check_core_transition_result(MOD_PD_STATE_OFF, FWK_SUCCESS);
    check_core_transition_result(MOD_PD_STATE_ON, FWK_SUCCESS);
}

static struct fwk_event queued_report;
static unsigned int queued_reports;

static int capture_queued_report(struct fwk_event *event, int call_count)
{
    (void)call_count;
    queued_report = *event;
    queued_reports++;
    return FWK_SUCCESS;
}

void test_dynamic_reports_coalesce_and_preserve_commanded_off(void)
{
    struct mod_pd_driver_input_api input = {
        .report_power_state_transition = capture_cluster_report,
    };
    struct ppu_v1_pd_ctx pd = {
        .id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PPU_V1, 0),
        .pd_driver_input_api = &input,
    };
    ppu_v1_ctx.pd_ctx_table = &pd;
    queued_reports = cluster_reports = 0;
    __fwk_put_event_StubWithCallback(capture_queued_report);

    for (unsigned int i = 0; i < 1000; ++i) {
        TEST_ASSERT_EQUAL(FWK_SUCCESS, report_power_state(&pd,
            i & 1 ? MOD_PD_STATE_ON : MOD_PD_STATE_SLEEP, true));
    }
    TEST_ASSERT_EQUAL(1, queued_reports);
    TEST_ASSERT_EQUAL(0, cluster_reports);
    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ppu_v1_process_event(&queued_report, NULL));
    TEST_ASSERT_EQUAL(1, cluster_reports);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_ON, captured_state);
    TEST_ASSERT_FALSE(pd.report_pending);

    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        report_power_state(&pd, MOD_PD_STATE_SLEEP, true));
    TEST_ASSERT_EQUAL(2, queued_reports);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        report_power_state(&pd, MOD_PD_STATE_OFF, false));
    TEST_ASSERT_EQUAL(2, cluster_reports); // commanded OFF is immediate
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, captured_state);
    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ppu_v1_process_event(&queued_report, NULL));
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, captured_state); // no stale SLEEP
    TEST_ASSERT_EQUAL(2, cluster_reports); // no duplicate commanded OFF
}

void test_dynamic_reports_skip_duplicates_but_preserve_transitions(void)
{
    struct mod_pd_driver_input_api input = {
        .report_power_state_transition = capture_cluster_report,
    };
    struct ppu_v1_pd_ctx pd = {
        .id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PPU_V1, 0),
        .pd_driver_input_api = &input,
    };
    const unsigned int states[] = {
        MOD_PD_STATE_ON, MOD_PD_STATE_ON, MOD_PD_STATE_SLEEP,
        MOD_PD_STATE_SLEEP, MOD_PD_STATE_ON,
    };
    const unsigned int reports[] = { 1, 1, 2, 2, 3 };
    ppu_v1_ctx.pd_ctx_table = &pd;
    queued_reports = cluster_reports = 0;
    __fwk_put_event_StubWithCallback(capture_queued_report);

    for (unsigned int i = 0; i < FWK_ARRAY_SIZE(states); ++i) {
        TEST_ASSERT_EQUAL(FWK_SUCCESS, report_power_state(&pd, states[i], true));
        fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
        TEST_ASSERT_EQUAL(FWK_SUCCESS, ppu_v1_process_event(&queued_report, NULL));
        TEST_ASSERT_EQUAL(reports[i], cluster_reports);
        TEST_ASSERT_EQUAL(states[i], captured_state);
    }
    /* Repeated explicit completion must not be deduplicated. */
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        report_power_state(&pd, MOD_PD_STATE_ON, false));
    TEST_ASSERT_EQUAL(4, cluster_reports);
}

static int fail_first_report(fwk_id_t id, unsigned int state)
{
    capture_cluster_report(id, state);
    return cluster_reports == 1 ? FWK_E_BUSY : FWK_SUCCESS;
}

void test_failed_dynamic_report_is_not_cached(void)
{
    struct mod_pd_driver_input_api input = {
        .report_power_state_transition = fail_first_report,
    };
    struct ppu_v1_pd_ctx pd = {
        .id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PPU_V1, 0),
        .pd_driver_input_api = &input,
    };
    ppu_v1_ctx.pd_ctx_table = &pd;
    queued_reports = cluster_reports = 0;
    __fwk_put_event_StubWithCallback(capture_queued_report);
    for (unsigned int i = 0; i < 2; ++i) {
        TEST_ASSERT_EQUAL(FWK_SUCCESS,
            report_power_state(&pd, MOD_PD_STATE_ON, true));
        fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
        TEST_ASSERT_EQUAL(i == 0 ? FWK_E_BUSY : FWK_SUCCESS,
            ppu_v1_process_event(&queued_report, NULL));
        TEST_ASSERT_EQUAL(i + 1, cluster_reports);
    }
}

void test_dynamic_minimum_already_awake_reports_on(void)
{
    struct mod_pd_driver_input_api input = {
        .report_power_state_transition = capture_cluster_report,
    };
    struct ppu_v1_pd_ctx pd = {
        .id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PPU_V1, 0),
        .pd_driver_input_api = &input,
    };
    ppu_v1_ctx.pd_ctx_table = &pd;
    queued_reports = cluster_reports = 0;
    __fwk_put_event_StubWithCallback(capture_queued_report);
    ppu_v1_is_power_active_edge_interrupt_ExpectAndReturn(
        &pd.ppu, PPU_V1_MODE_ON, false);
    ppu_v1_is_dyn_policy_min_interrupt_ExpectAndReturn(&pd.ppu, true);
    ppu_v1_ack_interrupt_Expect(&pd.ppu, PPU_V1_ISR_DYN_POLICY_MIN_IRQ);
    ppu_v1_interrupt_mask_Expect(&pd.ppu, PPU_V1_IMR_DYN_POLICY_MIN_IRQ_MASK);
    ppu_v1_set_input_edge_sensitivity_Expect(
        &pd.ppu, PPU_V1_MODE_ON, PPU_V1_EDGE_SENSITIVITY_RISING_EDGE);
    ppu_v1_is_power_devactive_high_ExpectAndReturn(&pd.ppu, PPU_V1_MODE_ON, true);
    ppu_v1_set_input_edge_sensitivity_Expect(
        &pd.ppu, PPU_V1_MODE_ON, PPU_V1_EDGE_SENSITIVITY_MASKED);
    ppu_v1_ack_power_active_edge_interrupt_Expect(&pd.ppu, PPU_V1_MODE_ON);
    ppu_v1_interrupt_unmask_Expect(&pd.ppu, PPU_V1_IMR_DYN_POLICY_MIN_IRQ_MASK);
    core_pd_ppu_interrupt_handler(&pd);
    TEST_ASSERT_EQUAL(1, queued_reports);
    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ppu_v1_process_event(&queued_report, NULL));
    TEST_ASSERT_EQUAL(MOD_PD_STATE_ON, captured_state);
}

void test_cluster_wake_while_edges_masked_restores_on(void)
{
    const struct mod_ppu_v1_pd_config config = { 0 };
    struct ppu_v1_cluster_pd_ctx cluster = { 0 };
    struct mod_pd_driver_input_api input = {
        .report_power_state_transition = capture_cluster_report,
    };
    struct ppu_v1_pd_ctx pd = {
        .id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PPU_V1, 0),
        .config = &config, .data = &cluster, .pd_driver_input_api = &input,
    };
    ppu_v1_ctx.pd_ctx_table = &pd;
    queued_reports = cluster_reports = 0;
    __fwk_put_event_StubWithCallback(capture_queued_report);
    ppu_v1_is_power_active_edge_interrupt_ExpectAndReturn(
        &pd.ppu, PPU_V1_MODE_ON, true);
    ppu_v1_ack_power_active_edge_interrupt_Expect(&pd.ppu, PPU_V1_MODE_ON);
    ppu_v1_get_power_mode_ExpectAndReturn(&pd.ppu, PPU_V1_MODE_ON);
    for (int mode = PPU_V1_MODE_ON; mode > 0; --mode)
        ppu_v1_is_power_devactive_high_ExpectAndReturn(&pd.ppu, mode, false);
    ppu_v1_set_input_edge_sensitivity_Expect(
        &pd.ppu, PPU_V1_MODE_ON, PPU_V1_EDGE_SENSITIVITY_MASKED);
    ppu_v1_set_power_mode_ExpectAndReturn(
        &pd.ppu, PPU_V1_MODE_OFF, NULL, FWK_SUCCESS);
    ppu_v1_set_input_edge_sensitivity_Expect(
        &pd.ppu, PPU_V1_MODE_ON, PPU_V1_EDGE_SENSITIVITY_RISING_EDGE);
    ppu_v1_is_power_devactive_high_ExpectAndReturn(&pd.ppu, PPU_V1_MODE_ON, true);
    ppu_v1_ack_power_active_edge_interrupt_Expect(&pd.ppu, PPU_V1_MODE_ON);
    ppu_v1_set_input_edge_sensitivity_Expect(
        &pd.ppu, PPU_V1_MODE_ON, PPU_V1_EDGE_SENSITIVITY_MASKED);
    ppu_v1_request_operating_mode_ExpectAndReturn(&pd.ppu, 0, FWK_SUCCESS);
    ppu_v1_set_power_mode_ExpectAndReturn(
        &pd.ppu, PPU_V1_MODE_ON, NULL, FWK_SUCCESS);
    ppu_v1_set_input_edge_sensitivity_Expect(
        &pd.ppu, PPU_V1_MODE_ON, PPU_V1_EDGE_SENSITIVITY_FALLING_EDGE);
    cluster_pd_ppu_normal_mode_int_handler(&pd);
    TEST_ASSERT_EQUAL(1, queued_reports);
    fwk_id_get_element_idx_ExpectAnyArgsAndReturn(0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ppu_v1_process_event(&queued_report, NULL));
    TEST_ASSERT_EQUAL(MOD_PD_STATE_ON, captured_state);
}


static struct ppu_v1_pd_ctx *graceful_core(void)
{
    static const struct mod_ppu_v1_pd_config config = {
        .pd_type = MOD_PD_TYPE_CORE,
        .graceful_core_off = true,
    };
    static struct mod_pd_driver_input_api input = {
        .report_power_state_transition = capture_cluster_report,
    };
    struct ppu_v1_pd_ctx *pd = &pd_table[0];

    pd->id = FWK_ID_ELEMENT(FWK_MODULE_IDX_PPU_V1, 0);
    pd->config = &config;
    pd->pd_driver_input_api = &input;
    pd->alarm_api = NULL;
    cluster_reports = queued_reports = 0;
    fwk_id_get_element_idx_IgnoreAndReturn(0);
    __fwk_put_event_StubWithCallback(capture_queued_report);
    return pd;
}

static void expect_off_mask(struct ppu_v1_pd_ctx *pd)
{
    ppu_v1_set_input_edge_sensitivity_Expect(
        &pd->ppu, PPU_V1_MODE_ON, PPU_V1_EDGE_SENSITIVITY_MASKED);
    ppu_v1_interrupt_mask_Expect(&pd->ppu, PPU_V1_IMR_DYN_POLICY_MIN_IRQ_MASK);
    ppu_v1_ack_interrupt_Expect(&pd->ppu, PPU_V1_ISR_DYN_POLICY_MIN_IRQ);
}

static void expect_static_off(struct ppu_v1_pd_ctx *pd)
{
    expect_off_mask(pd);
    ppu_v1_set_power_mode_ExpectAndReturn(
        &pd->ppu, PPU_V1_MODE_OFF, NULL, FWK_SUCCESS);
    ppu_v1_lock_off_disable_Expect(&pd->ppu);
    ppu_v1_off_unlock_Expect(&pd->ppu);
}

static void request_graceful_off(struct ppu_v1_pd_ctx *pd)
{
    expect_off_mask(pd);
    ppu_v1_get_power_mode_ExpectAndReturn(&pd->ppu, PPU_V1_MODE_ON);
    ppu_v1_lock_off_enable_Expect(&pd->ppu);
    ppu_v1_dynamic_enable_Expect(&pd->ppu, PPU_V1_MODE_OFF);
    ppu_v1_interrupt_unmask_Expect(&pd->ppu, PPU_V1_IMR_DYN_POLICY_MIN_IRQ_MASK);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        ppu_v1_core_pd_set_state(pd->id, MOD_PD_STATE_OFF));
    TEST_ASSERT_TRUE(pd->off_pending);
}

static void complete_off_irq(struct ppu_v1_pd_ctx *pd)
{
    ppu_v1_is_dyn_policy_min_interrupt_ExpectAndReturn(&pd->ppu, true);
    ppu_v1_ack_interrupt_Expect(&pd->ppu, PPU_V1_ISR_DYN_POLICY_MIN_IRQ);
    ppu_v1_get_power_mode_ExpectAndReturn(&pd->ppu, PPU_V1_MODE_OFF);
    ppu_v1_interrupt_mask_Expect(&pd->ppu, PPU_V1_IMR_DYN_POLICY_MIN_IRQ_MASK);
    ppu_v1_set_power_mode_ExpectAndReturn(
        &pd->ppu, PPU_V1_MODE_OFF, NULL, FWK_SUCCESS);
    ppu_v1_lock_off_disable_Expect(&pd->ppu);
    ppu_v1_off_unlock_Expect(&pd->ppu);
    core_pd_ppu_interrupt_handler(pd);
    TEST_ASSERT_TRUE(pd->off_pending);
}

static void expect_core_on(struct ppu_v1_pd_ctx *pd)
{
    ppu_v1_interrupt_unmask_Expect(&pd->ppu, PPU_V1_IMR_DYN_POLICY_MIN_IRQ_MASK);
    ppu_v1_set_input_edge_sensitivity_Expect(
        &pd->ppu, PPU_V1_MODE_ON, PPU_V1_EDGE_SENSITIVITY_MASKED);
    ppu_v1_set_power_mode_ExpectAndReturn(
        &pd->ppu, PPU_V1_MODE_ON, NULL, FWK_SUCCESS);
    ppu_v1_dynamic_enable_Expect(&pd->ppu, PPU_V1_MODE_OFF);
}

void test_graceful_off_waits_for_wfi_and_rejects_stale_min_irq(void)
{
    struct ppu_v1_pd_ctx *pd = graceful_core();
    request_graceful_off(pd);
    TEST_ASSERT_EQUAL(0, cluster_reports);
    TEST_ASSERT_EQUAL(0, queued_reports);

    /* A previous idle interrupt cannot complete a still-powered core. */
    ppu_v1_is_dyn_policy_min_interrupt_ExpectAndReturn(&pd->ppu, true);
    ppu_v1_ack_interrupt_Expect(&pd->ppu, PPU_V1_ISR_DYN_POLICY_MIN_IRQ);
    ppu_v1_get_power_mode_ExpectAndReturn(&pd->ppu, PPU_V1_MODE_ON);
    core_pd_ppu_interrupt_handler(pd);
    TEST_ASSERT_EQUAL(0, cluster_reports);
    TEST_ASSERT_EQUAL(0, queued_reports);

    complete_off_irq(pd);
    TEST_ASSERT_EQUAL(0, cluster_reports);
    TEST_ASSERT_EQUAL(1, queued_reports);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ppu_v1_process_event(&queued_report, NULL));
    TEST_ASSERT_FALSE(pd->off_pending);
    TEST_ASSERT_EQUAL(1, cluster_reports);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, captured_state);
}

void test_on_during_graceful_off_is_reported_after_off(void)
{
    struct ppu_v1_pd_ctx *pd = graceful_core();
    request_graceful_off(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        ppu_v1_core_pd_set_state(pd->id, MOD_PD_STATE_ON));
    TEST_ASSERT_TRUE(pd->on_pending);
    complete_off_irq(pd);
    /* Normal-queue ON must also wait while the IRQ completion is queued. */
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        ppu_v1_core_pd_set_state(pd->id, MOD_PD_STATE_ON));
    TEST_ASSERT_EQUAL(0, cluster_reports);
    expect_core_on(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ppu_v1_process_event(&queued_report, NULL));
    TEST_ASSERT_FALSE(pd->off_pending);
    TEST_ASSERT_FALSE(pd->on_pending);
    TEST_ASSERT_EQUAL(2, cluster_reports);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, captured_states[0]);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_ON, captured_states[1]);
}

void test_repeated_off_cancels_deferred_on(void)
{
    struct ppu_v1_pd_ctx *pd = graceful_core();
    request_graceful_off(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        ppu_v1_core_pd_set_state(pd->id, MOD_PD_STATE_ON));
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        ppu_v1_core_pd_set_state(pd->id, MOD_PD_STATE_OFF));
    TEST_ASSERT_FALSE(pd->on_pending);
    complete_off_irq(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ppu_v1_process_event(&queued_report, NULL));
    TEST_ASSERT_EQUAL(1, cluster_reports);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, captured_state);
}

void test_force_off_cancels_stale_completion_across_new_off(void)
{
    struct ppu_v1_pd_ctx *pd = graceful_core();
    request_graceful_off(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        ppu_v1_core_pd_set_state(pd->id, MOD_PD_STATE_ON));
    complete_off_irq(pd);
    struct fwk_event old_completion = queued_report;

    /* Recovery precedes the old ISR event, so it must invalidate that event. */
    fwk_module_is_valid_element_id_ExpectAndReturn(pd->id, true);
    fwk_id_get_module_idx_ExpectAndReturn(pd->id, FWK_MODULE_IDX_PPU_V1);
    expect_static_off(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, recovery_api.force_core_off(pd->id));
    TEST_ASSERT_FALSE(pd->off_pending);
    TEST_ASSERT_FALSE(pd->on_pending);
    TEST_ASSERT_EQUAL(1, cluster_reports);

    request_graceful_off(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        ppu_v1_core_pd_set_state(pd->id, MOD_PD_STATE_ON));
    complete_off_irq(pd);
    struct fwk_event new_completion = queued_report;
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ppu_v1_process_event(&old_completion, NULL));
    TEST_ASSERT_TRUE(pd->off_pending);
    TEST_ASSERT_TRUE(pd->on_pending);
    TEST_ASSERT_EQUAL(1, cluster_reports);
    expect_core_on(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ppu_v1_process_event(&new_completion, NULL));
    TEST_ASSERT_EQUAL(3, cluster_reports);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, captured_states[1]);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_ON, captured_states[2]);
}

void test_core_reset_uses_static_off_without_waiting_for_wfi(void)
{
    struct ppu_v1_pd_ctx *pd = graceful_core();
    expect_static_off(pd);
    expect_core_on(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ppu_v1_core_pd_reset(pd->id));
    TEST_ASSERT_FALSE(pd->off_pending);
    TEST_ASSERT_EQUAL(2, cluster_reports);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, captured_states[0]);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_ON, captured_states[1]);
}

static bool minimum_acknowledged;

static void acknowledge_minimum(struct ppu_v1_regs *ppu, unsigned int mask,
    int call_count)
{
    (void)ppu;
    (void)call_count;
    TEST_ASSERT_EQUAL(PPU_V1_ISR_DYN_POLICY_MIN_IRQ, mask);
    minimum_acknowledged = true;
}

static enum ppu_v1_mode off_during_ack(struct ppu_v1_regs *ppu, int call_count)
{
    (void)ppu;
    (void)call_count;
    TEST_ASSERT_TRUE_MESSAGE(minimum_acknowledged,
        "Sampling before ack can lose a WFI OFF interrupt");
    return PPU_V1_MODE_OFF;
}

void test_wfi_during_minimum_ack_completes_without_another_irq(void)
{
    struct ppu_v1_pd_ctx *pd = graceful_core();
    minimum_acknowledged = false;
    ppu_v1_ack_interrupt_StubWithCallback(acknowledge_minimum);
    ppu_v1_get_power_mode_StubWithCallback(off_during_ack);
    ppu_v1_set_input_edge_sensitivity_Expect(
        &pd->ppu, PPU_V1_MODE_ON, PPU_V1_EDGE_SENSITIVITY_MASKED);
    ppu_v1_interrupt_mask_Expect(&pd->ppu, PPU_V1_IMR_DYN_POLICY_MIN_IRQ_MASK);
    expect_static_off(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        ppu_v1_core_pd_set_state(pd->id, MOD_PD_STATE_OFF));
    TEST_ASSERT_FALSE(pd->off_pending);
    TEST_ASSERT_EQUAL(0, queued_reports);
    TEST_ASSERT_EQUAL(1, cluster_reports);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, captured_state);
}

void test_graceful_off_lock_survives_cluster_wake_and_idle_alarm(void)
{
    struct ppu_v1_pd_ctx *pd = graceful_core();
    struct ppu_v1_pd_ctx *cores[] = { pd };
    struct ppu_v1_cluster_pd_ctx cluster = {
        .core_count = 1, .core_pd_ctx_table = cores,
    };
    struct ppu_v1_pd_ctx cluster_pd = { .data = &cluster };
    pd->off_pending = true;
    unlock_all_cores(&cluster_pd);
    deeper_locking_alarm_callback(0);
    TEST_ASSERT_TRUE(pd->off_pending);
}


void test_graceful_off_report_failure_does_not_release_deferred_on(void)
{
    struct ppu_v1_pd_ctx *pd = graceful_core();
    struct mod_pd_driver_input_api input = {
        .report_power_state_transition = fail_first_report,
    };
    pd->pd_driver_input_api = &input;
    request_graceful_off(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        ppu_v1_core_pd_set_state(pd->id, MOD_PD_STATE_ON));
    complete_off_irq(pd);
    TEST_ASSERT_EQUAL(FWK_E_BUSY, ppu_v1_process_event(&queued_report, NULL));
    TEST_ASSERT_TRUE(pd->off_pending);
    TEST_ASSERT_TRUE(pd->on_pending);
    TEST_ASSERT_FALSE(pd->reported_state_valid);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, captured_state);
}

void test_deferred_on_failure_does_not_report_on(void)
{
    struct ppu_v1_pd_ctx *pd = graceful_core();
    request_graceful_off(pd);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        ppu_v1_core_pd_set_state(pd->id, MOD_PD_STATE_ON));
    complete_off_irq(pd);
    ppu_v1_interrupt_unmask_Expect(&pd->ppu, PPU_V1_IMR_DYN_POLICY_MIN_IRQ_MASK);
    ppu_v1_set_input_edge_sensitivity_Expect(
        &pd->ppu, PPU_V1_MODE_ON, PPU_V1_EDGE_SENSITIVITY_MASKED);
    ppu_v1_set_power_mode_ExpectAndReturn(
        &pd->ppu, PPU_V1_MODE_ON, NULL, FWK_E_TIMEOUT);
    TEST_ASSERT_EQUAL(FWK_E_TIMEOUT, ppu_v1_process_event(&queued_report, NULL));
    TEST_ASSERT_EQUAL(1, cluster_reports);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, pd->reported_state);
}

int mod_ppu_v1_test_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_graceful_off_waits_for_wfi_and_rejects_stale_min_irq);
    RUN_TEST(test_on_during_graceful_off_is_reported_after_off);
    RUN_TEST(test_repeated_off_cancels_deferred_on);
    RUN_TEST(test_force_off_cancels_stale_completion_across_new_off);
    RUN_TEST(test_core_reset_uses_static_off_without_waiting_for_wfi);
    RUN_TEST(test_wfi_during_minimum_ack_completes_without_another_irq);
    RUN_TEST(test_graceful_off_lock_survives_cluster_wake_and_idle_alarm);
    RUN_TEST(test_graceful_off_report_failure_does_not_release_deferred_on);
    RUN_TEST(test_deferred_on_failure_does_not_report_on);
    RUN_TEST(test_dynamic_reports_skip_duplicates_but_preserve_transitions);
    RUN_TEST(test_failed_dynamic_report_is_not_cached);
    RUN_TEST(test_cluster_wake_while_edges_masked_restores_on);
    RUN_TEST(test_dynamic_reports_coalesce_and_preserve_commanded_off);
    RUN_TEST(test_dynamic_minimum_already_awake_reports_on);
    RUN_TEST(test_core_off_timeout_does_not_report_off);
    RUN_TEST(test_core_on_timeout_does_not_report_on);
    RUN_TEST(test_core_success_reports_observed_transition);
    RUN_TEST(test_cluster_off_timeout_does_not_report_off);
    RUN_TEST(test_cluster_on_timeout_does_not_report_on);
    RUN_TEST(test_cluster_success_reports_observed_transition);
    RUN_TEST(test_cluster_on_arms_idle_detection_without_irq_caller);
    RUN_TEST(test_dynamic_core_lock_wait_is_bounded_when_timer_configured);
    RUN_TEST(test_suspend_poll_reports_only_observed_off);
    RUN_TEST(test_suspend_poll_timeout_does_not_report_off);
    RUN_TEST(test_suspend_poll_rearms_with_bounded_remaining_count);

    RUN_TEST(test_ppu_v1_mod_init);
    RUN_TEST(test_ppu_v1_pd_init_error);
    RUN_TEST(test_ppu_v1_pd_init);
    RUN_TEST(test_ppu_v1_core_pd_set_state_sleep);
    RUN_TEST(test_start_deeper_locking_alarm);
    RUN_TEST(test_start_deeper_locking_alarm_null_api);
    RUN_TEST(test_deeper_locking_alarm_callback);

    return UNITY_END();
}

int main(void)
{
    return mod_ppu_v1_test_main();
}
