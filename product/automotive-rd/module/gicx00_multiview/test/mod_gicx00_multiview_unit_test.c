/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fwk_module_idx.h"
#include "internal/gicx00_multiview_reg.h"

#include <Mockfwk_module.h>
#include <unity.h>

#include <fwk_mmio.h>

#include <stdint.h>
#include <string.h>

#include UNIT_TEST_SRC

static uint32_t gicr_registers[16];
static uint32_t gicd_registers[0x400 / sizeof(uint32_t)];

static const struct mod_gicx00_multiview_redistributor_map
    redistributor_map[] = {
        {
            .gicr_base = (uintptr_t)gicr_registers,
            .view = MOD_GICX00_MULTIVIEW_VIEW_1,
        },
    };

static const struct mod_gicx00_multiview_config config = {
    .gicd_base = (uintptr_t)gicd_registers,
    .redistributor_map = redistributor_map,
    .redistributor_map_count = FWK_ARRAY_SIZE(redistributor_map),
};

static const fwk_id_t gic_id =
    FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_GICX00_MULTIVIEW, 0);

void setUp(void)
{
    memset(gicr_registers, 0, sizeof(gicr_registers));
    memset(gicd_registers, 0, sizeof(gicd_registers));
}

void tearDown(void)
{
}

static void expect_config(void)
{
    fwk_module_is_valid_element_id_ExpectAndReturn(gic_id, true);
    fwk_module_get_data_ExpectAndReturn(gic_id, &config);
}

void test_power_state_round_trip(void)
{
    int status;
    enum mod_gicx00_multiview_redistributor_power_state state;

    expect_config();
    status = set_redistributor_power_state(
        gic_id, 0, MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_OFF);
    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL_HEX32(
        GICR_PWRR_RDPD,
        fwk_mmio_read_32((uintptr_t)gicr_registers + GICR_PWRR));

    expect_config();
    status = read_redistributor_power_state(gic_id, 0, &state);
    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL_INT(
        MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_OFF, state);

    expect_config();
    status = set_redistributor_power_state(
        gic_id, 0, MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_ON);
    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);

    expect_config();
    status = read_redistributor_power_state(gic_id, 0, &state);
    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL_INT(
        MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_ON, state);
}

void test_power_group_transition_times_out(void)
{
    int status;

    fwk_mmio_write_32((uintptr_t)gicr_registers + GICR_PWRR, GICR_PWRR_RDGPD);
    expect_config();
    status = set_redistributor_power_state(
        gic_id, 0, MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_ON);
    TEST_ASSERT_EQUAL_INT(FWK_E_TIMEOUT, status);
}

void test_invalid_power_state_is_rejected(void)
{
    int status = set_redistributor_power_state(
        gic_id, 0, MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_COUNT);

    TEST_ASSERT_EQUAL_INT(FWK_E_PARAM, status);
}

void test_invalid_redistributor_is_rejected(void)
{
    int status;

    expect_config();
    status = set_redistributor_power_state(
        gic_id, 1, MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_ON);
    TEST_ASSERT_EQUAL_INT(FWK_E_PARAM, status);
}

void test_processor_sleep_times_out_without_children_asleep(void)
{
    int status;

    expect_config();
    status = set_processor_sleep(gic_id, 0, true);
    TEST_ASSERT_EQUAL_INT(FWK_E_TIMEOUT, status);
    TEST_ASSERT_BITS_HIGH(
        GICR_WAKER_PROCESSOR_SLEEP,
        fwk_mmio_read_32((uintptr_t)gicr_registers + GICR_WAKER));
}

void test_processor_awake_state_completes(void)
{
    int status;

    expect_config();
    status = set_processor_sleep(gic_id, 0, false);
    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
}

void test_interrupt_state_reports_real_pending_and_active_readback(void)
{
    int status;
    struct mod_gicx00_multiview_interrupt_state observed;
    const unsigned int interrupt_id = 991U;
    const uint32_t mask = UINT32_C(1) << (interrupt_id % 32U);
    const uintptr_t pending_address = (uintptr_t)gicd_registers + 0x0200U +
        ((interrupt_id / 32U) * sizeof(uint32_t));
    const uintptr_t active_address = (uintptr_t)gicd_registers + 0x0300U +
        ((interrupt_id / 32U) * sizeof(uint32_t));

    fwk_mmio_write_32(pending_address, mask | UINT32_C(0x5A));
    fwk_mmio_write_32(active_address, mask | UINT32_C(0xA5));

    expect_config();
    status = read_interrupt_state(gic_id, interrupt_id, &observed);

    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
    TEST_ASSERT_TRUE(observed.pending);
    TEST_ASSERT_TRUE(observed.active);
    TEST_ASSERT_EQUAL_HEX32(mask | UINT32_C(0x5A), observed.pending_raw);
    TEST_ASSERT_EQUAL_HEX32(mask | UINT32_C(0xA5), observed.active_raw);
}

void test_interrupt_state_set_is_verified_from_register_readback(void)
{
    int status;
    struct mod_gicx00_multiview_interrupt_state observed;
    const unsigned int interrupt_id = 991U;

    expect_config();
    fwk_module_is_valid_element_id_ExpectAndReturn(gic_id, true);
    fwk_module_get_data_ExpectAndReturn(gic_id, &config);
    status = set_interrupt_state(gic_id, interrupt_id, true, true);
    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);

    expect_config();
    status = read_interrupt_state(gic_id, interrupt_id, &observed);
    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
    TEST_ASSERT_TRUE(observed.pending);
    TEST_ASSERT_TRUE(observed.active);
}

void test_interrupt_clear_times_out_when_hardware_keeps_stale_state(void)
{
    int status;
    const unsigned int interrupt_id = 991U;
    const uint32_t mask = UINT32_C(1) << (interrupt_id % 32U);
    const unsigned int register_index = interrupt_id / 32U;

    fwk_mmio_write_32(
        (uintptr_t)gicd_registers + GICX00_GICD_ISPENDR(register_index), mask);
    fwk_mmio_write_32(
        (uintptr_t)gicd_registers + GICX00_GICD_ISACTIVER(register_index),
        mask);

    expect_config();
    fwk_module_is_valid_element_id_ExpectAndReturn(gic_id, true);
    fwk_module_get_data_ExpectAndReturn(gic_id, &config);
    status = set_interrupt_state(gic_id, interrupt_id, false, false);

    TEST_ASSERT_EQUAL_INT(FWK_E_TIMEOUT, status);
}

void test_redistributor_state_reports_raw_power_and_waker_values(void)
{
    int status;
    struct mod_gicx00_multiview_redistributor_state observed;

    fwk_mmio_write_32((uintptr_t)gicr_registers + GICR_PWRR, GICR_PWRR_RDPD);
    fwk_mmio_write_32(
        (uintptr_t)gicr_registers + GICR_WAKER,
        GICR_WAKER_PROCESSOR_SLEEP | GICR_WAKER_CHILDREN_ASLEEP);

    expect_config();
    status = read_redistributor_state(gic_id, 0U, &observed);

    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL_HEX32(GICR_PWRR_RDPD, observed.pwrr);
    TEST_ASSERT_EQUAL_HEX32(
        GICR_WAKER_PROCESSOR_SLEEP | GICR_WAKER_CHILDREN_ASLEEP,
        observed.waker);
    TEST_ASSERT_TRUE(observed.powered_down);
    TEST_ASSERT_TRUE(observed.processor_sleep);
    TEST_ASSERT_TRUE(observed.children_asleep);
}

int template_test_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_power_state_round_trip);
    RUN_TEST(test_power_group_transition_times_out);
    RUN_TEST(test_invalid_power_state_is_rejected);
    RUN_TEST(test_invalid_redistributor_is_rejected);
    RUN_TEST(test_processor_sleep_times_out_without_children_asleep);
    RUN_TEST(test_processor_awake_state_completes);
    RUN_TEST(test_interrupt_state_reports_real_pending_and_active_readback);
    RUN_TEST(test_interrupt_state_set_is_verified_from_register_readback);
    RUN_TEST(test_interrupt_clear_times_out_when_hardware_keeps_stale_state);
    RUN_TEST(test_redistributor_state_reports_raw_power_and_waker_values);
    return UNITY_END();
}

#if !defined(TEST_ON_TARGET)
int main(void)
{
    return template_test_main();
}
#endif
