/* SPDX-License-Identifier: BSD-3-Clause */
#include "scp_unity.h"
#include "unity.h"

#include <Mockfwk_interrupt.h>
#include <Mockfwk_module.h>
#include <Mockfwk_notification.h>
#include <internal/Mockfwk_core_internal.h>
#include UNIT_TEST_SRC

static struct mod_fmu_drain_status fake_drain;
static int get_state(bool critical, struct mod_fmu_drain_status *state)
{
    *state = fake_drain;
    return FWK_SUCCESS;
}
static const struct mod_fmu_api fake_api = { .get_drain_status = get_state };
void setUp(void)
{
    active = true;
    current_case = 0;
    stage = 1;
    notification_count = ticks = 0;
    event_queued = false;
    completion_status = FWK_SUCCESS;
    fmu_api = &fake_api;
}
void tearDown(void)
{
}
void test_ae_deadline_reports_failure_and_posts_completion(void)
{
    ticks = 499;
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    watchdog(0);
    TEST_ASSERT_EQUAL(FWK_E_TIMEOUT, completion_status);
    TEST_ASSERT_TRUE(event_queued);
}
void test_ae_failed_queue_can_retry_without_false_success(void)
{
    current_case = 2;
    fake_drain.masked = true;
    fake_drain.last_error = FWK_E_TIMEOUT;
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_E_NOMEM);
    watchdog(0);
    TEST_ASSERT_EQUAL(FWK_E_NOMEM, completion_status);
    TEST_ASSERT_FALSE(event_queued);
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    watchdog(0);
    TEST_ASSERT_TRUE(event_queued);
    TEST_ASSERT_EQUAL(FWK_E_NOMEM, completion_status);
}
void test_ae_notification_has_owned_lifetime(void)
{
    struct fwk_event event = { .id = mod_fmu_notification_id_fault };
    struct mod_fmu_fault_notification_params *p = (void *)event.params;
    *p = expected[3];
    process_notification(&event, NULL);
    p->fault.block_id = 9;
    TEST_ASSERT_EQUAL_UINT(1, notification_count);
    TEST_ASSERT_EQUAL_UINT(1, notifications[0].fault.block_id);
    active = false;
    process_notification(&event, NULL);
    TEST_ASSERT_EQUAL_UINT(1, notification_count);
}
void test_external_ram_identity_requires_detector_not_errins(void)
{
    struct mod_fmu_fault_notification_params fault = {
        .fault = { .device_idx = 8, .node_idx = 1, .block_id = 0 },
    };
    for (unsigned int kind = 0; kind < 3; kind++) {
        fault.critical = kind == 2;
        fault.fault.sm_idx = 20 + kind;
        fault.fault.injected = false;
        TEST_ASSERT_TRUE(matches_ram_fault(&fault, kind));
        fault.fault.injected = true;
        TEST_ASSERT_FALSE(matches_ram_fault(&fault, kind));
    }
    fault.fault.injected = false;
    fault.fault.block_id = 1;
    TEST_ASSERT_FALSE(matches_ram_fault(&fault, 2));
}

void test_ae_module_elements_follow_framework_initialization_contract(void)
{
    const void *api;
    const fwk_id_t module = FWK_ID_MODULE(FWK_MODULE_IDX_TEST_AE);
    const fwk_id_t element = FWK_ID_ELEMENT(FWK_MODULE_IDX_TEST_AE, 0);
    unsigned int count = 0;
    while (ae_elements[count].name != NULL)
        count++;
    TEST_ASSERT_EQUAL_UINT(2, count);
    TEST_ASSERT_NOT_NULL(module_test_ae.element_init);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, module_test_ae.init(module, count, NULL));
    /* These are the two mandatory conditions checked by
     * fwk_module_init_elements. */
    for (unsigned int index = 0; index < count; index++) {
        TEST_ASSERT_NOT_NULL(ae_elements[index].data);
        TEST_ASSERT_EQUAL(
            FWK_SUCCESS,
            module_test_ae.element_init(
                FWK_ID_ELEMENT(FWK_MODULE_IDX_TEST_AE, index),
                ae_elements[index].sub_element_count,
                ae_elements[index].data));
    }
    TEST_ASSERT_EQUAL(
        FWK_E_PARAM, module_test_ae.element_init(element, 0, NULL));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, module_test_ae.bind(element, 0));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, module_test_ae.start(element));
    TEST_ASSERT_EQUAL(
        FWK_SUCCESS,
        module_test_ae.process_bind_request(
            FWK_ID_MODULE(FWK_MODULE_IDX_INTEGRATION_TEST),
            element,
            FWK_ID_API(
                FWK_MODULE_IDX_TEST_AE, MOD_INTEGRATION_TEST_API_IDX_TEST),
            &api));
    TEST_ASSERT_EQUAL_PTR(&ram_test_api, api);
    TEST_ASSERT_EQUAL(
        FWK_SUCCESS,
        module_test_ae.process_bind_request(
            FWK_ID_MODULE(FWK_MODULE_IDX_INTEGRATION_TEST),
            FWK_ID_ELEMENT(FWK_MODULE_IDX_TEST_AE, 1),
            FWK_ID_API(
                FWK_MODULE_IDX_TEST_AE, MOD_INTEGRATION_TEST_API_IDX_TEST),
            &api));
    TEST_ASSERT_EQUAL_PTR(&hierarchy_test_api, api);
    TEST_ASSERT_EQUAL(
        FWK_E_PARAM,
        module_test_ae.element_init(
            FWK_ID_ELEMENT(FWK_MODULE_IDX_TEST_AE, 1), 0, &ram_test_api));
}

void test_hierarchy_identity_checks_each_source_once_without_origin_claim(void)
{
    for (unsigned int case_idx = 0; case_idx < 2; case_idx++) {
        unsigned int count = case_idx == 0 ? 4 : 2;
        notification_count = count;
        for (unsigned int i = 0; i < count; i++) {
            notifications[i] = hierarchy_expected[case_idx][count - 1 - i];
            /* System/NI have no origin bit; this field is not evidence. */
            notifications[i].fault.injected = true;
        }
        TEST_ASSERT_TRUE(hierarchy_identity_matches(case_idx));
        notifications[0] = notifications[1];
        TEST_ASSERT_FALSE(hierarchy_identity_matches(case_idx));
        notification_count++;
        TEST_ASSERT_FALSE(hierarchy_identity_matches(case_idx));
    }
    TEST_ASSERT_FALSE(hierarchy_identity_matches(2));
}

void test_hierarchy_deadline_reports_incomplete_notifications(void)
{
    current_case = 5;
    notification_count = 1;
    ticks = 499;
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    watchdog(0);
    TEST_ASSERT_EQUAL(FWK_E_TIMEOUT, completion_status);
    TEST_ASSERT_TRUE(event_queued);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ae_deadline_reports_failure_and_posts_completion);
    RUN_TEST(test_ae_failed_queue_can_retry_without_false_success);
    RUN_TEST(test_ae_notification_has_owned_lifetime);
    RUN_TEST(test_external_ram_identity_requires_detector_not_errins);
    RUN_TEST(test_ae_module_elements_follow_framework_initialization_contract);
    RUN_TEST(test_hierarchy_identity_checks_each_source_once_without_origin_claim);
    RUN_TEST(test_hierarchy_deadline_reports_incomplete_notifications);
    return UNITY_END();
}
