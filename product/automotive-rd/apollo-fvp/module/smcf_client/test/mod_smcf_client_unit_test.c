/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "scp_unity.h"
#include "unity.h"

#include <Mockfwk_id.h>
#include <Mockfwk_interrupt.h>
#include <Mockfwk_mm.h>
#include <Mockfwk_module.h>
#include <Mockfwk_notification.h>

#include <fwk_macros.h>

#include <string.h>

#include UNIT_TEST_SRC

#include <mod_power_domain.h>
#include <mod_smcf_client.h>

static unsigned int fake_start_sampling_count;
static unsigned int fake_stop_sampling_count;
static unsigned int fake_amu_get_counter_count;
static unsigned int fake_sensor_get_samples_call_count;
static fwk_id_t last_start_sampling_id;
static fwk_id_t last_stop_sampling_id;

void setUp(void)
{
    /* Reset fake function count */
    fake_start_sampling_count = 0;
    fake_stop_sampling_count = 0;
    fake_amu_get_counter_count = 0;
    fake_sensor_get_samples_call_count = 0;

    /* Reset context */
    memset(&ctx, 0, sizeof(ctx));

    /* Reset print flag */
    print_is_on_flag = 0;
}

void tearDown(void)
{
    /* Do Nothing */
}

/**
 * @brief Verify module initialization succeeds when SMCF element count is read
 *        successfully and backing storage allocation succeeds.
 *
 * Expects:
 * - SMCF element count query to succeed.
 * - MGI configuration pointer table to be allocated.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_init_success(void)
{
    int status;
    unsigned int element_count = 1;
    ctx.mgi_count = element_count;
    int dummy = 0;

    fwk_module_get_element_count_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF), &(ctx.mgi_count), FWK_SUCCESS);
    fwk_mm_calloc_ExpectAndReturn(
        element_count, sizeof(struct mod_smcf_client_mgi_conf *), &dummy);

    status = smcf_client_init(fwk_module_id_smcf_client, element_count, NULL);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/**
 * @brief Verify module initialization propagates errors from querying the SMCF
 *        module element count.
 *
 * Expects:
 * - SMCF element count query to fail.
 * - Function returns the same error code.
 */
void test_smcf_client_init_fail_no_element_count(void)
{
    int status;
    unsigned int element_count = 1;
    ctx.mgi_count = element_count;

    fwk_module_get_element_count_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF), &(ctx.mgi_count), FWK_E_PARAM);

    status = smcf_client_init(fwk_module_id_smcf_client, element_count, NULL);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

/**
 * @brief Verify module initialization succeeds for zero configured elements and
 *        clears the MGI configuration table pointer.
 *
 * Expects:
 * - Function returns `FWK_SUCCESS`.
 * - `ctx.mgi_conf` is overwritten to `NULL`.
 */
void test_smcf_client_init_success_zero_elements(void)
{
    int status;
    unsigned int element_count = 0;
    size_t smcf_elem_count;

    /* Ensure we can observe that init() overwrites this to NULL */
    ctx.mgi_conf = (const struct mod_smcf_client_mgi_conf **)0x1;
    ctx.mgi_count = 0;

    /* Value returned by fwk_module_get_element_count() is irrelevant here
     */
    smcf_elem_count = 7;

    fwk_module_get_element_count_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF), &(ctx.mgi_count), FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&smcf_elem_count);

    status = smcf_client_init(fwk_module_id_smcf_client, element_count, NULL);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_NULL(ctx.mgi_conf);
}

/**
 * @brief Verify module initialization rejects a non-zero smcf-client element
 *        count that does not match the SMCF module element count.
 *
 * Expects:
 * - SMCF element count query to succeed.
 * - Element count mismatch to be detected.
 * - Function returns `FWK_E_PARAM`.
 */
void test_smcf_client_init_fail_invalid_element_count(void)
{
    int status;
    unsigned int element_count = 1;
    size_t smcf_elem_count;

    /*
     * smcf_client_init() first reads the SMCF module element count into
     * ctx.mgi_count, then compares it against the smcf-client element_count
     * (only when element_count != 0).
     */
    ctx.mgi_count = 0;
    smcf_elem_count = element_count + 1;

    fwk_module_get_element_count_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF), &(ctx.mgi_count), FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&smcf_elem_count);

    status = smcf_client_init(fwk_module_id_smcf_client, element_count, NULL);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

/**
 * @brief Verify per-element initialization stores the provided element
 *        configuration in the module context at the expected index.
 *
 * Expects:
 * - Element index lookup to succeed.
 * - `ctx.mgi_conf[element_idx]` to point at the provided config.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_element_init(void)
{
    int status;
    unsigned int element_count = 1;
    size_t smcf_elem_count;

    static struct mod_smcf_client_mli_conf mlis[1] = {
        [0] = { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_AMU },
    };

    static struct mod_smcf_client_mgi_conf mgi_conf0 = {
        .smcf_mgi_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
        .mlis = mlis,
    };

    /* Backing storage for ctx.mgi_conf (what fwk_mm_calloc() “allocates”) */
    static const struct mod_smcf_client_mgi_conf *mgi_conf_ptrs[1];

    /* Init must succeed and allocate ctx.mgi_conf */
    ctx.mgi_count = 0;
    smcf_elem_count = element_count;

    fwk_module_get_element_count_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF), &(ctx.mgi_count), FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&smcf_elem_count);

    fwk_mm_calloc_ExpectAndReturn(
        element_count,
        sizeof(struct mod_smcf_client_mgi_conf *),
        (void *)mgi_conf_ptrs);

    status = smcf_client_init(fwk_module_id_smcf_client, element_count, NULL);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    fwk_id_get_element_idx_ExpectAndReturn(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0);

    status = smcf_client_element_init(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0, &mgi_conf0);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&mgi_conf0, ctx.mgi_conf[0]);
}

/* Return success only on first call, record last seen element_id */
static int fake_start_data_sampling(fwk_id_t element_id)
{
    if (fake_start_sampling_count == 0) {
        fake_start_sampling_count++;
        last_start_sampling_id = element_id;
        return FWK_SUCCESS;
    } else {
        return FWK_E_RANGE;
    }
}

/* Return success only on first call, record last seen element_id */
static int fake_stop_data_sampling(fwk_id_t element_id)
{
    if (fake_stop_sampling_count == 0) {
        fake_stop_sampling_count++;
        last_stop_sampling_id = element_id;
        return FWK_SUCCESS;
    } else {
        return FWK_E_RANGE;
    }
}

/* Fake AMU API */
static struct smcf_data_api fake_sampling_api = {
    .start_data_sampling = fake_start_data_sampling,
    .stop_data_sampling = fake_stop_data_sampling,
};

/**
 * @brief Verify starting sampling across all MGIs succeeds when the platform
 *        sampling API succeeds for every MGI.
 *
 * Expects:
 * - Platform start-sampling API to be called once per MGI.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_start_sampling_all_mgis_success(void)
{
    int status;
    ctx.mgi_count = 1;
    ctx.platform_sampling_api = &fake_sampling_api;
    fwk_id_t expected_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0);

    status = start_sampling_all_mgis();

    TEST_ASSERT_EQUAL(ctx.mgi_count, fake_start_sampling_count);
    TEST_ASSERT_EQUAL_MEMORY(
        &expected_id, &last_start_sampling_id, sizeof(fwk_id_t));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/**
 * @brief Verify starting sampling across all MGIs returns an error when any MGI
 *        start-sampling operation fails.
 *
 * Expects:
 * - A platform start-sampling call to fail for an MGI.
 * - Function returns the first encountered error.
 */
void test_smcf_client_start_sampling_all_mgis_fail(void)
{
    int status;
    ctx.mgi_count = 2;
    ctx.platform_sampling_api = &fake_sampling_api;

    status = start_sampling_all_mgis();

    TEST_ASSERT_EQUAL(FWK_E_RANGE, status);
}

/**
 * @brief Verify starting sampling across all MGIs is a no-op and succeeds when
 *        there are no MGIs.
 *
 * Expects:
 * - No platform start-sampling calls to be made.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_start_sampling_all_mgis_zero_mgis(void)
{
    int status;

    ctx.mgi_count = 0;
    ctx.platform_sampling_api = &fake_sampling_api;
    fwk_id_t sentinel_id = FWK_ID_ELEMENT(42, 42);
    last_start_sampling_id = sentinel_id;

    status = start_sampling_all_mgis();

    TEST_ASSERT_EQUAL_MEMORY(
        &sentinel_id, &last_start_sampling_id, sizeof(fwk_id_t));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(0, fake_start_sampling_count);
}

/**
 * @brief Verify stopping sampling across all MGIs succeeds when the platform
 *        sampling API succeeds for every MGI.
 *
 * Expects:
 * - Platform stop-sampling API to be called once per MGI.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_stop_sampling_all_mgis_success(void)
{
    int status;
    ctx.mgi_count = 1;
    ctx.platform_sampling_api = &fake_sampling_api;
    fwk_id_t expected_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0);

    status = stop_sampling_all_mgis();

    TEST_ASSERT_EQUAL(ctx.mgi_count, fake_stop_sampling_count);
    TEST_ASSERT_EQUAL_MEMORY(
        &expected_id, &last_stop_sampling_id, sizeof(fwk_id_t));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/**
 * @brief Verify stopping sampling across all MGIs returns an error when any MGI
 *        stop-sampling operation fails.
 *
 * Expects:
 * - A platform stop-sampling call to fail for an MGI.
 * - Function returns the first encountered error.
 */
void test_smcf_client_stop_sampling_all_mgis_fail(void)
{
    int status;
    ctx.mgi_count = 2;
    ctx.platform_sampling_api = &fake_sampling_api;

    status = stop_sampling_all_mgis();

    TEST_ASSERT_EQUAL(FWK_E_RANGE, status);
}

/**
 * @brief Verify stopping sampling across all MGIs is a no-op and succeeds when
 *        there are no MGIs.
 *
 * Expects:
 * - No platform stop-sampling calls to be made.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_stop_sampling_all_mgis_zero_mgis(void)
{
    int status;

    ctx.mgi_count = 0;
    ctx.platform_sampling_api = &fake_sampling_api;
    fwk_id_t sentinel_id = FWK_ID_ELEMENT(42, 42);
    last_stop_sampling_id = sentinel_id;

    status = stop_sampling_all_mgis();

    TEST_ASSERT_EQUAL_MEMORY(
        &sentinel_id, &last_stop_sampling_id, sizeof(fwk_id_t));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(0, fake_stop_sampling_count);
}

/**
 * @brief Verify the print-enable flag toggling helper flips the global flag and
 *        returns the new state.
 *
 * Expects:
 * - `print_is_on_flag` to invert its value.
 * - Function returns the updated flag value.
 */
void test_smcf_client_toggle_print(void)
{
    int status;
    int original_flag_val = 0;
    print_is_on_flag = original_flag_val;
    status = toggle_print();
    TEST_ASSERT_EQUAL(!original_flag_val, status);
    TEST_ASSERT_EQUAL(!original_flag_val, print_is_on_flag);
}

/**
 * @brief Verify bind in round 1 succeeds (round 1 is treated as a no-op in this
 *        test).
 *
 * Expects:
 * - Function returns `FWK_SUCCESS`.
 */
void test_platform_smcf_bind_round1(void)
{
    int status;
    status = smcf_client_bind(fwk_module_id_smcf_client, 1);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/**
 * @brief Verify bind in round 0 succeeds when all required module API binds
 *        succeed.
 *
 * Expects:
 * - Bind to platform sampling API to succeed.
 * - Bind to SMCF control API to succeed.
 * - Bind to AMU SMCF driver data API to succeed.
 * - Bind to sensor SMCF driver data API to succeed.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_bind_round0_success(void)
{
    int status;

    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_PLATFORM_SMCF),
        FWK_ID_API(
            FWK_MODULE_IDX_PLATFORM_SMCF,
            MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API),
        &ctx.platform_sampling_api,
        FWK_SUCCESS);
    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF),
        FWK_ID_API(FWK_MODULE_IDX_SMCF, MOD_SMCF_API_IDX_CONTROL),
        &ctx.control_api,
        FWK_SUCCESS);
    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_AMU_SMCF_DRV),
        FWK_ID_API(FWK_MODULE_IDX_AMU_SMCF_DRV, MOD_AMU_SMCF_DRV_API_IDX_DATA),
        &ctx.amu_data_api,
        FWK_SUCCESS);
    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SENSOR_SMCF_DRV),
        FWK_ID_API(
            FWK_MODULE_IDX_SENSOR_SMCF_DRV,
            MOD_SENSOR_SMCF_DRV_API_IDX_GET_MULTIPLE_SAMPLES),
        &ctx.sensor_data_api,
        FWK_SUCCESS);

    status = smcf_client_bind(fwk_module_id_smcf_client, 0);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/**
 * @brief Verify bind in round 0 fails when binding to the platform SMCF
 * sampling API fails.
 *
 * Expects:
 * - Platform sampling API bind to fail.
 * - Function returns the same error code.
 */
void test_smcf_client_bind_fail_bind_to_platform_smcf(void)
{
    int status;

    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_PLATFORM_SMCF),
        FWK_ID_API(
            FWK_MODULE_IDX_PLATFORM_SMCF,
            MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API),
        &ctx.platform_sampling_api,
        FWK_E_PARAM);

    status = smcf_client_bind(fwk_module_id_smcf_client, 0);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

/**
 * @brief Verify bind in round 0 fails when binding to the SMCF control API
 *        fails.
 *
 * Expects:
 * - Platform sampling API bind to succeed.
 * - SMCF control API bind to fail.
 * - Function returns the same error code.
 */
void test_smcf_client_bind_fail_bind_to_smcf(void)
{
    int status;

    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_PLATFORM_SMCF),
        FWK_ID_API(
            FWK_MODULE_IDX_PLATFORM_SMCF,
            MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API),
        &ctx.platform_sampling_api,
        FWK_SUCCESS);
    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF),
        FWK_ID_API(FWK_MODULE_IDX_SMCF, MOD_SMCF_API_IDX_CONTROL),
        &ctx.control_api,
        FWK_E_PARAM);

    status = smcf_client_bind(fwk_module_id_smcf_client, 0);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

/**
 * @brief Verify bind in round 0 fails when binding to the AMU SMCF driver data
 *        API fails.
 *
 * Expects:
 * - Platform sampling API bind to succeed.
 * - SMCF control API bind to succeed.
 * - AMU data API bind to fail.
 * - Function returns the same error code.
 */
void test_smcf_client_bind_fail_bind_to_amu_smcf_drv(void)
{
    int status;

    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_PLATFORM_SMCF),
        FWK_ID_API(
            FWK_MODULE_IDX_PLATFORM_SMCF,
            MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API),
        &ctx.platform_sampling_api,
        FWK_SUCCESS);
    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF),
        FWK_ID_API(FWK_MODULE_IDX_SMCF, MOD_SMCF_API_IDX_CONTROL),
        &ctx.control_api,
        FWK_SUCCESS);
    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_AMU_SMCF_DRV),
        FWK_ID_API(FWK_MODULE_IDX_AMU_SMCF_DRV, MOD_AMU_SMCF_DRV_API_IDX_DATA),
        &ctx.amu_data_api,
        FWK_E_PARAM);

    status = smcf_client_bind(fwk_module_id_smcf_client, 0);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

/**
 * @brief Verify bind in round 0 fails when binding to the sensor SMCF driver
 *        data API fails.
 *
 * Expects:
 * - Platform sampling API bind to succeed.
 * - SMCF control API bind to succeed.
 * - AMU data API bind to succeed.
 * - Sensor data API bind to fail.
 * - Function returns the same error code.
 */
void test_smcf_client_bind_fail_bind_to_sensor_smcf_drv(void)
{
    int status;

    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_PLATFORM_SMCF),
        FWK_ID_API(
            FWK_MODULE_IDX_PLATFORM_SMCF,
            MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API),
        &ctx.platform_sampling_api,
        FWK_SUCCESS);
    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF),
        FWK_ID_API(FWK_MODULE_IDX_SMCF, MOD_SMCF_API_IDX_CONTROL),
        &ctx.control_api,
        FWK_SUCCESS);
    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_AMU_SMCF_DRV),
        FWK_ID_API(FWK_MODULE_IDX_AMU_SMCF_DRV, MOD_AMU_SMCF_DRV_API_IDX_DATA),
        &ctx.amu_data_api,
        FWK_SUCCESS);
    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SENSOR_SMCF_DRV),
        FWK_ID_API(
            FWK_MODULE_IDX_SENSOR_SMCF_DRV,
            MOD_SENSOR_SMCF_DRV_API_IDX_GET_MULTIPLE_SAMPLES),
        &ctx.sensor_data_api,
        FWK_E_PARAM);

    status = smcf_client_bind(fwk_module_id_smcf_client, 0);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

#ifdef BUILD_HAS_NOTIFICATION

/**
 * @brief Verify new-sample processing propagates errors when the framework
 *        fails to provide the MLI sub-element count.
 *
 * Expects:
 * - Source element name and index to be queried.
 * - Sub-element count query to fail.
 * - Function returns the same error code.
 */
void test_smcf_client_process_new_sample_fail_get_sub_element_count(void)
{
    int status;
    const struct fwk_event event = { .source_id = FWK_ID_MODULE(0) };
    const char *mgi_name = "\0";

    fwk_module_get_element_name_ExpectAndReturn(event.source_id, mgi_name);
    fwk_id_get_element_idx_ExpectAndReturn(event.source_id, 0);
    fwk_module_get_sub_element_count_ExpectAnyArgsAndReturn(FWK_E_PARAM);

    status = smcf_client_process_new_sample(&event);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

/**
 * @brief Verify new-sample processing rejects events whose source element index
 *        is out of range of the configured MGI table.
 *
 * Expects:
 * - Source element name and index to be queried.
 * - Sub-element count query to succeed.
 * - Index range check to fail.
 * - Function returns `FWK_E_RANGE`.
 */
void test_smcf_client_process_new_sample_fail_mgi_idx_out_of_range(void)
{
    int status;
    const struct fwk_event event = { .source_id = FWK_ID_MODULE(0) };
    const char *mgi_name = "\0";
    ctx.mgi_count = 0;

    fwk_module_get_element_name_ExpectAndReturn(event.source_id, mgi_name);
    fwk_id_get_element_idx_ExpectAndReturn(event.source_id, 1);
    fwk_module_get_sub_element_count_ExpectAnyArgsAndReturn(FWK_SUCCESS);

    status = smcf_client_process_new_sample(&event);

    TEST_ASSERT_EQUAL(FWK_E_RANGE, status);
}

/**
 * @brief Verify new-sample processing returns `FWK_E_SUPPORT` when an MLI has
 *        an unsupported monitor type.
 *
 * Expects:
 * - MLI type dispatch to hit the unsupported/default path.
 * - No AMU or sensor driver API calls to be required for this failure.
 * - Function returns `FWK_E_SUPPORT`.
 */
void test_smcf_client_process_new_sample_fail_unsupported_mli_type(void)
{
    int status;
    size_t smcf_elem_count;
    size_t mli_count;

    const struct fwk_event event = {
        .source_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
    };

    const char *mgi_name = "\0";

    /*
     * One MGI with one MLI of unsupported type -> should hit default: and
     * return FWK_E_SUPPORT without calling AMU/SENSOR APIs.
     */
    static struct mod_smcf_client_mli_conf mlis[1] = {
        [0] = { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_COUNT },
    };

    static struct mod_smcf_client_mgi_conf mgi_conf0 = {
        .smcf_mgi_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
        .mlis = mlis,
    };

    static const struct mod_smcf_client_mgi_conf *mgi_conf_ptrs[1];

    /* Make init succeed and set ctx.mgi_count = 1 */
    ctx.mgi_count = 0;
    smcf_elem_count = 1;

    fwk_module_get_element_count_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF), &(ctx.mgi_count), FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&smcf_elem_count);

    fwk_mm_calloc_ExpectAndReturn(
        smcf_elem_count,
        sizeof(struct mod_smcf_client_mgi_conf *),
        (void *)mgi_conf_ptrs);

    status = smcf_client_init(fwk_module_id_smcf_client, 1, NULL);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Install element configuration */
    fwk_id_get_element_idx_ExpectAndReturn(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0);

    status = smcf_client_element_init(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0, &mgi_conf0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Setup for smcf_client_process_new_sample() */
    print_is_on_flag = 0;

    fwk_module_get_element_name_ExpectAndReturn(event.source_id, mgi_name);
    fwk_id_get_element_idx_ExpectAndReturn(event.source_id, 0);

    mli_count = 1;
    fwk_module_get_sub_element_count_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_module_get_sub_element_count_ReturnThruPtr_mod_sub_elem_cnt(&mli_count);

    status = smcf_client_process_new_sample(&event);

    TEST_ASSERT_EQUAL(FWK_E_SUPPORT, status);
}

/* Return success only for MGI 0 MLI 0 */
static int fake_amu_get_counters(
    fwk_id_t start_counter_id,
    uint64_t *counter_buff,
    size_t num_counter)
{
    (void)start_counter_id;
    (void)counter_buff;
    (void)num_counter;

    if (fake_amu_get_counter_count == 0) {
        fake_amu_get_counter_count++;
        return FWK_SUCCESS;
    } else {
        return FWK_E_RANGE;
    }
}

/* Fake AMU API */
static struct amu_api fake_amu_api = {
    .get_counters = fake_amu_get_counters,
};

/**
 * @brief Verify AMU new-sample processing propagates an error when any AMU
 *        counter read fails while iterating MLIs.
 *
 * Expects:
 * - AMU counter read to be attempted for each AMU MLI.
 * - An error returned by an AMU read to be propagated.
 */
void test_smcf_client_process_new_sample_amu(void)
{
    int status;
    size_t smcf_elem_count;
    size_t mli_count;

    /* Event should look like it came from SMCF MGI element 0 */
    const struct fwk_event event = {
        .source_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
    };

    const char *mgi_name = "\0";

    /*
     * One MGI with two MLIs (both AMU) so get_counters() is called twice.
     * fake_amu_get_counters() succeeds once and then returns FWK_E_RANGE.
     */
    static struct mod_smcf_client_mli_conf mlis[2] = {
        [0] = { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_AMU },
        [1] = { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_AMU },
    };

    static struct mod_smcf_client_mgi_conf mgi_conf0 = {
        .smcf_mgi_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
        .mlis = mlis,
    };

    /* Backing storage for ctx.mgi_conf (what fwk_mm_calloc() “allocates”) */
    static const struct mod_smcf_client_mgi_conf *mgi_conf_ptrs[1];

    /* Make init succeed and set ctx.mgi_count = 1 */
    ctx.mgi_count = 0;
    smcf_elem_count = 1;

    fwk_module_get_element_count_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF), &(ctx.mgi_count), FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&smcf_elem_count);

    fwk_mm_calloc_ExpectAndReturn(
        smcf_elem_count,
        sizeof(struct mod_smcf_client_mgi_conf *),
        (void *)mgi_conf_ptrs);

    status = smcf_client_init(fwk_module_id_smcf_client, 1, NULL);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Install element configuration (must NOT be NULL) */
    fwk_id_get_element_idx_ExpectAndReturn(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0);

    status = smcf_client_element_init(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0, &mgi_conf0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Setup for smcf_client_process_new_sample() */
    ctx.amu_data_api = &fake_amu_api;
    print_is_on_flag = 0;

    /* Reset fake call counter so the first call returns FWK_SUCCESS */
    fake_amu_get_counter_count = 0;

    fwk_module_get_element_name_ExpectAndReturn(event.source_id, mgi_name);
    fwk_id_get_element_idx_ExpectAndReturn(event.source_id, 0);

    mli_count = 2;
    fwk_module_get_sub_element_count_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_module_get_sub_element_count_ReturnThruPtr_mod_sub_elem_cnt(&mli_count);

    status = smcf_client_process_new_sample(&event);

    /* First AMU get_counters() succeeds, second returns FWK_E_RANGE */
    TEST_ASSERT_EQUAL(FWK_E_RANGE, status);
}

/**
 * @brief Verify AMU new-sample processing succeeds with printing enabled when
 *        the AMU driver returns success.
 *
 * Expects:
 * - AMU counter read to succeed for the configured MLI(s).
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_process_new_sample_amu_success_print_on(void)
{
    int status;
    size_t smcf_elem_count;
    size_t mli_count;

    const struct fwk_event event = {
        .source_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
    };

    const char *mgi_name = "\0";

    /* One MGI with one MLI (AMU) -> fake_amu_get_counters() succeeds */
    static struct mod_smcf_client_mli_conf mlis[1] = {
        [0] = { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_AMU },
    };

    static struct mod_smcf_client_mgi_conf mgi_conf0 = {
        .smcf_mgi_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
        .mlis = mlis,
    };

    static const struct mod_smcf_client_mgi_conf *mgi_conf_ptrs[1];

    /* Make init succeed and set ctx.mgi_count = 1 */
    ctx.mgi_count = 0;
    smcf_elem_count = 1;

    fwk_module_get_element_count_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF), &(ctx.mgi_count), FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&smcf_elem_count);

    fwk_mm_calloc_ExpectAndReturn(
        smcf_elem_count,
        sizeof(struct mod_smcf_client_mgi_conf *),
        (void *)mgi_conf_ptrs);

    status = smcf_client_init(fwk_module_id_smcf_client, 1, NULL);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Install element configuration */
    fwk_id_get_element_idx_ExpectAndReturn(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0);

    status = smcf_client_element_init(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0, &mgi_conf0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Setup for smcf_client_process_new_sample() */
    ctx.amu_data_api = &fake_amu_api;

    /* Enable printing to cover the print_is_on_flag branch */
    print_is_on_flag = 1;

    /* Ensure the single AMU call returns FWK_SUCCESS */
    fake_amu_get_counter_count = 0;

    fwk_module_get_element_name_ExpectAndReturn(event.source_id, mgi_name);
    fwk_id_get_element_idx_ExpectAndReturn(event.source_id, 0);

    mli_count = 1;
    fwk_module_get_sub_element_count_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_module_get_sub_element_count_ReturnThruPtr_mod_sub_elem_cnt(&mli_count);

    status = smcf_client_process_new_sample(&event);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/* Fake sensor get_samples(); succeed once, then fail */
static int fake_sensor_get_samples(
    fwk_id_t sensor_id,
    uint32_t *values,
    size_t size)
{
    (void)sensor_id;
    (void)values;
    (void)size;

    if (fake_sensor_get_samples_call_count == 0) {
        fake_sensor_get_samples_call_count++;
        return FWK_SUCCESS;
    }

    return FWK_E_RANGE;
}

/* Fake Sensor API */
static struct mod_sensor_smcf_drv_multiple_samples_api fake_sensor_api = {
    .get_samples = fake_sensor_get_samples,
};

/**
 * @brief Verify sensor new-sample processing propagates an error when any
 *        sensor sample read fails while iterating MLIs.
 *
 * Expects:
 * - Sensor sample read to be attempted for each sensor MLI.
 * - An error returned by a sensor read to be propagated.
 */
void test_smcf_client_process_new_sample_sensor(void)
{
    int status;
    size_t smcf_elem_count;
    size_t mli_count;

    /* Event should look like it came from SMCF MGI element 0 */
    const struct fwk_event event = {
        .source_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
    };

    const char *mgi_name = "\0";

    /*
     * One MGI with two MLIs (both SENSOR) so get_samples() is called twice.
     * fake_sensor_get_samples() succeeds once and then returns FWK_E_RANGE.
     */
    static struct mod_smcf_client_mli_conf mlis[2] = {
        [0] = { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_SENSOR },
        [1] = { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_SENSOR },
    };

    static struct mod_smcf_client_mgi_conf mgi_conf0 = {
        .smcf_mgi_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
        .mlis = mlis,
    };

    /* Backing storage for ctx.mgi_conf (what fwk_mm_calloc() “allocates”) */
    static const struct mod_smcf_client_mgi_conf *mgi_conf_ptrs[1];

    /* Make init succeed and set ctx.mgi_count = 1 */
    ctx.mgi_count = 0;
    smcf_elem_count = 1;

    fwk_module_get_element_count_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF), &(ctx.mgi_count), FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&smcf_elem_count);

    fwk_mm_calloc_ExpectAndReturn(
        smcf_elem_count,
        sizeof(struct mod_smcf_client_mgi_conf *),
        (void *)mgi_conf_ptrs);

    status = smcf_client_init(fwk_module_id_smcf_client, 1, NULL);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Install element configuration (must NOT be NULL) */
    fwk_id_get_element_idx_ExpectAndReturn(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0);

    status = smcf_client_element_init(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0, &mgi_conf0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Setup for smcf_client_process_new_sample() */
    ctx.sensor_data_api = &fake_sensor_api;
    print_is_on_flag = 0;

    /* Reset fake call counter so the first call returns FWK_SUCCESS */
    fake_sensor_get_samples_call_count = 0;

    fwk_module_get_element_name_ExpectAndReturn(event.source_id, mgi_name);
    fwk_id_get_element_idx_ExpectAndReturn(event.source_id, 0);

    mli_count = 2;
    fwk_module_get_sub_element_count_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_module_get_sub_element_count_ReturnThruPtr_mod_sub_elem_cnt(&mli_count);

    status = smcf_client_process_new_sample(&event);

    /* First SENSOR get_samples() succeeds, second returns FWK_E_RANGE */
    TEST_ASSERT_EQUAL(FWK_E_RANGE, status);
}

/**
 * @brief Verify sensor new-sample processing succeeds with printing enabled
 *        when the sensor driver returns success.
 *
 * Expects:
 * - Sensor sample read to succeed for the configured MLI(s).
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_process_new_sample_sensor_success_print_on(void)
{
    int status;
    size_t smcf_elem_count;
    size_t mli_count;

    const struct fwk_event event = {
        .source_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
    };

    const char *mgi_name = "\0";

    /* One MGI with one MLI (SENSOR) -> fake_sensor_get_samples() succeeds */
    static struct mod_smcf_client_mli_conf mlis[1] = {
        [0] = { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_SENSOR },
    };

    static struct mod_smcf_client_mgi_conf mgi_conf0 = {
        .smcf_mgi_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
        .mlis = mlis,
    };

    static const struct mod_smcf_client_mgi_conf *mgi_conf_ptrs[1];

    /* Make init succeed and set ctx.mgi_count = 1 */
    ctx.mgi_count = 0;
    smcf_elem_count = 1;

    fwk_module_get_element_count_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF), &(ctx.mgi_count), FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&smcf_elem_count);

    fwk_mm_calloc_ExpectAndReturn(
        smcf_elem_count,
        sizeof(struct mod_smcf_client_mgi_conf *),
        (void *)mgi_conf_ptrs);

    status = smcf_client_init(fwk_module_id_smcf_client, 1, NULL);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Install element configuration */
    fwk_id_get_element_idx_ExpectAndReturn(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0);

    status = smcf_client_element_init(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0), 0, &mgi_conf0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Setup for smcf_client_process_new_sample() */
    ctx.sensor_data_api = &fake_sensor_api;

    /* Enable printing to cover the print_is_on_flag branch */
    print_is_on_flag = 1;

    /* Ensure the single SENSOR call returns FWK_SUCCESS */
    fake_sensor_get_samples_call_count = 0;

    fwk_module_get_element_name_ExpectAndReturn(event.source_id, mgi_name);
    fwk_id_get_element_idx_ExpectAndReturn(event.source_id, 0);

    mli_count = 1;
    fwk_module_get_sub_element_count_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_module_get_sub_element_count_ReturnThruPtr_mod_sub_elem_cnt(&mli_count);

    status = smcf_client_process_new_sample(&event);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/**
 * @brief Verify PD transition processing stops sampling for all PD states that
 *        are defined as “stop” states.
 *
 * Expects:
 * - For each stop state, sampling stop to be invoked.
 * - Function returns `FWK_SUCCESS` for each tested stop state.
 */
void test_smcf_client_process_pd_transition_stop_states_success(void)
{
    int status;

    static struct mod_pd_power_state_transition_notification_params params;
    static struct fwk_event event;
    memset(&event, 0, sizeof(event));

    /* stop_sampling_all_mgis() will stop once and succeed */
    ctx.mgi_count = 1;
    ctx.platform_sampling_api = &fake_sampling_api;

    /* All states that should trigger stop_sampling_all_mgis() */
    const unsigned int stop_states[] = {
        (unsigned int)MOD_PD_STATE_OFF,   (unsigned int)MOD_PD_STATE_OFF_0,
        (unsigned int)MOD_PD_STATE_OFF_1, (unsigned int)MOD_PD_STATE_OFF_2,
        (unsigned int)MOD_PD_STATE_SLEEP,
    };

    for (unsigned int i = 0; i < FWK_ARRAY_SIZE(stop_states); i++) {
        fake_stop_sampling_count = 0;

        params.state = stop_states[i];

        /* Copy notification params into the fwk_event params buffer */
        memcpy(event.params, &params, sizeof(params));

        status = smcf_client_process_pd_transition(&event);

        TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
        TEST_ASSERT_EQUAL(1u, fake_stop_sampling_count);
    }
}

/**
 * @brief Verify PD transition processing propagates an error if stopping
 *        sampling across MGIs fails.
 *
 * Expects:
 * - Stop-sampling to be attempted across MGIs.
 * - A stop-sampling failure to be propagated.
 */
void test_smcf_client_process_pd_transition_stop_states_fail(void)
{
    int status;

    static struct mod_pd_power_state_transition_notification_params params;
    static struct fwk_event event;
    memset(&event, 0, sizeof(event));

    /*
     * stop_sampling_all_mgis() will call stop twice (mgi_count=2).
     * fake_stop_data_sampling() succeeds once, then returns FWK_E_RANGE.
     */
    ctx.mgi_count = 2;
    ctx.platform_sampling_api = &fake_sampling_api;

    fake_stop_sampling_count = 0;

    params.state = (unsigned int)MOD_PD_STATE_OFF;
    memcpy(event.params, &params, sizeof(params));

    status = smcf_client_process_pd_transition(&event);

    TEST_ASSERT_EQUAL(FWK_E_RANGE, status);
    TEST_ASSERT_EQUAL(1u, fake_stop_sampling_count);
}

/**
 * @brief Verify PD transition processing starts sampling when transitioning to
 *        `MOD_PD_STATE_ON`.
 *
 * Expects:
 * - Start-sampling to be invoked across MGIs.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_process_pd_transition_on_success(void)
{
    int status;

    static struct mod_pd_power_state_transition_notification_params params;
    static struct fwk_event event;
    memset(&event, 0, sizeof(event));

    /* start_sampling_all_mgis() will start once and succeed */
    ctx.mgi_count = 1;
    ctx.platform_sampling_api = &fake_sampling_api;

    fake_start_sampling_count = 0;

    params.state = (unsigned int)MOD_PD_STATE_ON;
    memcpy(event.params, &params, sizeof(params));

    status = smcf_client_process_pd_transition(&event);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(1u, fake_start_sampling_count);
}

/**
 * @brief Verify PD transition processing propagates an error if starting
 *        sampling across MGIs fails.
 *
 * Expects:
 * - Start-sampling to be attempted across MGIs.
 * - A start-sampling failure to be propagated.
 */
void test_smcf_client_process_pd_transition_on_fail(void)
{
    int status;

    static struct mod_pd_power_state_transition_notification_params params;
    static struct fwk_event event;
    memset(&event, 0, sizeof(event));

    /*
     * start_sampling_all_mgis() will call start twice (mgi_count=2).
     * fake_start_data_sampling() succeeds once, then returns FWK_E_RANGE.
     */
    ctx.mgi_count = 2;
    ctx.platform_sampling_api = &fake_sampling_api;

    fake_start_sampling_count = 0;

    params.state = (unsigned int)MOD_PD_STATE_ON;
    memcpy(event.params, &params, sizeof(params));

    status = smcf_client_process_pd_transition(&event);

    TEST_ASSERT_EQUAL(FWK_E_RANGE, status);
    TEST_ASSERT_EQUAL(1u, fake_start_sampling_count);
}

/**
 * @brief Verify PD transition processing rejects an unsupported PD state and
 *        does not start or stop sampling.
 *
 * Expects:
 * - No start-sampling or stop-sampling calls to be made.
 * - Function returns `FWK_E_SUPPORT`.
 */
void test_smcf_client_process_pd_transition_unsupported_state(void)
{
    int status;

    static struct mod_pd_power_state_transition_notification_params params;
    static struct fwk_event event;
    memset(&event, 0, sizeof(event));

    ctx.mgi_count = 2;
    ctx.platform_sampling_api = &fake_sampling_api;

    fake_start_sampling_count = 0;
    fake_stop_sampling_count = 0;

    /* Pick any value not handled in the switch */
    params.state = (unsigned int)0xDEADBEEF;
    memcpy(event.params, &params, sizeof(params));

    status = smcf_client_process_pd_transition(&event);

    TEST_ASSERT_EQUAL(FWK_E_SUPPORT, status);
    TEST_ASSERT_EQUAL(0u, fake_start_sampling_count);
    TEST_ASSERT_EQUAL(0u, fake_stop_sampling_count);
}

/**
 * @brief Callback helper to compare two framework IDs for equality.
 */
static bool fwk_id_is_equal_cb(
    fwk_id_t left,
    fwk_id_t right,
    int cmock_num_calls)
{
    (void)cmock_num_calls;
    return (memcmp(&left, &right, sizeof(fwk_id_t)) == 0);
}

/**
 * @brief Verify notification processing dispatches “new sample ready” to the
 *        new-sample handler and propagates its return status.
 *
 * Expects:
 * - Notification ID to be matched to the new-sample notification.
 * - New-sample handler to run and its error code to be returned.
 */
void test_smcf_client_process_notification_new_sample_dispatch_and_propagate(
    void)
{
    int status;

    static struct fwk_event event;
    memset(&event, 0, sizeof(event));

    /* Make fwk_id_is_equal() behave deterministically for all comparisons */
    fwk_id_is_equal_StubWithCallback(fwk_id_is_equal_cb);

    event.id = mod_smcf_notification_id_new_data_sample_ready;
    event.source_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0);

    /* Expectations inside smcf_client_process_new_sample() */
    fwk_module_get_element_name_ExpectAndReturn(event.source_id, "\0");
    fwk_id_get_element_idx_ExpectAndReturn(event.source_id, 0);

    /* Force early failure before any ctx.mgi_conf dereference */
    fwk_module_get_sub_element_count_ExpectAnyArgsAndReturn(FWK_E_PARAM);

    status = smcf_client_process_notification(&event, NULL);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

/**
 * @brief Verify notification processing dispatches PD transition notifications
 *        to the PD transition handler and succeeds when sampling start
 *        succeeds.
 *
 * Expects:
 * - Notification ID to be matched to the PD transition notification.
 * - Start-sampling to be invoked (via PD transition handling).
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_process_notification_pd_transition_dispatch_and_success(
    void)
{
    int status;

    static struct mod_pd_power_state_transition_notification_params params;
    static struct fwk_event event;
    memset(&event, 0, sizeof(event));

    fwk_id_is_equal_StubWithCallback(fwk_id_is_equal_cb);

    event.id = pd_transition_notification_id;

    /* PD transition payload in event.params */
    params.state = (unsigned int)MOD_PD_STATE_ON;
    memcpy(event.params, &params, sizeof(params));

    /* start_sampling_all_mgis() will start once and succeed */
    ctx.mgi_count = 1;
    ctx.platform_sampling_api = &fake_sampling_api;
    fake_start_sampling_count = 0;

    status = smcf_client_process_notification(&event, NULL);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(1u, fake_start_sampling_count);
}

/**
 * @brief Verify notification processing dispatches PD transition notifications
 *        to the PD transition handler and propagates failures from sampling
 *        start.
 *
 * Expects:
 * - Notification ID to be matched to the PD transition notification.
 * - A start-sampling failure (via PD transition handling) to be propagated.
 */
void test_smcf_client_process_notification_pd_transition_dispatch_and_fail(void)
{
    int status;

    static struct mod_pd_power_state_transition_notification_params params;
    static struct fwk_event event;
    memset(&event, 0, sizeof(event));

    fwk_id_is_equal_StubWithCallback(fwk_id_is_equal_cb);

    event.id = pd_transition_notification_id;

    /*
     * start_sampling_all_mgis() will call start twice (mgi_count=2).
     * fake_start_data_sampling() succeeds once, then returns FWK_E_RANGE.
     */
    ctx.mgi_count = 2;
    ctx.platform_sampling_api = &fake_sampling_api;
    fake_start_sampling_count = 0;

    params.state = (unsigned int)MOD_PD_STATE_ON;
    memcpy(event.params, &params, sizeof(params));

    status = smcf_client_process_notification(&event, NULL);

    TEST_ASSERT_EQUAL(FWK_E_RANGE, status);
    TEST_ASSERT_EQUAL(1u, fake_start_sampling_count);
}

/**
 * @brief Verify notification processing rejects unknown notification IDs.
 *
 * Expects:
 * - No dispatch to new-sample or PD transition handlers to occur.
 * - No sampling start/stop operations to be performed.
 * - Function returns `FWK_E_SUPPORT`.
 */
void test_smcf_client_process_notification_unknown_notification(void)
{
    int status;

    static struct fwk_event event;
    memset(&event, 0, sizeof(event));

    fwk_id_is_equal_StubWithCallback(fwk_id_is_equal_cb);

    /*
     * Any ID that isn't one of the two supported notification IDs.
     * Using a client-module notification with an arbitrary idx is fine.
     */
    event.id =
        FWK_ID_NOTIFICATION(FWK_MODULE_IDX_SMCF_CLIENT, (unsigned char)42);

    fake_start_sampling_count = 0;
    fake_stop_sampling_count = 0;

    status = smcf_client_process_notification(&event, NULL);

    TEST_ASSERT_EQUAL(FWK_E_SUPPORT, status);
    TEST_ASSERT_EQUAL(0u, fake_start_sampling_count);
    TEST_ASSERT_EQUAL(0u, fake_stop_sampling_count);
}

/**
 * @brief Verify bind request processing returns the module control API when the
 *        requested API index is the control API.
 *
 * Expects:
 * - API index decode to identify the control API.
 * - Returned API pointer to be `&smcf_client_ctrl_api`.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_process_bind_request_control_api_success(void)
{
    int status;
    const void *api = NULL;

    fwk_id_t requester_id = FWK_ID_MODULE(FWK_MODULE_IDX_SMCF_CLIENT);
    fwk_id_t pd_id = FWK_ID_NONE;
    fwk_id_t api_id = FWK_ID_API(FWK_MODULE_IDX_SMCF_CLIENT, 0);

    /* Force api index to CONTROL */
    fwk_id_get_api_idx_ExpectAndReturn(
        api_id, (unsigned int)MOD_SMCF_CLIENT_API_IDX_CONTROL);

    status =
        smcf_client_process_bind_request(requester_id, pd_id, api_id, &api);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&smcf_client_ctrl_api, api);
}

/**
 * @brief Verify bind request processing rejects unsupported API indices and
 *        does not modify the provided API pointer.
 *
 * Expects:
 * - API index decode to yield a non-control value.
 * - Function returns `FWK_E_PARAM`.
 * - Output API pointer remains unchanged.
 */
void test_smcf_client_process_bind_request_invalid_api_fails(void)
{
    int status;

    /* Sentinel to check it is not modified on error */
    const void *api = (const void *)0xDEADBEEF;

    fwk_id_t requester_id = FWK_ID_MODULE(FWK_MODULE_IDX_SMCF_CLIENT);
    fwk_id_t pd_id = FWK_ID_NONE;
    fwk_id_t api_id = FWK_ID_API(FWK_MODULE_IDX_SMCF_CLIENT, 0);

    /* Any value != CONTROL triggers default -> FWK_E_PARAM */
    fwk_id_get_api_idx_ExpectAndReturn(
        api_id, (unsigned int)MOD_SMCF_CLIENT_API_COUNT);

    status =
        smcf_client_process_bind_request(requester_id, pd_id, api_id, &api);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
    TEST_ASSERT_EQUAL_PTR((const void *)0xDEADBEEF, api);
}

#endif /* BUILD_HAS_NOTIFICATION */

/**
 * @brief Verify start returns success when invoked with an element ID (element
 *        start path is a no-op in this test).
 *
 * Expects:
 * - ID type check to report an element ID.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_start_element_id_returns_success(void)
{
    int status;
    fwk_id_t id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF_CLIENT, 0);

    fwk_id_is_type_ExpectAndReturn(id, FWK_ID_TYPE_ELEMENT, true);

    status = smcf_client_start(id);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/**
 * @brief Verify start propagates failures from querying the module element
 *        count when invoked with the module ID.
 *
 * Expects:
 * - ID type check to report a non-element ID.
 * - Module element count query to fail.
 * - Function returns the same error code.
 */
void test_smcf_client_start_get_element_count_fail(void)
{
    int status;
    fwk_id_t id = FWK_ID_MODULE(FWK_MODULE_IDX_SMCF_CLIENT);

    fwk_id_is_type_ExpectAndReturn(id, FWK_ID_TYPE_ELEMENT, false);

    fwk_module_get_element_count_ExpectAnyArgsAndReturn(FWK_E_PARAM);

    status = smcf_client_start(id);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

/**
 * @brief Verify start succeeds and performs no subscriptions when the module
 *        has zero elements.
 *
 * Expects:
 * - Module element count to be reported as zero.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_start_no_elements_returns_success(void)
{
    int status;
    fwk_id_t id = FWK_ID_MODULE(FWK_MODULE_IDX_SMCF_CLIENT);
    size_t element_count = 0;

    fwk_id_is_type_ExpectAndReturn(id, FWK_ID_TYPE_ELEMENT, false);

    fwk_module_get_element_count_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&element_count);

    status = smcf_client_start(id);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/**
 * @brief Verify start fails when subscribing to the PD transition notification
 *        fails, and triggers the error-path element-name lookup.
 *
 * Expects:
 * - PD transition notification subscription to fail.
 * - Module element name to be queried for logging on the error path.
 * - Function returns the same subscription error code.
 */
void test_smcf_client_start_pd_transition_subscribe_fail(void)
{
    int status;
    fwk_id_t id = FWK_ID_MODULE(FWK_MODULE_IDX_SMCF_CLIENT);
    size_t element_count = 1;

    fwk_id_is_type_ExpectAndReturn(id, FWK_ID_TYPE_ELEMENT, false);

    fwk_module_get_element_count_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&element_count);

    /* Build PD source ID (exact value doesn't matter for this test) */
    fwk_id_build_element_id_ExpectAndReturn(
        fwk_module_id_power_domain, PD_STATIC_DEV_IDX_SYSTOP, FWK_ID_NONE);

    /* Fail the PD subscription */
    fwk_notification_subscribe_ExpectAndReturn(
        pd_transition_notification_id, FWK_ID_NONE, id, FWK_E_ACCESS);

    /* Called on the error path for logging */
    fwk_module_get_element_name_ExpectAndReturn(id, "\0");

    status = smcf_client_start(id);

    TEST_ASSERT_EQUAL(FWK_E_ACCESS, status);
}

/**
 * @brief Verify start fails when subscribing to an MGI “new data sample ready”
 *        notification fails after successfully subscribing to PD transitions.
 *
 * Expects:
 * - PD transition subscription to succeed.
 * - Subscription to at least one MGI new-sample notification to be attempted.
 * - A failing MGI subscription error to be returned.
 */
void test_smcf_client_start_mgi_subscribe_fail(void)
{
    int status;
    fwk_id_t id = FWK_ID_MODULE(FWK_MODULE_IDX_SMCF_CLIENT);
    size_t element_count = 1;

    fwk_id_is_type_ExpectAndReturn(id, FWK_ID_TYPE_ELEMENT, false);

    fwk_module_get_element_count_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&element_count);

    /* Need at least 1 MGI subscription attempt */
    ctx.mgi_count = 2;

    fwk_id_build_element_id_ExpectAndReturn(
        fwk_module_id_power_domain, PD_STATIC_DEV_IDX_SYSTOP, FWK_ID_NONE);

    /*
     * Subscribe sequence:
     *  1) PD transition subscribe -> success
     *  2) MGI0 new-sample subscribe -> success
     *  3) MGI1 new-sample subscribe -> fail (function must return this)
     */
    fwk_notification_subscribe_ExpectAndReturn(
        pd_transition_notification_id, FWK_ID_NONE, id, FWK_SUCCESS);
    fwk_notification_subscribe_ExpectAndReturn(
        mod_smcf_notification_id_new_data_sample_ready,
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
        id,
        FWK_SUCCESS);
    fwk_notification_subscribe_ExpectAndReturn(
        mod_smcf_notification_id_new_data_sample_ready,
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 1),
        id,
        FWK_E_RANGE);

    status = smcf_client_start(id);

    TEST_ASSERT_EQUAL(FWK_E_RANGE, status);
}

/**
 * @brief Verify start subscribes to PD transition notifications and to the
 *        per-MGI “new data sample ready” notifications when all subscriptions
 *        succeed.
 *
 * Expects:
 * - PD transition notification subscription to succeed.
 * - New-sample notification subscription to succeed for each configured MGI.
 * - Function returns `FWK_SUCCESS`.
 */
void test_smcf_client_start_success_subscribes_all(void)
{
    int status;
    fwk_id_t id = FWK_ID_MODULE(FWK_MODULE_IDX_SMCF_CLIENT);
    size_t element_count = 1;

    fwk_id_is_type_ExpectAndReturn(id, FWK_ID_TYPE_ELEMENT, false);

    fwk_module_get_element_count_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fwk_module_get_element_count_ReturnThruPtr_mod_elem_count(&element_count);

    ctx.mgi_count = 2;

    fwk_id_build_element_id_ExpectAndReturn(
        fwk_module_id_power_domain, PD_STATIC_DEV_IDX_SYSTOP, FWK_ID_NONE);

    /* 1 PD + 2 MGIs */
    fwk_notification_subscribe_ExpectAndReturn(
        pd_transition_notification_id, FWK_ID_NONE, id, FWK_SUCCESS);
    fwk_notification_subscribe_ExpectAndReturn(
        mod_smcf_notification_id_new_data_sample_ready,
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
        id,
        FWK_SUCCESS);
    fwk_notification_subscribe_ExpectAndReturn(
        mod_smcf_notification_id_new_data_sample_ready,
        FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 1),
        id,
        FWK_SUCCESS);

    status = smcf_client_start(id);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

int smcf_client_test_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_smcf_client_init_success);
    RUN_TEST(test_smcf_client_init_fail_no_element_count);
    RUN_TEST(test_smcf_client_init_success_zero_elements);
    RUN_TEST(test_smcf_client_init_fail_invalid_element_count);
    RUN_TEST(test_smcf_client_element_init);
    RUN_TEST(test_smcf_client_start_sampling_all_mgis_success);
    RUN_TEST(test_smcf_client_start_sampling_all_mgis_fail);
    RUN_TEST(test_smcf_client_start_sampling_all_mgis_zero_mgis);
    RUN_TEST(test_smcf_client_stop_sampling_all_mgis_success);
    RUN_TEST(test_smcf_client_stop_sampling_all_mgis_fail);
    RUN_TEST(test_smcf_client_stop_sampling_all_mgis_zero_mgis);
    RUN_TEST(test_smcf_client_toggle_print);
    RUN_TEST(test_platform_smcf_bind_round1);
    RUN_TEST(test_smcf_client_bind_round0_success);
    RUN_TEST(test_smcf_client_bind_fail_bind_to_platform_smcf);
    RUN_TEST(test_smcf_client_bind_fail_bind_to_smcf);
    RUN_TEST(test_smcf_client_bind_fail_bind_to_amu_smcf_drv);
    RUN_TEST(test_smcf_client_bind_fail_bind_to_sensor_smcf_drv);
    RUN_TEST(test_smcf_client_process_new_sample_fail_get_sub_element_count);
    RUN_TEST(test_smcf_client_process_new_sample_fail_mgi_idx_out_of_range);
    RUN_TEST(test_smcf_client_process_new_sample_fail_unsupported_mli_type);
    RUN_TEST(test_smcf_client_process_new_sample_amu);
    RUN_TEST(test_smcf_client_process_new_sample_amu_success_print_on);
    RUN_TEST(test_smcf_client_process_new_sample_sensor);
    RUN_TEST(test_smcf_client_process_new_sample_sensor_success_print_on);
    RUN_TEST(test_smcf_client_process_pd_transition_stop_states_success);
    RUN_TEST(test_smcf_client_process_pd_transition_stop_states_fail);
    RUN_TEST(test_smcf_client_process_pd_transition_on_success);
    RUN_TEST(test_smcf_client_process_pd_transition_on_fail);
    RUN_TEST(test_smcf_client_process_pd_transition_unsupported_state);
    RUN_TEST(
        test_smcf_client_process_notification_new_sample_dispatch_and_propagate);
    RUN_TEST(
        test_smcf_client_process_notification_pd_transition_dispatch_and_success);
    RUN_TEST(
        test_smcf_client_process_notification_pd_transition_dispatch_and_fail);
    RUN_TEST(test_smcf_client_process_notification_unknown_notification);
    RUN_TEST(test_smcf_client_process_bind_request_control_api_success);
    RUN_TEST(test_smcf_client_process_bind_request_invalid_api_fails);
    RUN_TEST(test_smcf_client_start_element_id_returns_success);
    RUN_TEST(test_smcf_client_start_get_element_count_fail);
    RUN_TEST(test_smcf_client_start_no_elements_returns_success);
    RUN_TEST(test_smcf_client_start_pd_transition_subscribe_fail);
    RUN_TEST(test_smcf_client_start_mgi_subscribe_fail);
    RUN_TEST(test_smcf_client_start_success_subscribes_all);

    return UNITY_END();
}

#if !defined(TEST_ON_TARGET)
int main(void)
{
    return smcf_client_test_main();
}
#endif
