/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "unity.h"

#include <Mockfwk_id.h>
#include <Mockfwk_module.h>
#include <Mockfwk_notification.h>

#include <mod_cluster_control.h>

#include <fwk_mmio.h>
#include <fwk_module_idx.h>
#include <fwk_string.h>

#include UNIT_TEST_SRC
#include "config_cluster_control.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void setUp(void)
{
    /* Clear the cluster control registers between tests */
    fwk_str_memset(cluster_control_reg, 0, sizeof(cluster_control_reg));
}

void tearDown(void)
{
    /* Do Nothing */
}

static void validate_rvbars(void)
{
    unsigned int cluster_idx;
    uint32_t rvbar, astart, aend;

    for (cluster_idx = 0; cluster_idx < FWK_ARRAY_SIZE(cluster_control_reg);
         cluster_idx++) {
        astart = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x34));
        TEST_ASSERT_EQUAL(astart, 0);
        aend = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x38));
        TEST_ASSERT_EQUAL(aend, 1);
        astart = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x3C));
        TEST_ASSERT_EQUAL(astart, 2);
        aend = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x40));
        TEST_ASSERT_EQUAL(aend, 3);
        astart = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x44));
        TEST_ASSERT_EQUAL(astart, 4);
        aend = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x48));
        TEST_ASSERT_EQUAL(aend, 5);
        astart = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x4C));
        TEST_ASSERT_EQUAL(astart, 6);
        aend = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x50));
        TEST_ASSERT_EQUAL(aend, 7);

        rvbar = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x100));
        TEST_ASSERT_EQUAL(rvbar, 0xABABABAB);
        rvbar = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x104));
        TEST_ASSERT_EQUAL(rvbar, 0xCDCDCDCD);
        rvbar = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x108));
        TEST_ASSERT_EQUAL(rvbar, 0xABABABAB);
        rvbar = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x10C));
        TEST_ASSERT_EQUAL(rvbar, 0xCDCDCDCD);
        rvbar = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x110));
        TEST_ASSERT_EQUAL(rvbar, 0xABABABAB);
        rvbar = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x114));
        TEST_ASSERT_EQUAL(rvbar, 0xCDCDCDCD);
        rvbar = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x118));
        TEST_ASSERT_EQUAL(rvbar, 0xABABABAB);
        rvbar = fwk_mmio_read_32(
            (uintptr_t)(cluster_control_reg[cluster_idx] + 0x11C));
        TEST_ASSERT_EQUAL(rvbar, 0xCDCDCDCD);
    }
}

static const void *fwk_module_get_data_direct_stub(fwk_id_t id, int num_calls)
{
    if (id.common.type == FWK_ID_TYPE_MODULE)
        return &cluster_control_config_direct;
    else {
        uint32_t element_idx = id.element.element_idx;
        return &config_cluster_control_element[element_idx];
    }
}

void test_cluster_control_direct(void)
{
    int status;
    fwk_id_t element_id;

    fwk_id_is_type_IgnoreAndReturn(false);
    fwk_module_get_data_StubWithCallback(fwk_module_get_data_direct_stub);
    fwk_id_type_is_valid_IgnoreAndReturn(false);
    fwk_module_get_element_name_IgnoreAndReturn("Test");

    for (uint32_t idx = 0; idx < FWK_ARRAY_SIZE(cluster_control_reg); idx++) {
        element_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_CLUSTER_CONTROL, idx);

        status = cluster_control_start(element_id);
        TEST_ASSERT_EQUAL(status, FWK_SUCCESS);
    }

    validate_rvbars();
}

static const void *fwk_module_get_data_notification_stub(
    fwk_id_t id,
    int num_calls)
{
    if (id.common.type == FWK_ID_TYPE_MODULE)
        return &cluster_control_config_notification;
    else {
        uint32_t element_idx = id.element.element_idx;
        return &config_cluster_control_element[element_idx];
    }
}

void test_cluster_control_notification(void)
{
    int status;
    fwk_id_t element_id;

    fwk_module_get_data_StubWithCallback(fwk_module_get_data_notification_stub);
    fwk_id_type_is_valid_IgnoreAndReturn(true);
    fwk_module_get_element_name_IgnoreAndReturn("Test");

    for (uint32_t idx = 0; idx < FWK_ARRAY_SIZE(cluster_control_reg); idx++) {
        element_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_CLUSTER_CONTROL, idx);

        fwk_id_is_type_ExpectAndReturn(element_id, FWK_ID_TYPE_MODULE, false);

        fwk_id_is_equal_ExpectAndReturn(
            fwk_module_id_test_module, FWK_ID_NONE, false);

        fwk_notification_subscribe_ExpectAndReturn(
            test_module_notification_test,
            fwk_module_id_test_module,
            element_id,
            FWK_SUCCESS);

        status = cluster_control_start(element_id);

        TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    }

    for (uint32_t idx = 0; idx < FWK_ARRAY_SIZE(cluster_control_reg); idx++) {
        element_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_CLUSTER_CONTROL, idx);
        struct fwk_event event = {
            .target_id = element_id,
            .source_id = fwk_module_id_test_module,
            .id = test_module_notification_test,
        };

        fwk_id_is_type_ExpectAndReturn(element_id, FWK_ID_TYPE_ELEMENT, true);

        fwk_id_is_equal_ExpectAndReturn(
            test_module_notification_test, test_module_notification_test, true);

        fwk_notification_unsubscribe_ExpectAndReturn(
            test_module_notification_test,
            fwk_module_id_test_module,
            element_id,
            FWK_SUCCESS);

        status = cluster_control_process_notification(&event, NULL);

        TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    }

    validate_rvbars();
}

int cluster_control_test_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_cluster_control_direct);
    RUN_TEST(test_cluster_control_notification);

    return UNITY_END();
}

#if !defined(TEST_ON_TARGET)
int main(void)
{
    return cluster_control_test_main();
}
#endif
