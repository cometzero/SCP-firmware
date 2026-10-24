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
#include <mod_timer.h>

static uint8_t fmu_reg[64 * FWK_KIB] = { 0 };

struct mod_fmu_dev_config fmu_config = {
    .base = (uintptr_t)&fmu_reg[0],
    .parent_cr_index = 0,
    .parent_ncr_index = 1,
};

static unsigned int wait_calls;
static unsigned int wait_fail_call;

int wait(
    fwk_id_t dev_id,
    uint32_t microseconds,
    bool (*cond)(void *),
    void *data)
{
    if (++wait_calls == wait_fail_call) {
        return FWK_E_TIMEOUT;
    }
    return cond(data) ? FWK_SUCCESS : FWK_E_DATA;
}

struct mod_timer_api timer_api_impl = {
    .wait = wait,
};

static struct mod_fmu_ack_state ack_state;

void setUp(void)
{
    ack_state = (struct mod_fmu_ack_state){ 0 };
    wait_calls = wait_fail_call = 0;
    /* Clear the cluster control registers between tests */
    fwk_str_memset(fmu_reg, 0, sizeof(fmu_reg));
    timer_api = &timer_api_impl;
}

void tearDown(void)
{
    /* Do nothing */
}

void test_fmu_gic_inject(void)
{
    int status;
    uint32_t val;
    struct mod_fmu_fault fault = {
        .device_idx = 0,
        .node_idx = 1,
        .sm_idx = 2,
    };

    status = inject(&fmu_config, &fault);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Assert fault has been injected */
    val = fwk_mmio_read_32((uintptr_t)fmu_reg + FMU_FIELD_SMERR);
    TEST_ASSERT_EQUAL_HEX(0x10000200, val);
}

void test_fmu_gic_next_fault_simple(void)
{
    bool exists;
    struct mod_fmu_fault fault = {
        .device_idx = 0,
        .node_idx = 1,
        .sm_idx = 2,
    };
    bool fault_tracked = false;
    unsigned int node;

    /* Set up FMU fault response */
    fwk_mmio_write_64(fmu_config.base + FMU_FIELD_ERRGSR, FWK_BIT(3));
    fwk_mmio_write_64(
        fmu_config.base + FMU_FIELD_ERRSTATUS(3),
        (2UL << FMU_ERRSTATUS_SMID_SHIFT) | FMU_ERRSTATUS_V_MASK |
            FMU_ERRSTATUS_UE_MASK);

    exists = fault_peek(&fmu_config, false, &node);
    fault_ack(&fmu_config, &fault, node, &fault_tracked, &ack_state);
    TEST_ASSERT_TRUE(exists);
    TEST_ASSERT_EQUAL(3, node);
    TEST_ASSERT_EQUAL(1, fault.node_idx);
    TEST_ASSERT_EQUAL(2, fault.sm_idx);
    TEST_ASSERT_TRUE(fault_tracked);
    TEST_ASSERT_EQUAL_HEX64(
        FMU_ERRSTATUS_V_MASK | FMU_ERRSTATUS_UE_MASK,
        fwk_mmio_read_64(fmu_config.base + FMU_FIELD_ERRSTATUS(3)));
}

void test_fmu_ack_overflow_refreshes_record_pair(void)
{
    struct mod_fmu_fault fault = { 0 };
    bool tracked = false;
    uint64_t clear = FMU_ERRSTATUS_V_MASK | FMU_ERRSTATUS_UE_MASK |
        FMU_ERRSTATUS_OF_MASK;

    fwk_mmio_write_64(fmu_config.base + FMU_FIELD_ERRGSR, FWK_BIT(4));
    fwk_mmio_write_64(fmu_config.base + FMU_FIELD_ERRSTATUS(4),
        clear | FMU_STATUS_OFX_MASK | (10UL << FMU_ERRSTATUS_SMID_SHIFT));
    fault_ack(&fmu_config, &fault, 4, &tracked, &ack_state);
    TEST_ASSERT_TRUE(tracked);
    TEST_ASSERT_EQUAL(2, fault.node_idx);
    TEST_ASSERT_EQUAL(10, fault.sm_idx);
    TEST_ASSERT_EQUAL_HEX64(clear,
        fwk_mmio_read_64(fmu_config.base + FMU_FIELD_ERRSTATUS(4)));
    TEST_ASSERT_EQUAL_HEX32(0x20000004,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_ERRUPDATE));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_device_update(&fmu_config, 5));
    TEST_ASSERT_EQUAL_HEX32(0x20000002,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_ERRUPDATE));
}

void test_fmu_busy_wait_failure_is_reported(void)
{
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_STATUS, FMU_STATUS_BUSY_MASK);
    TEST_ASSERT_EQUAL(FWK_E_DATA, fmu_device_wait_busy(&fmu_config));
}

void test_fmu_bind_enables_discovered_interrupts(void)
{
    fwk_id_t id = FWK_ID_ELEMENT(FWK_MODULE_IDX_FMU, 0);

    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRDEVID, 6);
    for (unsigned int record = 0; record < 6; record++) {
        fwk_mmio_write_64(fmu_config.base + FMU_FIELD_ERRCTLR(record), 3);
    }
    fwk_module_bind_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_module_get_data_ExpectAndReturn(id, &fmu_config);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, bind(id));
    for (unsigned int record = 0; record < 6; record++) {
        TEST_ASSERT_EQUAL_HEX64(record % 2 == 0 ? 0x2003 : 7,
            fwk_mmio_read_64(fmu_config.base + FMU_FIELD_ERRCTLR(record)));
    }
    TEST_ASSERT_EQUAL_HEX64(0,
        fwk_mmio_read_64(fmu_config.base + FMU_FIELD_ERRCTLR(6)));
}

void test_fmu_gic_set_enabled(void)
{
    int status;
    struct mod_fmu_fault fault = {
        .device_idx = 0,
        .node_idx = 2,
        .sm_idx = 1,
    };
    uint32_t val;

    /* Set value to true and read back */
    status = set_enabled(&fmu_config, &fault, true);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    val = fwk_mmio_read_32(fmu_config.base + FMU_FIELD_SMEN);
    TEST_ASSERT_EQUAL_HEX(0x20000101, val);

    /* Set value to false and read back */
    status = set_enabled(&fmu_config, &fault, false);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    val = fwk_mmio_read_32(fmu_config.base + FMU_FIELD_SMEN);
    TEST_ASSERT_EQUAL_HEX(0x20000100, val);
}

void test_fmu_gic_set_critical(void)
{
    int status;
    struct mod_fmu_fault fault = {
        .device_idx = 0,
        .node_idx = 2,
        .sm_idx = 1,
    };
    uint32_t val;

    /* Set value to true and read back */
    status = set_critical(&fmu_config, &fault, true);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    val = fwk_mmio_read_32(fmu_config.base + FMU_FIELD_SMCR);
    TEST_ASSERT_EQUAL_HEX(0x20000101, val);

    /* Set value to false and read back */
    status = set_critical(&fmu_config, &fault, false);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    val = fwk_mmio_read_32(fmu_config.base + FMU_FIELD_SMCR);
    TEST_ASSERT_EQUAL_HEX(0x20000100, val);
}

void test_fmu_mixed_criticality_and_record_token(void)
{
    unsigned int record;
    bool tracked = false;
    struct mod_fmu_fault fault = { 0 };
    fwk_mmio_write_64(fmu_config.base + FMU_FIELD_ERRGSR, 3);
    fwk_mmio_write_64(fmu_config.base + FMU_FIELD_ERRSTATUS(0),
        FMU_ERRSTATUS_V_MASK | (1UL << FMU_ERRSTATUS_SMID_SHIFT));
    fwk_mmio_write_64(fmu_config.base + FMU_FIELD_ERRSTATUS(1),
        FMU_ERRSTATUS_V_MASK | FMU_ERRSTATUS_INJ_MASK |
        (2UL << FMU_ERRSTATUS_SMID_SHIFT) |
        (UINT64_C(1) << FMU_ERRSTATUS_BLKID_SHIFT));
    TEST_ASSERT_TRUE(fault_peek(&fmu_config, false, &record));
    TEST_ASSERT_EQUAL_UINT(1, record);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        fault_ack(&fmu_config, &fault, record, &tracked, &ack_state));
    TEST_ASSERT_EQUAL_UINT(2, fault.sm_idx);
    TEST_ASSERT_EQUAL_UINT(1, fault.block_id);
    TEST_ASSERT_TRUE(fault.injected);
    TEST_ASSERT_TRUE(fault_peek(&fmu_config, true, &record));
    TEST_ASSERT_EQUAL_UINT(0, record);
}

void test_fmu_ack_failure_does_not_mark_fault_tracked(void)
{
    struct mod_fmu_fault fault = { 0 };
    bool tracked = false;
    fwk_mmio_write_64(fmu_config.base + FMU_FIELD_ERRSTATUS(0),
                     FMU_ERRSTATUS_V_MASK);
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_STATUS,
                     FMU_STATUS_TIMEOUT_MASK);
    TEST_ASSERT_EQUAL(FWK_E_TIMEOUT,
        fault_ack(&fmu_config, &fault, 0, &tracked, &ack_state));
    TEST_ASSERT_FALSE(tracked);
}

void test_gic_mhu_upgraded_parent_source_selects_noncritical_leaf(void)
{
    unsigned int record;
    struct mod_fmu_dev_config config = fmu_config;
    const unsigned int parent_pins[][2] = { { 204, 203 }, { 4, 5 } };
    fwk_mmio_write_64(config.base + FMU_FIELD_ERRGSR, FWK_BIT(1));
    fwk_mmio_write_64(config.base + FMU_FIELD_ERRSTATUS(1),
        FMU_ERRSTATUS_V_MASK | (17UL << FMU_ERRSTATUS_SMID_SHIFT));
    TEST_ASSERT_NOT_NULL(mod_fmu_gic_mhu_api.fault_peek_source);
    for (unsigned int index = 0; index < 2; index++) {
        config.parent_cr_index = parent_pins[index][0];
        config.parent_ncr_index = parent_pins[index][1];
        TEST_ASSERT_FALSE(fault_peek(&config, true, &record));
        TEST_ASSERT_TRUE(mod_fmu_gic_mhu_api.fault_peek_source(
            &config, config.parent_ncr_index, &record));
        TEST_ASSERT_EQUAL_UINT(1, record);
        TEST_ASSERT_FALSE(mod_fmu_gic_mhu_api.fault_peek_source(
            &config, config.parent_cr_index, &record));
        TEST_ASSERT_FALSE(mod_fmu_gic_mhu_api.fault_peek_source(
            &config, 99, &record));
    }
}

void test_fmu_protection_id_preserves_block_and_rejects_truncation(void)
{
    struct mod_fmu_fault fault = { .node_idx = 2, .sm_idx = 1, .block_id = 1 };
    TEST_ASSERT_EQUAL(FWK_SUCCESS, inject(&fmu_config, &fault));
    TEST_ASSERT_EQUAL_HEX32(0x20010100,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_SMERR));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, set_enabled(&fmu_config, &fault, true));
    TEST_ASSERT_EQUAL_HEX32(0x20010101,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_SMEN));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, set_critical(&fmu_config, &fault, true));
    TEST_ASSERT_EQUAL_HEX32(0x20010101,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_SMCR));
    fault.block_id = 0x1000;
    TEST_ASSERT_EQUAL(FWK_E_PARAM, inject(&fmu_config, &fault));
    fault.block_id = 0;
    fault.node_idx = 8;
    TEST_ASSERT_EQUAL(FWK_E_PARAM, set_enabled(&fmu_config, &fault, true));
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_STATUS,
                     FMU_STATUS_UNEXP_RESP_MASK);
    TEST_ASSERT_EQUAL(FWK_E_DEVICE, fmu_device_wait_busy(&fmu_config));
}

void test_fmu_refresh_failure_preserves_identity_without_reack(void)
{
    struct mod_fmu_fault fault = { 0 };
    bool tracked = false;
    fwk_mmio_write_64(fmu_config.base + FMU_FIELD_ERRSTATUS(6),
        FMU_ERRSTATUS_V_MASK | FMU_STATUS_OFX_MASK | FMU_ERRSTATUS_INJ_MASK |
        (UINT64_C(1) << FMU_ERRSTATUS_BLKID_SHIFT) |
        (3UL << FMU_ERRSTATUS_SMID_SHIFT));
    wait_fail_call = 2; /* W1C succeeds; ERRUPDATE fails. */
    TEST_ASSERT_EQUAL(FWK_E_TIMEOUT,
        fault_ack(&fmu_config, &fault, 6, &tracked, &ack_state));
    TEST_ASSERT_TRUE(ack_state.acknowledged);
    TEST_ASSERT_TRUE(ack_state.refresh_pending);
    TEST_ASSERT_TRUE(tracked);
    TEST_ASSERT_EQUAL_UINT(3, fault.node_idx);
    TEST_ASSERT_EQUAL_UINT(1, fault.block_id);
    TEST_ASSERT_EQUAL_UINT(3, fault.sm_idx);
    TEST_ASSERT_TRUE(fault.injected);
    /* Simulate the cleared hardware slot. A retry must not W1C it again. */
    fwk_mmio_write_64(fmu_config.base + FMU_FIELD_ERRSTATUS(6), 0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        fault_ack(&fmu_config, &fault, 6, &tracked, &ack_state));
    TEST_ASSERT_EQUAL_HEX64(0,
        fwk_mmio_read_64(fmu_config.base + FMU_FIELD_ERRSTATUS(6)));
    TEST_ASSERT_FALSE(ack_state.refresh_pending);
    TEST_ASSERT_EQUAL_UINT(3, wait_calls);
    TEST_ASSERT_EQUAL_UINT(1, fault.block_id);
    TEST_ASSERT_EQUAL_HEX32(0x30000004,
        fwk_mmio_read_32(fmu_config.base + FMU_FIELD_ERRUPDATE));
}

int fmu_test_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fmu_refresh_failure_preserves_identity_without_reack);
    RUN_TEST(test_fmu_mixed_criticality_and_record_token);
    RUN_TEST(test_gic_mhu_upgraded_parent_source_selects_noncritical_leaf);
    RUN_TEST(test_fmu_ack_failure_does_not_mark_fault_tracked);
    RUN_TEST(test_fmu_protection_id_preserves_block_and_rejects_truncation);
    RUN_TEST(test_fmu_gic_inject);
    RUN_TEST(test_fmu_gic_next_fault_simple);
    RUN_TEST(test_fmu_ack_overflow_refreshes_record_pair);
    RUN_TEST(test_fmu_busy_wait_failure_is_reported);
    RUN_TEST(test_fmu_bind_enables_discovered_interrupts);
    RUN_TEST(test_fmu_gic_set_enabled);
    RUN_TEST(test_fmu_gic_set_critical);

    return UNITY_END();
}

#if !defined(TEST_ON_TARGET)
int main(void)
{
    return fmu_test_main();
}
#endif
