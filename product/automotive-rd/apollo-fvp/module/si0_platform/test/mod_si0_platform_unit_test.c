/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "si0_cfgd_scmi.h"
#include "unity.h"

#include <Mockfwk_id.h>
#include <Mockfwk_module.h>
#include <Mockfwk_notification.h>

#include <mod_power_domain.h>
#include <mod_ppu_v1.h>
#include <mod_si0_platform.h>
#include <mod_transport.h>

#include <fwk_module_idx.h>

#include UNIT_TEST_SRC

#include "Mocksi0_platform.h"
#include "config_si0_platform.h"

static int mock_sds_struct_write_status = FWK_SUCCESS;
static uint32_t mock_sds_struct_write_structure_id;
static unsigned int mock_sds_struct_write_offset;
static uint32_t mock_sds_struct_write_data;
static size_t mock_sds_struct_write_size;

static int mock_sds_struct_write(
    uint32_t structure_id,
    unsigned int offset,
    const void *data,
    size_t size)
{
    mock_sds_struct_write_structure_id = structure_id;
    mock_sds_struct_write_offset = offset;
    mock_sds_struct_write_data = *(const uint32_t *)data;
    mock_sds_struct_write_size = size;

    return mock_sds_struct_write_status;
}

void setUp(void)
{
    mock_sds_struct_write_status = FWK_SUCCESS;
    mock_sds_struct_write_structure_id = 0U;
    mock_sds_struct_write_offset = 0U;
    mock_sds_struct_write_data = 0U;
    mock_sds_struct_write_size = 0U;
}

void tearDown(void)
{
    /* Do Nothing */
}

struct mod_si0_platform_config system_config = {
    .primary_cpu_mpid = 0,
    .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
};

const struct mod_transport_firmware_signal_api
    platform_rse_transport_signal_api = { 0 };

const void *get_rse_platform_transport_signal_api(void)
{
    return &platform_rse_transport_signal_api;
}

int platform_rse_bind(const struct mod_si0_platform_config *config)
{
    return FWK_SUCCESS;
}

int notify_rse_and_wait_for_response(void)
{
    return FWK_SUCCESS;
}
/*!
 * \brief SI0 Platform unit test: si0_platform_mod_init(),
 *
 *  \details Test successful initialization of si0_platform module
 */
void test_si0_platform_mod_init_success(void)
{
    int status;

    fwk_id_type_is_valid_ExpectAndReturn(system_config.timer_id, true);
    fwk_id_type_is_valid_ExpectAndReturn(system_config.transport_id, true);
    status =
        si0_platform_mod_init(fwk_module_id_si0_platform, 0, &system_config);

    TEST_ASSERT_EQUAL(status, FWK_SUCCESS);
}

/*!
 * \brief SI0 Platform unit test: si0_platform_bind(),
 *
 *  \details Test successful bind of si0_platform module
 */
void test_si0_platform_bind_success(void)
{
    int status;

    platform_power_mgmt_bind_ExpectAndReturn(FWK_SUCCESS);
    fwk_module_bind_IgnoreAndReturn(FWK_SUCCESS);

    status = si0_platform_bind(fwk_module_id_si0_platform, 0);

    TEST_ASSERT_EQUAL(status, FWK_SUCCESS);
}

/*!
 * \brief SI0 Platform unit test: si0_platform_bind(),
 *
 *  \details Test failure in bind of si0_platform module
 */
void test_si0_platform_bind_fail(void)
{
    int status;

    platform_power_mgmt_bind_ExpectAndReturn(FWK_E_DATA);
    fwk_module_bind_IgnoreAndReturn(FWK_E_DATA);

    status = si0_platform_bind(fwk_module_id_si0_platform, 0);

    TEST_ASSERT_EQUAL(status, FWK_E_DATA);
}

/*!
 * \brief SI0 Platform unit test: si0_platform_start(),
 *
 *  \details Test successful start of si0_platform module
 */
void test_si0_platform_mod_start_success(void)
{
    int status;
    struct fwk_event event = { 0 };
    event.id = mod_si0_platform_notification_subsys_init;
    event.source_id = fwk_module_id_si0_platform;
    unsigned int count = 0U;

    fwk_notification_notify_ExpectAndReturn(&event, &count, FWK_SUCCESS);

    fwk_id_get_element_idx_IgnoreAndReturn(0);
    fwk_module_get_element_name_IgnoreAndReturn("Test");

    fwk_id_t pd_transition_source_id =
        FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, 0);
    fwk_id_build_element_id_ExpectAndReturn(
        fwk_module_id_power_domain, 0, pd_transition_source_id);

    fwk_id_t pd_transition_notification_id = FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN,
        MOD_PD_NOTIFICATION_IDX_POWER_STATE_TRANSITION);

    fwk_notification_subscribe_ExpectAndReturn(
        pd_transition_notification_id,
        pd_transition_source_id,
        fwk_module_id_si0_platform,
        FWK_SUCCESS);

    fwk_id_t mod_pd_notification_id_pre_warmreset = FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN, MOD_PD_NOTIFICATION_IDX_PRE_WARM_RESET);

    fwk_notification_subscribe_ExpectAndReturn(
        mod_pd_notification_id_pre_warmreset,
        FWK_ID_MODULE(FWK_MODULE_IDX_POWER_DOMAIN),
        fwk_module_id_si0_platform,
        FWK_SUCCESS);

    status = si0_platform_start(fwk_module_id_si0_platform);
    TEST_ASSERT_EQUAL(status, FWK_SUCCESS);
}

/*!
 * \brief SI0 Platform unit test: si0_platform_start(),
 *
 *  \details Test failure starting the si0_platform module
 */
void test_si0_platform_mod_start_pre_warmreset_fail_notification(void)
{
    int status;
    struct fwk_event event = { 0 };
    event.id = mod_si0_platform_notification_subsys_init;
    event.source_id = fwk_module_id_si0_platform;
    unsigned int count = 0U;

    fwk_notification_notify_ExpectAndReturn(&event, &count, FWK_E_DATA);

    fwk_id_get_element_idx_IgnoreAndReturn(0);
    fwk_module_get_element_name_IgnoreAndReturn("Test");

    fwk_id_t pd_transition_source_id =
        FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, 0);
    fwk_id_build_element_id_ExpectAndReturn(
        fwk_module_id_power_domain, 0, pd_transition_source_id);

    fwk_id_t pd_transition_notification_id = FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN,
        MOD_PD_NOTIFICATION_IDX_POWER_STATE_TRANSITION);
    fwk_notification_subscribe_ExpectAndReturn(
        pd_transition_notification_id,
        pd_transition_source_id,
        fwk_module_id_si0_platform,
        FWK_E_DATA);

    status = si0_platform_start(fwk_module_id_si0_platform);
    TEST_ASSERT_EQUAL(status, FWK_E_PANIC);
}

/*!
 * \brief SI0 Platform unit test: si0_platform_start(),
 *
 *  \details Test failure starting the si0_platform module
 */
void test_si0_platform_mod_start_pd_trans_fail_notification(void)
{
    int status;
    struct fwk_event event = { 0 };
    event.id = mod_si0_platform_notification_subsys_init;
    event.source_id = fwk_module_id_si0_platform;
    unsigned int count = 0U;

    fwk_notification_notify_ExpectAndReturn(&event, &count, FWK_E_DATA);

    fwk_id_get_element_idx_IgnoreAndReturn(0);
    fwk_module_get_element_name_IgnoreAndReturn("Test");

    fwk_id_t pd_transition_source_id =
        FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, 0);
    fwk_id_build_element_id_ExpectAndReturn(
        fwk_module_id_power_domain, 0, pd_transition_source_id);

    fwk_id_t pd_transition_notification_id = FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN,
        MOD_PD_NOTIFICATION_IDX_POWER_STATE_TRANSITION);
    fwk_notification_subscribe_ExpectAndReturn(
        pd_transition_notification_id,
        pd_transition_source_id,
        fwk_module_id_si0_platform,
        FWK_SUCCESS);

    fwk_id_t mod_pd_notification_id_pre_warmreset = FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN, MOD_PD_NOTIFICATION_IDX_PRE_WARM_RESET);

    fwk_notification_subscribe_ExpectAndReturn(
        mod_pd_notification_id_pre_warmreset,
        FWK_ID_MODULE(FWK_MODULE_IDX_POWER_DOMAIN),
        fwk_module_id_si0_platform,
        FWK_E_DATA);

    status = si0_platform_start(fwk_module_id_si0_platform);
    TEST_ASSERT_EQUAL(status, FWK_E_PANIC);
}

/*!
 * \brief SI0 Platform unit test: update_sds_reset_syndrome(),
 *
 *  \details Test successful reset syndrome write to SDS
 */
void test_update_sds_reset_syndrome_success(void)
{
    int status;
    const uint32_t reset_syndrome = 0x8U;
    const struct mod_sds_structure_desc sds_structure_desc = {
        .id = SDS_RESET_SYNDROME_STRUCT_ID,
        .size = sizeof(reset_syndrome),
    };
    static const struct mod_sds_api sds_api = {
        .struct_write = mock_sds_struct_write,
    };

    si0_platform_ctx.sds_api = &sds_api;
    fwk_module_get_data_ExpectAndReturn(
        sds_reset_syndrome_id, &sds_structure_desc);

    status = update_sds_reset_syndrome(reset_syndrome);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(
        sds_structure_desc.id, mock_sds_struct_write_structure_id);
    TEST_ASSERT_EQUAL(0U, mock_sds_struct_write_offset);
    TEST_ASSERT_EQUAL(reset_syndrome, mock_sds_struct_write_data);
    TEST_ASSERT_EQUAL(sizeof(reset_syndrome), mock_sds_struct_write_size);
}

/*!
 * \brief SI0 Platform unit test: update_sds_reset_syndrome(),
 *
 *  \details Test failure when SDS structure descriptor is unavailable
 */
void test_update_sds_reset_syndrome_fail_null_sds_desc(void)
{
    int status;
    static const struct mod_sds_api sds_api = {
        .struct_write = mock_sds_struct_write,
    };

    si0_platform_ctx.sds_api = &sds_api;
    fwk_module_get_data_ExpectAndReturn(sds_reset_syndrome_id, NULL);

    status = update_sds_reset_syndrome(0x8U);

    TEST_ASSERT_EQUAL(FWK_E_DATA, status);
}

/*!
 * \brief SI0 Platform unit test: update_sds_reset_syndrome(),
 *
 *  \details Test failure when SDS write operation fails
 */
void test_update_sds_reset_syndrome_fail_struct_write(void)
{
    int status;
    const uint32_t reset_syndrome = 0x8U;
    const struct mod_sds_structure_desc sds_structure_desc = {
        .id = SDS_RESET_SYNDROME_STRUCT_ID,
        .size = sizeof(reset_syndrome),
    };
    static const struct mod_sds_api sds_api = {
        .struct_write = mock_sds_struct_write,
    };

    mock_sds_struct_write_status = FWK_E_DEVICE;
    si0_platform_ctx.sds_api = &sds_api;
    fwk_module_get_data_ExpectAndReturn(
        sds_reset_syndrome_id, &sds_structure_desc);

    status = update_sds_reset_syndrome(reset_syndrome);

    TEST_ASSERT_EQUAL(FWK_E_DEVICE, status);
}

int si0_platform_test_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_si0_platform_mod_init_success);
    RUN_TEST(test_si0_platform_bind_success);
    RUN_TEST(test_si0_platform_bind_fail);
    RUN_TEST(test_si0_platform_mod_start_success);
    RUN_TEST(test_si0_platform_mod_start_pre_warmreset_fail_notification);
    RUN_TEST(test_si0_platform_mod_start_pd_trans_fail_notification);
    RUN_TEST(test_update_sds_reset_syndrome_success);
    RUN_TEST(test_update_sds_reset_syndrome_fail_null_sds_desc);
    RUN_TEST(test_update_sds_reset_syndrome_fail_struct_write);

    return UNITY_END();
}

int pd_transition_ap_platform_hook(unsigned int pd_state)
{
    return FWK_SUCCESS;
}

int main(void)
{
    return si0_platform_test_main();
}
