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
    .base = (uintptr_t)&fmu_reg[0],
    .parent_cr_index = 0,
    .parent_ncr_index = 1,
};

void setUp(void)
{
    /* Clear the cluster control registers between tests */
    fwk_str_memset(fmu_reg, 0, sizeof(fmu_reg));
}

void tearDown(void)
{
    /* Do nothing */
}

void test_fmu_ni710ae_inject(void)
{
    int status;
    struct mod_fmu_fault fault = {
        .device_idx = 0,
        .node_idx = 1,
        .sm_idx = 2,
    };

    /* Set numer of records value to 1 */
    fwk_mmio_write_32((uintptr_t)fmu_reg + FMU_ERRDEVID, FWK_BIT(1));

    status = inject(&fmu_config, &fault);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

void test_fmu_ni710ae_next_fault_simple(void)
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
        (uintptr_t)fmu_reg + FMU_FIELD_ERRSTATUS(1),
        MOD_FMU_SM_SYSTEM_INPUT_ERROR << FMU_ERR_STATUS_IERR_SHIFT);

    exists = fault_peek(&fmu_config, &node);
    fault_ack(&fmu_config, &fault, node, &fault_tracked);
    TEST_ASSERT_TRUE(exists);
    TEST_ASSERT_EQUAL(1, fault.node_idx);
    TEST_ASSERT_EQUAL(MOD_FMU_SM_SYSTEM_INPUT_ERROR, fault.sm_idx);
    TEST_ASSERT_TRUE(fault_tracked);
}

void test_fmu_ni710ae_set_enabled(void)
{
    int status;
    struct mod_fmu_fault fault = {
        .device_idx = 0,
        .node_idx = 2,
        .sm_idx = 1,
    };
    uint32_t val;

    /* Set numer of records value to 1*/
    fwk_mmio_write_32((uintptr_t)fmu_reg + FMU_ERRDEVID, FWK_BIT(1));

    /* Set value to true and read back */
    status = set_enabled(&fmu_config, &fault, true);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    val = fwk_mmio_read_32(fmu_config.base + FMU_ERR_SMEN);
    TEST_ASSERT_EQUAL_HEX(0x0003FFFF, val);

    /* Set value to false and read back */
    status = set_enabled(&fmu_config, &fault, false);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    val = fwk_mmio_read_32(fmu_config.base + FMU_ERR_SMEN);
    TEST_ASSERT_EQUAL_HEX(0, val);
}

void test_fmu_ni710ae_set_critical(void)
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

    val = fwk_mmio_read_32(fmu_config.base + FMU_ERR_SMEN);
    TEST_ASSERT_EQUAL_HEX(0x0003BFFF, val);

    /* Set value to false and read back */
    status = set_critical(&fmu_config, &fault, false);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    val = fwk_mmio_read_32(fmu_config.base + FMU_ERR_SMEN);
    TEST_ASSERT_EQUAL_HEX(0x0003FFFF, val);
}

int fmu_test_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fmu_ni710ae_next_fault_simple);
    RUN_TEST(test_fmu_ni710ae_set_enabled);
    RUN_TEST(test_fmu_ni710ae_set_critical);
    RUN_TEST(test_fmu_ni710ae_inject);

    return UNITY_END();
}

#if !defined(TEST_ON_TARGET)
int main(void)
{
    return fmu_test_main();
}
#endif
