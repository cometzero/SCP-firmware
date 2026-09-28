/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "unity.h"
#include <Mockfwk_id.h>
#include UNIT_TEST_SRC

static struct mod_scmi_system_power_config config;
static fwk_id_t subscriptions[2];
static unsigned int notified;

static void capture_notify(fwk_id_t service_id, int protocol_id,
    int message_id, const void *payload, size_t size)
{
    const struct scmi_sys_power_state_notifier *notification = payload;
    TEST_ASSERT_EQUAL(subscriptions[1].value, service_id.value);
    TEST_ASSERT_EQUAL(MOD_SCMI_PROTOCOL_ID_SYS_POWER, protocol_id);
    TEST_ASSERT_EQUAL(SCMI_SYS_POWER_STATE_SET_NOTIFY, message_id);
    TEST_ASSERT_EQUAL(sizeof(*notification), size);
    TEST_ASSERT_EQUAL(0, notification->agent_id);
    TEST_ASSERT_EQUAL(0, notification->flags);
    TEST_ASSERT_EQUAL(SCMI_SYSTEM_STATE_WARM_RESET, notification->system_state);
    notified++;
}

void setUp(void)
{
    static const struct mod_scmi_from_protocol_api scmi_api = {
        .notify = capture_notify,
    };
    config.platform_notification_id = FWK_ID_MODULE(7);
    scmi_sys_power_ctx.config = &config;
    scmi_sys_power_ctx.scmi_api = &scmi_api;
    scmi_sys_power_ctx.agent_count = 2;
    scmi_sys_power_ctx.system_power_notifications = subscriptions;
    subscriptions[0] = FWK_ID_NONE;
    subscriptions[1] = FWK_ID_ELEMENT(FWK_MODULE_IDX_SCMI, 1);
    notified = 0;
}

void tearDown(void) { Mockfwk_id_Verify(); }

static void test_subscribed_notification_uses_platform_identity(void)
{
    fwk_id_is_equal_ExpectAndReturn(subscriptions[0], FWK_ID_NONE, true);
    fwk_id_is_equal_ExpectAndReturn(subscriptions[1], FWK_ID_NONE, false);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, platform_notify_warm_reset());
    TEST_ASSERT_EQUAL(1, notified);
}

static void test_no_subscriber_returns_error(void)
{
    fwk_id_is_equal_ExpectAndReturn(subscriptions[0], FWK_ID_NONE, true);
    fwk_id_is_equal_ExpectAndReturn(subscriptions[1], FWK_ID_NONE, true);
    TEST_ASSERT_EQUAL(FWK_E_STATE, platform_notify_warm_reset());
    TEST_ASSERT_EQUAL(0, notified);
}

static void test_platform_binding_is_restricted(void)
{
    const void *api = NULL;
    fwk_id_t id = FWK_ID_API(FWK_MODULE_IDX_SCMI_SYSTEM_POWER,
        MOD_SCMI_SYSTEM_POWER_API_IDX_PLATFORM);
    fwk_id_t owner = config.platform_notification_id;

    fwk_id_get_api_idx_ExpectAndReturn(id, MOD_SCMI_SYSTEM_POWER_API_IDX_PLATFORM);
    fwk_id_is_equal_ExpectAndReturn(owner, FWK_ID_NONE, false);
    fwk_id_is_equal_ExpectAndReturn(owner, owner, true);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        scmi_sys_power_process_bind_request(owner, FWK_ID_NONE, id, &api));
    TEST_ASSERT_EQUAL_PTR(&platform_api, api);

    fwk_id_get_api_idx_ExpectAndReturn(id, MOD_SCMI_SYSTEM_POWER_API_IDX_PLATFORM);
    fwk_id_is_equal_ExpectAndReturn(owner, FWK_ID_NONE, false);
    fwk_id_is_equal_ExpectAndReturn(FWK_ID_NONE, owner, false);
    TEST_ASSERT_EQUAL(FWK_E_ACCESS,
        scmi_sys_power_process_bind_request(FWK_ID_NONE, FWK_ID_NONE, id, &api));

    config.platform_notification_id = FWK_ID_NONE;
    fwk_id_get_api_idx_ExpectAndReturn(id, MOD_SCMI_SYSTEM_POWER_API_IDX_PLATFORM);
    fwk_id_is_equal_ExpectAndReturn(FWK_ID_NONE, FWK_ID_NONE, true);
    TEST_ASSERT_EQUAL(FWK_E_ACCESS,
        scmi_sys_power_process_bind_request(owner, FWK_ID_NONE, id, &api));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_subscribed_notification_uses_platform_identity);
    RUN_TEST(test_no_subscriber_returns_error);
    RUN_TEST(test_platform_binding_is_restricted);
    return UNITY_END();
}
