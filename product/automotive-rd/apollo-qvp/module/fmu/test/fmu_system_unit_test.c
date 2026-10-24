/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "scp_unity.h"
#include "unity.h"

#include <Mockfwk_interrupt.h>
#include <Mockfwk_module.h>

#include <fwk_macros.h>
#include <fwk_mm.h>
#include <fwk_string.h>

#include UNIT_TEST_SRC

#include <mod_fmu.h>

static uint8_t fmu_reg[64 * FWK_KIB] = { 0 };

struct mod_fmu_dev_config fmu_config = {
    .base = (uintptr_t)fmu_reg,
    .parent_cr_index = 0,
    .parent_ncr_index = 1,
};

static struct mod_fmu_ack_state ack_state;

void setUp(void)
{
    ack_state = (struct mod_fmu_ack_state){ 0 };
    /* Clear the cluster control registers between tests */
    fwk_str_memset(fmu_reg, 0, sizeof(fmu_reg));
}

void tearDown(void)
{
    /* Do nothing */
}

void test_fmu_system_inject(void)
{
    int status;
    uint32_t val;
    struct mod_fmu_fault fault = {
        .device_idx = 0,
        .node_idx = 1,
        .sm_idx = MOD_FMU_SM_SYSTEM_INPUT_ERROR,
    };

    status = inject(&fmu_config, &fault);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Assert fault has been injected */
    val = fwk_mmio_read_32((uintptr_t)fmu_reg + FMU_FIELD_ERRIMPDEF(1));
    TEST_ASSERT_BITS(
        FMU_ERRIMPDEF_IE_MASK,
        MOD_FMU_SM_SYSTEM_INPUT_ERROR << FMU_ERRIMPDEF_IE_SHIFT,
        val);
}

void test_fmu_system_next_fault_simple(void)
{
    bool exists;
    struct mod_fmu_fault fault = {
        .device_idx = 0,
        .node_idx = 1,
        .sm_idx = MOD_FMU_SM_SYSTEM_INPUT_ERROR,
    };
    bool fault_tracked = false;
    unsigned int node;

    /* Set up FMU fault response */
    fwk_mmio_write_32((uintptr_t)fmu_reg + FMU_FIELD_ERRGSR_L(0), FWK_BIT(1));
    fwk_mmio_write_32(
        (uintptr_t)fmu_reg + FMU_FIELD_ERR_STATUS(1),
        FMU_ERR_STATUS_V_MASK |
            (MOD_FMU_SM_SYSTEM_INPUT_ERROR << FMU_ERR_STATUS_IERR_SHIFT));

    exists = fault_peek(&fmu_config, false, &node);
    fault_ack(&fmu_config, &fault, node, &fault_tracked, &ack_state);
    TEST_ASSERT_TRUE(exists);
    TEST_ASSERT_EQUAL(1, fault.node_idx);
    TEST_ASSERT_EQUAL(MOD_FMU_SM_SYSTEM_INPUT_ERROR, fault.sm_idx);
    TEST_ASSERT_TRUE(fault_tracked);
}

void test_fmu_system_next_fault_upstream(void)
{
    bool exists;
    unsigned int node;

    exists = fault_peek(&fmu_config, false, &node);
    TEST_ASSERT_FALSE(exists);
}

void test_fmu_system_set_enabled(void)
{
    int status;
    bool enabled;
    struct mod_fmu_fault fault = {
        .device_idx = 0,
        .node_idx = 2,
        .sm_idx = MOD_FMU_SM_ALL,
    };

    /* Set value to true and read back */
    status = set_enabled(&fmu_config, &fault, true);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    status = get_enabled(&fmu_config, &fault, &enabled);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(true, enabled);

    /* Set value to false and read back */
    status = set_enabled(&fmu_config, &fault, false);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    status = get_enabled(&fmu_config, &fault, &enabled);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(false, enabled);
}

void test_fmu_system_set_count(void)
{
    int status;
    uint8_t count;

    status = set_count(&fmu_config, 12, 13);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    status = get_count(&fmu_config, 12, &count);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(13, count);
}

void test_fmu_system_set_threshold(void)
{
    int status;
    uint8_t count;

    status = set_threshold(&fmu_config, 34, 25);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    status = get_threshold(&fmu_config, 34, &count);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(25, count);
}

void test_fmu_system_set_upgrade_enabled(void)
{
    int status;
    bool enabled;

    /* Set value to true and read back */
    status = set_upgrade_enabled(&fmu_config, 101, true);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    status = get_upgrade_enabled(&fmu_config, 101, &enabled);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(true, enabled);

    /* Set value to false and read back */
    status = set_upgrade_enabled(&fmu_config, 101, false);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    status = get_upgrade_enabled(&fmu_config, 101, &enabled);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(false, enabled);
}

void test_system_peek_filters_severity_and_stale_gsr(void)
{
    unsigned int node;
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRGSR_L(0), 7);
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERR_STATUS(1),
                     FMU_ERR_STATUS_V_MASK);
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERR_STATUS(2),
                     FMU_ERR_STATUS_V_MASK | FMU_ERR_STATUS_CI_MASK);
    TEST_ASSERT_TRUE(fault_peek(&fmu_config, true, &node));
    TEST_ASSERT_EQUAL_UINT(2, node);
    TEST_ASSERT_TRUE(fault_peek(&fmu_config, false, &node));
    TEST_ASSERT_EQUAL_UINT(1, node);
}

void test_system_input_ack_clears_source_then_observed_status(void)
{
    struct mod_fmu_fault fault = { 0 };
    bool tracked = false;
    uint32_t status = FMU_ERR_STATUS_V_MASK | FMU_ERR_STATUS_CI_MASK |
        FMU_ERR_STATUS_IERR_ERR_IN_MASK | FMU_ERR_STATUS_IERR_ERR_IN_PARITY_MASK;
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERR_STATUS(5), status);
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRIMPDEF(5),
                     0x12000180 | FMU_ERRIMPDEF_IE_MASK);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        fault_ack(&fmu_config, &fault, 5, &tracked, &ack_state));
    TEST_ASSERT_EQUAL_UINT(0x18, fault.sm_idx);
    TEST_ASSERT_EQUAL_HEX32(0x1200018C,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_ERRIMPDEF(5)));
    TEST_ASSERT_EQUAL_HEX32(status & FMU_ERR_STATUS_W1C_MASK,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_ERR_STATUS(5)));
}

void test_system_parent_route_preserves_upgraded_noncritical_leaf(void)
{
    unsigned int node;
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRGSR_L(0), FWK_BIT(5));
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERR_STATUS(5),
                     FMU_ERR_STATUS_V_MASK | FMU_ERR_STATUS_IERR_ERR_IN_MASK);
    TEST_ASSERT_TRUE(fault_peek_source(
        &fmu_config, fmu_config.parent_ncr_index, &node));
    TEST_ASSERT_EQUAL_UINT(5, node);
    TEST_ASSERT_FALSE(fault_peek_source(
        &fmu_config, fmu_config.parent_cr_index, &node));
    TEST_ASSERT_FALSE(fault_peek_source(&fmu_config, 99, &node));
}

void test_system_empty_ancestor_ack_is_idempotent_but_empty_leaf_is_invalid(void)
{
    struct mod_fmu_fault fault = { 0 };
    bool tracked = false;
    TEST_ASSERT_EQUAL(FWK_E_DATA,
        fault_ack(&fmu_config, &fault, 1, &tracked, &ack_state));
    TEST_ASSERT_FALSE(ack_state.acknowledged);
    tracked = true;
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        fault_ack(&fmu_config, &fault, 1, &tracked, &ack_state));
    TEST_ASSERT_TRUE(ack_state.acknowledged);
    TEST_ASSERT_EQUAL_HEX32(0,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_SYS_KEY));
}

void test_system_internal_record_ack_uses_status_not_res0_ic(void)
{
    struct mod_fmu_fault fault = { 0 };
    bool tracked = false;
    uint32_t status = FMU_ERR_STATUS_V_MASK | FWK_BIT(29) | FWK_BIT(27) |
        FMU_ERR_STATUS_IERR_INC_SEQ_MASK;
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERR_STATUS(383), status);
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRIMPDEF(383), 0x70);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        fault_ack(&fmu_config, &fault, 383, &tracked, &ack_state));
    TEST_ASSERT_EQUAL_UINT(2, fault.sm_idx);
    TEST_ASSERT_EQUAL_HEX32(0x70,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_ERRIMPDEF(383)));
    TEST_ASSERT_EQUAL_HEX32(status & FMU_ERR_STATUS_W1C_MASK,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_ERR_STATUS(383)));
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_SYS_KEY, 0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        fault_ack(&fmu_config, &fault, 383, &tracked, &ack_state));
    TEST_ASSERT_EQUAL_HEX32(0,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_SYS_KEY));
}

void test_system_rejects_invalid_records_and_mechanisms_without_mmio(void)
{
    struct mod_fmu_fault fault = { .node_idx = 384, .sm_idx = 16 };
    bool enabled;
    uint8_t count;
    TEST_ASSERT_EQUAL(FWK_E_PARAM, inject(&fmu_config, &fault));
    fault.node_idx = 0;
    fault.sm_idx = 1;
    TEST_ASSERT_EQUAL(FWK_E_PARAM, inject(&fmu_config, &fault));
    fault.node_idx = 383;
    fault.sm_idx = 16;
    TEST_ASSERT_EQUAL(FWK_E_PARAM, inject(&fmu_config, &fault));
    fault.node_idx = 384;
    fault.sm_idx = MOD_FMU_SM_ALL;
    TEST_ASSERT_EQUAL(FWK_E_PARAM, get_enabled(&fmu_config, &fault, &enabled));
    TEST_ASSERT_EQUAL(FWK_E_PARAM, set_enabled(&fmu_config, &fault, true));
    TEST_ASSERT_EQUAL(FWK_E_PARAM, get_count(&fmu_config, 384, &count));
    TEST_ASSERT_EQUAL(FWK_E_PARAM, set_count(&fmu_config, 384, 1));
    TEST_ASSERT_EQUAL(FWK_E_PARAM, get_threshold(&fmu_config, 384, &count));
    TEST_ASSERT_EQUAL(FWK_E_PARAM, set_threshold(&fmu_config, 384, 1));
    TEST_ASSERT_EQUAL(FWK_E_PARAM, get_upgrade_enabled(&fmu_config, 384, &enabled));
    TEST_ASSERT_EQUAL(FWK_E_PARAM, set_upgrade_enabled(&fmu_config, 384, true));
    TEST_ASSERT_EQUAL_HEX32(0,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_SYS_KEY));
}

int fmu_test_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fmu_system_inject);
    RUN_TEST(test_fmu_system_next_fault_simple);
    RUN_TEST(test_fmu_system_next_fault_upstream);
    RUN_TEST(test_fmu_system_set_enabled);
    RUN_TEST(test_fmu_system_set_count);
    RUN_TEST(test_fmu_system_set_threshold);
    RUN_TEST(test_fmu_system_set_upgrade_enabled);
    RUN_TEST(test_system_peek_filters_severity_and_stale_gsr);
    RUN_TEST(test_system_input_ack_clears_source_then_observed_status);
    RUN_TEST(test_system_parent_route_preserves_upgraded_noncritical_leaf);
    RUN_TEST(test_system_empty_ancestor_ack_is_idempotent_but_empty_leaf_is_invalid);
    RUN_TEST(test_system_internal_record_ack_uses_status_not_res0_ic);
    RUN_TEST(test_system_rejects_invalid_records_and_mechanisms_without_mmio);

    return UNITY_END();
}

#if !defined(TEST_ON_TARGET)
int main(void)
{
    return fmu_test_main();
}
#endif
