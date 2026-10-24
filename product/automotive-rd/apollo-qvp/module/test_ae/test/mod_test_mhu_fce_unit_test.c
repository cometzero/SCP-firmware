/* SPDX-License-Identifier: BSD-3-Clause */
#include "scp_unity.h"
#include "unity.h"

#include <Mockfwk_interrupt.h>
#include <Mockfwk_module.h>
#include <internal/Mockfwk_core_internal.h>
#include UNIT_TEST_SRC

static uint32_t rx, tx;
void setUp(void)
{
    rx = 0x320AE001;
    tx = 0;
    receive_address.local_view_address = (uintptr_t)&rx;
    transmit_address.local_view_address = (uintptr_t)&tx;
    active = true;
    accepted = ticks = 0;
    event_queued = false;
    completion_status = FWK_SUCCESS;
}
void tearDown(void)
{
}
void test_receive_acknowledges_and_replies_once(void)
{
    receive(0);
    TEST_ASSERT_FALSE(active);
    TEST_ASSERT_EQUAL_UINT(1, accepted);
    TEST_ASSERT_EQUAL_HEX32(rx ^ UINT32_MAX, tx);
    receive(0);
    TEST_ASSERT_EQUAL_UINT(1, accepted);
}
void test_missing_payload_completes_with_timeout(void)
{
    ticks = 299;
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    watchdog(0);
    TEST_ASSERT_EQUAL(FWK_E_TIMEOUT, completion_status);
    TEST_ASSERT_FALSE(active);
    TEST_ASSERT_TRUE(event_queued);
}
void test_queue_exhaustion_is_reported_and_retried(void)
{
    receive(0);
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_E_NOMEM);
    watchdog(0);
    TEST_ASSERT_EQUAL(FWK_E_NOMEM, completion_status);
    TEST_ASSERT_FALSE(event_queued);
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    watchdog(0);
    TEST_ASSERT_TRUE(event_queued);
    TEST_ASSERT_EQUAL(FWK_E_NOMEM, completion_status);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_receive_acknowledges_and_replies_once);
    RUN_TEST(test_missing_payload_completes_with_timeout);
    RUN_TEST(test_queue_exhaustion_is_reported_and_retried);
    return UNITY_END();
}
