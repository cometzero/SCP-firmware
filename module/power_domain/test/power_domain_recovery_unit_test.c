/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "unity.h"

#include <Mockfwk_module.h>
#include <Mockfwk_notification.h>
#include <internal/Mockfwk_core_internal.h>

#include UNIT_TEST_SRC
#include "power_domain_state_checks.c"
#include "power_domain_notifications.c"

enum { CORE, CLUSTER, DOMAIN_COUNT };
static struct pd_ctx domains[DOMAIN_COUNT];
static struct fwk_event events[32];
static unsigned int head, tail, driver_calls[DOMAIN_COUNT];
static unsigned int notifications[8], notification_count;
static fwk_id_t dispatch_target;
static bool subscriber;

static int enqueue(struct fwk_event *event, int call_count)
{
    (void)call_count;
    TEST_ASSERT_LESS_THAN(FWK_ARRAY_SIZE(events), tail);
    events[tail++] = *event;
    return FWK_SUCCESS;
}

static int notify(struct fwk_event *event, unsigned int *count, int call_count)
{
    (void)call_count;
    *count = 0;
    if (!subscriber || fwk_id_get_element_idx(dispatch_target) != CORE ||
        !fwk_id_is_equal(event->id, mod_pd_notification_id_power_state_transition))
        return FWK_SUCCESS;

    const struct mod_pd_power_state_transition_notification_params *params =
        (const void *)event->params;
    TEST_ASSERT_LESS_THAN(FWK_ARRAY_SIZE(notifications), notification_count);
    notifications[notification_count++] = params->state;
    struct fwk_event response = {
        .id = event->id,
        .target_id = dispatch_target,
        .is_response = true,
    };
    *count = 1;
    return enqueue(&response, 0);
}

static int driver_set_state(fwk_id_t id, unsigned int state)
{
    unsigned int index = fwk_id_get_element_idx(id);
    driver_calls[index]++;
    return pd_driver_input_api.report_power_state_transition(
        domains[index].id, state);
}

void setUp(void)
{
    static const uint32_t allowed[] = {
        [MOD_PD_STATE_OFF] = 1U << MOD_PD_STATE_OFF,
        [MOD_PD_STATE_ON] = (1U << MOD_PD_STATE_OFF) | (1U << MOD_PD_STATE_ON),
    };
    static struct mod_power_domain_config config = { 0 };
    static const struct mod_power_domain_element_config element[] = {
        [CORE] = { .attributes.pd_type = MOD_PD_TYPE_CORE,
            .parent_idx = CLUSTER, .allowed_state_mask_table = allowed,
            .allowed_state_mask_table_size = FWK_ARRAY_SIZE(allowed) },
        [CLUSTER] = { .attributes.pd_type = MOD_PD_TYPE_CLUSTER,
            .parent_idx = DOMAIN_COUNT, .allowed_state_mask_table = allowed,
            .allowed_state_mask_table_size = FWK_ARRAY_SIZE(allowed) },
    };
    static struct mod_pd_driver_api driver = { .set_state = driver_set_state };

    Mockfwk_module_Init();
    Mockfwk_notification_Init();
    Mockfwk_core_internal_Init();
    fwk_module_is_valid_element_id_IgnoreAndReturn(true);
    fwk_module_get_element_name_IgnoreAndReturn("test-domain");
    __fwk_put_event_StubWithCallback(enqueue);
    fwk_notification_notify_StubWithCallback(notify);
    memset(&mod_pd_ctx, 0, sizeof(mod_pd_ctx));
    memset(domains, 0, sizeof(domains));
    memset(driver_calls, 0, sizeof(driver_calls));
    head = tail = notification_count = 0;
    subscriber = true;
    mod_pd_ctx.config = &config;
    mod_pd_ctx.pd_ctx_table = domains;
    mod_pd_ctx.pd_count = DOMAIN_COUNT;
    mod_pd_ctx.system_pd_ctx = &domains[CLUSTER];
    mod_pd_ctx.system_suspend.state = MOD_PD_STATE_ON;
    for (unsigned int i = 0; i < DOMAIN_COUNT; i++) {
        TEST_ASSERT_EQUAL(FWK_SUCCESS, pd_power_domain_init(
            FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, i), 0, &element[i]));
        domains[i].driver_id = domains[i].id;
        domains[i].driver_api = &driver;
        domains[i].current_state = domains[i].requested_state =
            domains[i].state_requested_to_driver = domains[i].driver_state =
                MOD_PD_STATE_ON;
    }
    TEST_ASSERT_EQUAL(FWK_SUCCESS, connect_pd_tree());
}

void tearDown(void)
{
    Mockfwk_module_Verify();
    Mockfwk_notification_Verify();
    Mockfwk_core_internal_Verify();
}

static void dispatch_one(void)
{
    struct fwk_event response = { 0 };
    TEST_ASSERT_LESS_THAN(tail, head);
    struct fwk_event event = events[head++];
    dispatch_target = event.target_id;
    int status = fwk_id_is_type(event.id, FWK_ID_TYPE_NOTIFICATION) ?
        module_power_domain.process_notification(&event, &response) :
        module_power_domain.process_event(&event, &response);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

static void drain(void)
{
    while (head < tail)
        dispatch_one();
}

static void request_and_force_off(void)
{
    TEST_ASSERT_EQUAL(FWK_SUCCESS, pd_restricted_api.set_state(domains[CORE].id,
        false, MOD_PD_COMPOSITE_STATE(MOD_PD_LEVEL_1, 0, 0,
            MOD_PD_STATE_OFF, MOD_PD_STATE_OFF)));
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        pd_driver_input_api.report_power_state_transition(
            domains[CORE].id, MOD_PD_STATE_OFF));
}

void test_duplicate_off_response_completes_parent_off(void)
{
    request_and_force_off();
    dispatch_one(); /* Request queues the driver's duplicate OFF report. */
    dispatch_one(); /* Forced report starts the notification sequence. */
    dispatch_one(); /* Duplicate report precedes the notification response. */
    TEST_ASSERT_EQUAL(1,
        domains[CORE].power_state_transition_notification_ctx.pending_responses);
    unsigned int origin =
        domains[CORE].power_state_transition_notification_ctx.previous_state;
    drain();
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, domains[CLUSTER].current_state);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_ON, origin);
    TEST_ASSERT_EQUAL(1, driver_calls[CLUSTER]);
    TEST_ASSERT_EQUAL(1, notification_count);
}

void test_forced_off_without_subscriber_completes_parent_off(void)
{
    subscriber = false;
    request_and_force_off();
    drain();
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, domains[CLUSTER].current_state);
    TEST_ASSERT_EQUAL(1, driver_calls[CLUSTER]);
    TEST_ASSERT_EQUAL(0, notification_count);
}

void test_off_then_on_while_notification_pending_keeps_parent_on(void)
{
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        pd_driver_input_api.report_power_state_transition(
            domains[CORE].id, MOD_PD_STATE_OFF));
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        pd_driver_input_api.report_power_state_transition(
            domains[CORE].id, MOD_PD_STATE_ON));
    dispatch_one();
    dispatch_one();
    TEST_ASSERT_EQUAL(MOD_PD_STATE_ON,
        domains[CORE].power_state_transition_notification_ctx.previous_state);
    drain();
    TEST_ASSERT_EQUAL(MOD_PD_STATE_ON, domains[CORE].current_state);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_ON, domains[CLUSTER].current_state);
    TEST_ASSERT_EQUAL(0, driver_calls[CLUSTER]);
    TEST_ASSERT_EQUAL(2, notification_count);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_OFF, notifications[0]);
    TEST_ASSERT_EQUAL(MOD_PD_STATE_ON, notifications[1]);
    TEST_ASSERT_EQUAL(0,
        domains[CORE].power_state_transition_notification_ctx.pending_responses);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_duplicate_off_response_completes_parent_off);
    RUN_TEST(test_forced_off_without_subscriber_completes_parent_off);
    RUN_TEST(test_off_then_on_while_notification_pending_keeps_parent_on);
    return UNITY_END();
}
