/* SPDX-License-Identifier: BSD-3-Clause */
#include "scp_unity.h"
#include "unity.h"

#include <Mockfwk_module.h>
#include <Mockfwk_notification.h>
#include <internal/Mockfwk_core_internal.h>
#include UNIT_TEST_SRC

static uint8_t count, threshold;
static bool upgrade;
static unsigned int policy_reads, criticality_calls;
static fwk_id_t policy_device;
static uint16_t policy_node;

static void check_policy(fwk_id_t device, uint16_t node)
{
    TEST_ASSERT_TRUE(fwk_id_is_equal(policy_device, device));
    TEST_ASSERT_EQUAL_UINT(policy_node, node);
}
static int fake_get_count(fwk_id_t device, uint16_t node, uint8_t *value)
{
    check_policy(device, node);
    policy_reads++;
    *value = count;
    return FWK_SUCCESS;
}
static int fake_get_threshold(fwk_id_t device, uint16_t node, uint8_t *value)
{
    check_policy(device, node);
    policy_reads++;
    *value = threshold;
    return FWK_SUCCESS;
}
static int fake_get_upgrade(fwk_id_t device, uint16_t node, bool *value)
{
    check_policy(device, node);
    policy_reads++;
    *value = upgrade;
    return FWK_SUCCESS;
}
static int fake_set_count(fwk_id_t device, uint16_t node, uint8_t value)
{
    check_policy(device, node);
    count = value;
    return FWK_SUCCESS;
}
static int fake_set_threshold(fwk_id_t device, uint16_t node, uint8_t value)
{
    check_policy(device, node);
    threshold = value;
    return FWK_SUCCESS;
}
static int fake_set_upgrade(fwk_id_t device, uint16_t node, bool value)
{
    check_policy(device, node);
    upgrade = value;
    return FWK_SUCCESS;
}
static int fake_inject(const struct mod_fmu_fault *fault)
{
    return FWK_SUCCESS;
}
static int fake_set_enabled(const struct mod_fmu_fault *fault, bool enabled)
{
    return FWK_SUCCESS;
}
static int fake_set_critical(const struct mod_fmu_fault *fault, bool critical)
{
    criticality_calls++;
    return FWK_E_SUPPORT;
}
static struct mod_fmu_api fake_api = {
    .get_count = fake_get_count, .set_count = fake_set_count,
    .get_threshold = fake_get_threshold, .set_threshold = fake_set_threshold,
    .get_upgrade_enabled = fake_get_upgrade,
    .set_upgrade_enabled = fake_set_upgrade,
    .inject = fake_inject, .set_enabled = fake_set_enabled,
    .set_critical = fake_set_critical,
};
void setUp(void)
{
    fmu_api = &fake_api;
    count = 7;
    threshold = 9;
    upgrade = false;
    policy_reads = criticality_calls = 0;
    test_fmu_setup();
}
void tearDown(void)
{
}
void test_legacy_root_upgrade_restores_original_policy_on_teardown(void)
{
    policy_device = root_fmu;
    policy_node = 1;
    TEST_ASSERT_EQUAL(FWK_PENDING, test_upgrade(STEP_START, NULL));
    TEST_ASSERT_TRUE(upgrade);
    TEST_ASSERT_EQUAL_UINT(2, threshold);
    TEST_ASSERT_EQUAL_UINT(0, count);
    /* Teardown also runs after an aborted pending test. */
    test_fmu_teardown();
    TEST_ASSERT_FALSE(upgrade);
    TEST_ASSERT_EQUAL_UINT(9, threshold);
    TEST_ASSERT_EQUAL_UINT(7, count);
    TEST_ASSERT_FALSE(saved_upgrade.valid);
    test_fmu_teardown();
    TEST_ASSERT_EQUAL_UINT(7, count);
}
void test_legacy_ni_upgrade_restores_policy_without_reconfiguring_criticality(void)
{
    struct mod_fmu_fault fault;
    policy_device = fmu1;
    policy_node = NI710AE_CL0_CR_INT - 2;
    upgrade_fault(&fault, NI710AE_CL0_CR_INT);
    TEST_ASSERT_EQUAL_UINT(policy_node, fault.node_idx);
    TEST_ASSERT_TRUE(upgrade);
    TEST_ASSERT_EQUAL_UINT(0, threshold);
    TEST_ASSERT_EQUAL_UINT(0, count);
    TEST_ASSERT_EQUAL(FWK_PENDING, inject_fault_ni710ae(
        cl0_ni710ae_fmu, NI710AE_CLUSTER_NODE_IDX, STEP_START, NULL));
    TEST_ASSERT_EQUAL_UINT(0, criticality_calls);
    upgrade_fault(&fault, NI710AE_CL0_CR_INT);
    TEST_ASSERT_EQUAL_UINT(3, policy_reads);
    test_fmu_teardown();
    TEST_ASSERT_FALSE(upgrade);
    TEST_ASSERT_EQUAL_UINT(9, threshold);
    TEST_ASSERT_EQUAL_UINT(7, count);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_legacy_root_upgrade_restores_original_policy_on_teardown);
    RUN_TEST(test_legacy_ni_upgrade_restores_policy_without_reconfiguring_criticality);
    return UNITY_END();
}
