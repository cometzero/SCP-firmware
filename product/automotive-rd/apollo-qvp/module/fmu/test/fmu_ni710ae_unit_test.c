/* SPDX-License-Identifier: BSD-3-Clause */
#include "scp_unity.h"
#include "unity.h"
#include <Mockfwk_interrupt.h>
#include <Mockfwk_module.h>
#include <fwk_macros.h>
#include <fwk_string.h>
#include UNIT_TEST_SRC

static uint8_t fmu_reg[64 * FWK_KIB];
static struct mod_fmu_dev_config fmu_config = {
    .base = (uintptr_t)fmu_reg,
    .has_parent_sources = true,
    .parent_source = { 1, 2, 3 },
};
static struct mod_fmu_ack_state ack_state;

void setUp(void)
{
    ack_state = (struct mod_fmu_ack_state){ 0 };
    fwk_str_memset(fmu_reg, 0, sizeof(fmu_reg));
    fwk_mmio_write_32(fmu_config.base + FMU_ERRDEVID, 3);
    fwk_mmio_write_32(fmu_config.base + FMU_ERR_MISC0(0), 0x4);
    fwk_mmio_write_32(fmu_config.base + FMU_ERR_MISC0(2), 0x10061);
    fwk_mmio_write_32(fmu_config.base + FMU_ERR_MISC0(2) + 4, 0x300801);
    fwk_mmio_write_32(fmu_config.base + FMU_ERR_CTLR_0, 0x210D);
}
void tearDown(void) {}

void test_ni_inject_selects_requested_node_without_cached_discovery(void)
{
    struct mod_fmu_fault fault = { .node_idx = 2, .sm_idx = 13 };
    TEST_ASSERT_EQUAL(FWK_SUCCESS, inject(&fmu_config, &fault));
    TEST_ASSERT_EQUAL_HEX32(0x10061, read32(&fmu_config, FMU_ERR_SMINFO));
    TEST_ASSERT_EQUAL_HEX32(0x300801, read32(&fmu_config, FMU_ERR_SMINFO + 4));
    TEST_ASSERT_EQUAL_UINT(13, read32(&fmu_config, FMU_ERR_INJECT));
    fault.node_idx = 0;
    TEST_ASSERT_EQUAL(FWK_SUCCESS, inject(&fmu_config, &fault));
    TEST_ASSERT_EQUAL_HEX32(0x4, read32(&fmu_config, FMU_ERR_SMINFO));
    TEST_ASSERT_EQUAL_HEX32(0, read32(&fmu_config, FMU_ERR_SMINFO + 4));
}

void test_ni_invalid_source_and_mechanism_do_not_write(void)
{
    struct mod_fmu_fault fault = { .node_idx = 3, .sm_idx = 13 };
    TEST_ASSERT_EQUAL(FWK_E_PARAM, inject(&fmu_config, &fault));
    fault.node_idx = 1;
    TEST_ASSERT_EQUAL(FWK_E_DATA, inject(&fmu_config, &fault));
    fault.node_idx = 2;
    fault.sm_idx = 18;
    TEST_ASSERT_EQUAL(FWK_E_PARAM, inject(&fmu_config, &fault));
    TEST_ASSERT_EQUAL(FWK_E_PARAM, set_enabled(&fmu_config, &fault, true));
    fault.sm_idx = 2;
    fault.block_id = 1;
    TEST_ASSERT_EQUAL(FWK_E_PARAM, inject(&fmu_config, &fault));
    TEST_ASSERT_EQUAL_HEX32(0, read32(&fmu_config, FMU_FIELD_SYS_KEY));
}

void test_ni_enable_preserves_other_mechanisms_and_global_control(void)
{
    struct mod_fmu_fault fault = { .node_idx = 2, .sm_idx = 2 };
    bool enabled;
    fwk_mmio_write_32(fmu_config.base + FMU_ERR_SMEN, 1U << 14);
    fwk_mmio_write_32(fmu_config.base + FMU_ERR_CTLR_0, 0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, set_enabled(&fmu_config, &fault, true));
    TEST_ASSERT_EQUAL_HEX32((1U << 14) | 4, read32(&fmu_config, FMU_ERR_SMEN));
    TEST_ASSERT_EQUAL_HEX32(0x210D, read32(&fmu_config, FMU_ERR_CTLR_0));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, get_enabled(&fmu_config, &fault, &enabled));
    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, set_enabled(&fmu_config, &fault, false));
    TEST_ASSERT_EQUAL_HEX32(1U << 14, read32(&fmu_config, FMU_ERR_SMEN));
}

void test_ni_runtime_criticality_is_unsupported(void)
{
    struct mod_fmu_fault fault = { .node_idx = 2, .sm_idx = 2 };
    TEST_ASSERT_EQUAL(FWK_E_SUPPORT, set_critical(&fmu_config, &fault, true));
    TEST_ASSERT_EQUAL(FWK_E_SUPPORT, set_critical(&fmu_config, &fault, false));
    TEST_ASSERT_EQUAL_HEX32(0, read32(&fmu_config, FMU_FIELD_SYS_KEY));
}

void test_ni_severity_peek_skips_opposite_class(void)
{
    unsigned int node;
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRGSR_L(0), 5);
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRSTATUS(0), V_BIT | UE_BIT);
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRSTATUS(2), V_BIT | UE_BIT | CI_BIT);
    TEST_ASSERT_TRUE(fault_peek(&fmu_config, true, &node));
    TEST_ASSERT_EQUAL_UINT(2, node);
    TEST_ASSERT_TRUE(fault_peek(&fmu_config, false, &node));
    TEST_ASSERT_EQUAL_UINT(0, node);
}

void test_ni_parent_route_handles_upgraded_ue_without_leaf_ci(void)
{
    unsigned int node;
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRGSR_L(0), 5);
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRSTATUS(0), V_BIT | (2U << 24));
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRSTATUS(2), V_BIT | UE_BIT);
    TEST_ASSERT_TRUE(fault_peek_source(&fmu_config, 1, &node));
    TEST_ASSERT_EQUAL_UINT(2, node);
    TEST_ASSERT_TRUE(fault_peek_source(&fmu_config, 2, &node));
    TEST_ASSERT_EQUAL_UINT(0, node);
    TEST_ASSERT_FALSE(fault_peek_source(&fmu_config, 3, &node));
    TEST_ASSERT_FALSE(fault_peek_source(&fmu_config, 9, &node));
}

void test_ni_masked_sources_and_stale_gsr_are_not_selected(void)
{
    unsigned int node;
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRGSR_L(0), 5);
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRSTATUS(2), V_BIT | UE_BIT);
    fwk_mmio_write_32(fmu_config.base + FMU_ERR_CTLR_0, CTRL_ED | CTRL_CFI);
    TEST_ASSERT_FALSE(fault_peek_source(&fmu_config, 1, &node));
    TEST_ASSERT_FALSE(fault_peek_source(&fmu_config, 2, &node));
}

void test_ni_mixed_snapshot_ack_is_once_and_clears_all_observed_flags(void)
{
    struct mod_fmu_fault fault = { 0 };
    bool tracked = false;
    uint32_t status = V_BIT | UE_BIT | CI_BIT | (2U << 24) | OF_BIT | (13U << 8);
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_ERRSTATUS(2), status);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        fault_ack(&fmu_config, &fault, 2, &tracked, &ack_state));
    TEST_ASSERT_EQUAL_UINT(2, fault.node_idx);
    TEST_ASSERT_EQUAL_UINT(13, fault.sm_idx);
    TEST_ASSERT_EQUAL_UINT(7, ack_state.parent_clear_mask);
    TEST_ASSERT_EQUAL_HEX32(status & STATUS_W1C,
                           read32(&fmu_config, FMU_FIELD_ERRSTATUS(2)));
    fwk_mmio_write_32(fmu_config.base + FMU_FIELD_SYS_KEY, 0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS,
        fault_ack(&fmu_config, &fault, 2, &tracked, &ack_state));
    TEST_ASSERT_EQUAL_HEX32(0, read32(&fmu_config, FMU_FIELD_SYS_KEY));
}

void test_ni_ack_rejects_invalid_or_empty_snapshot(void)
{
    struct mod_fmu_fault fault = { 0 };
    bool tracked = false;
    TEST_ASSERT_EQUAL(FWK_E_PARAM,
        fault_ack(&fmu_config, &fault, 3, &tracked, &ack_state));
    TEST_ASSERT_EQUAL(FWK_E_DATA,
        fault_ack(&fmu_config, &fault, 2, &tracked, &ack_state));
    TEST_ASSERT_FALSE(tracked);
    TEST_ASSERT_FALSE(ack_state.acknowledged);
    TEST_ASSERT_EQUAL_HEX32(0, read32(&fmu_config, FMU_FIELD_SYS_KEY));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ni_inject_selects_requested_node_without_cached_discovery);
    RUN_TEST(test_ni_invalid_source_and_mechanism_do_not_write);
    RUN_TEST(test_ni_enable_preserves_other_mechanisms_and_global_control);
    RUN_TEST(test_ni_runtime_criticality_is_unsupported);
    RUN_TEST(test_ni_severity_peek_skips_opposite_class);
    RUN_TEST(test_ni_parent_route_handles_upgraded_ue_without_leaf_ci);
    RUN_TEST(test_ni_masked_sources_and_stale_gsr_are_not_selected);
    RUN_TEST(test_ni_mixed_snapshot_ack_is_once_and_clears_all_observed_flags);
    RUN_TEST(test_ni_ack_rejects_invalid_or_empty_snapshot);
    return UNITY_END();
}
