/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "scp_unity.h"
#include "unity.h"

#include <Mockfwk_interrupt.h>
#include <Mockfwk_module.h>
#include <Mockfwk_notification.h>

#include <fwk_macros.h>

#include UNIT_TEST_SRC

#include <mod_platform_smcf.h>

/* Track calls + last arguments + configurable return status */
static int fake_start_calls;
static int fake_stop_calls;
static fwk_id_t fake_start_element_id;
static fwk_id_t fake_stop_element_id;
static int fake_start_return_status = FWK_SUCCESS;
static int fake_stop_return_status = FWK_SUCCESS;

void setUp(void)
{
    /* Do nothing */
}

void tearDown(void)
{
    /* Do nothing */
}

/**
 * @brief Verify that module initialization succeeds.
 *
 * Calls `platform_smcf_mod_init()` with the platform-smcf module ID and asserts
 * that it returns `FWK_SUCCESS`.
 */
void test_platform_smcf_init(void)
{
    int status;
    status = platform_smcf_mod_init(fwk_module_id_platform_smcf, 0, NULL);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/**
 * @brief Verify that bind round 0 binds the SMCF Data API successfully.
 *
 * Expects a 'fwk_module_bind()` call to the SMCF module's Data API and
 * configures the mock to return `FWK_SUCCESS`. Then calls
 * `platform_smcf_bind()` with `round = 0` and asserts `FWK_SUCCESS`.
 */
void test_platform_smcf_bind_round0_success(void)
{
    int status;
    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF),
        FWK_ID_API(FWK_MODULE_IDX_SMCF, MOD_SMCF_API_IDX_DATA),
        &platform_smcf_ctx.data_api,
        FWK_SUCCESS);
    status = platform_smcf_bind(fwk_module_id_platform_smcf, 0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/**
 * @brief Verify that bind round 0 propagates `fwk_module_bind()` failure.
 *
 * Expects the same `fwk_module_bind()` call as the success case, but configures
 * the mock to return `FWK_E_PARAM`. Verifies `platform_smcf_bind()` returns the
 * same error when invoked with `round = 0`.
 */
void test_platform_smcf_bind_round0_fail(void)
{
    int status;
    fwk_module_bind_ExpectAndReturn(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF),
        FWK_ID_API(FWK_MODULE_IDX_SMCF, MOD_SMCF_API_IDX_DATA),
        &platform_smcf_ctx.data_api,
        FWK_E_PARAM);
    status = platform_smcf_bind(fwk_module_id_platform_smcf, 0);
    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

/**
 * @brief Verify that bind rounds greater than 0 are a no-op and succeed.
 *
 * Calls `platform_smcf_bind()` with `round = 1` and asserts `FWK_SUCCESS`.
 * No mock expectations are set; the test implicitly checks that no binding
 * attempt is made in later rounds.
 */
void test_platform_smcf_bind_round1(void)
{
    int status;
    status = platform_smcf_bind(fwk_module_id_platform_smcf, 1);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

/**
 * @brief Verify that the supported platform sampling API can be bound and the
 * API pointer is written.
 *
 * Initializes the output pointer (`api`) to a sentinel value, calls
 * `platform_smcf_process_bind_request()` for
 * `MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API`, then verifies:
 * - the function returns `FWK_SUCCESS`
 * - the output pointer is updated to `&platform_smcf_sampling_api`
 */
void test_platform_smcf_bind_request_success(void)
{
    int status;

    /* Set a sentinel to prove the function actually writes *api */
    const void *api = (const void *)0x1;

    status = platform_smcf_process_bind_request(
        FWK_ID_MODULE(FWK_MODULE_IDX_PLATFORM_SMCF),
        FWK_ID_MODULE(FWK_MODULE_IDX_PLATFORM_SMCF),
        FWK_ID_API(
            FWK_MODULE_IDX_PLATFORM_SMCF,
            MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API),
        &api);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Must return the sampling API table */
    TEST_ASSERT_EQUAL_PTR(&platform_smcf_sampling_api, api);
}

/**
 * @brief Verify that an unsupported platform API bind request is rejected and
 * does not modify the output pointer.
 *
 * Initializes the output pointer (`api`) to a sentinel value and calls
 * `platform_smcf_process_bind_request()` with an unsupported API index
 * (`MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API + 1`). Verifies:
 * - the function returns `FWK_E_PARAM`
 * - the output pointer remains unchanged (still the sentinel)
 */
void test_platform_smcf_bind_request_fail(void)
{
    int status;

    const void *api = (const void *)0x1; /* sentinel */

    status = platform_smcf_process_bind_request(
        FWK_ID_MODULE(FWK_MODULE_IDX_PLATFORM_SMCF),
        FWK_ID_MODULE(FWK_MODULE_IDX_PLATFORM_SMCF),
        FWK_ID_API(
            FWK_MODULE_IDX_PLATFORM_SMCF,
            MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API + 1),
        &api);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);

    /* Ensure it didn't scribble on output for unsupported API */
    TEST_ASSERT_EQUAL_PTR((const void *)0x1, api);
}

/**
 * @brief Fake `start_data_sampling()` implementation used by sampling wrapper
 * tests.
 *
 * Records that it was called (increments `fake_start_calls`), stores the passed
 * `element_id` in `fake_start_element_id`, and returns
 * `fake_start_return_status` so tests can validate both success and error
 * propagation paths.
 *
 * @param element_id Element identifier forwarded by the wrapper.
 * @return The configured status in `fake_start_return_status`.
 */
static int fake_start_data_sampling(fwk_id_t element_id)
{
    fake_start_calls++;
    fake_start_element_id = element_id;
    return fake_start_return_status;
}

/**
 * @brief Fake `stop_data_sampling()` implementation used by sampling wrapper
 * tests.
 *
 * Records that it was called (increments `fake_stop_calls`), stores the passed
 * `element_id` in `fake_stop_element_id`, and returns `fake_stop_return_status`
 * so tests can validate both success and error propagation paths.
 *
 * @param element_id Element identifier forwarded by the wrapper.
 * @return The configured status in `fake_stop_return_status`.
 */
static int fake_stop_data_sampling(fwk_id_t element_id)
{
    fake_stop_calls++;
    fake_stop_element_id = element_id;
    return fake_stop_return_status;
}

static struct smcf_data_api fake_data_api = {
    .start_data_sampling = fake_start_data_sampling,
    .stop_data_sampling = fake_stop_data_sampling,
};

/**
 * @brief Verify that `smcf_start_sampling()` forwards the element ID and
 * propagates the underlying return status.
 *
 * Sets `platform_smcf_ctx.data_api` to a fake API that records calls/arguments.
 * Exercises two paths:
 * - Success: fake returns `FWK_SUCCESS` and wrapper must return `FWK_SUCCESS`,
 *   call exactly once, and forward `element_id` unchanged.
 * - Error propagation: fake returns `FWK_E_PARAM` and wrapper must return
 *   `FWK_E_PARAM`, still calling exactly once and forwarding `element_id`.
 */
void test_platform_smcf_start_sampling(void)
{
    int status;
    fwk_id_t element_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0);

    platform_smcf_ctx.data_api = &fake_data_api;

    /* Success path: validates call-through and argument passing */
    fake_start_calls = 0;
    fake_start_return_status = FWK_SUCCESS;

    status = smcf_start_sampling(element_id);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(1, fake_start_calls);
    TEST_ASSERT_EQUAL_MEMORY(
        &element_id, &fake_start_element_id, sizeof(fwk_id_t));

    /* Error propagation path */
    fake_start_calls = 0;
    fake_start_return_status = FWK_E_PARAM;

    status = smcf_start_sampling(element_id);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
    TEST_ASSERT_EQUAL(1, fake_start_calls);
    TEST_ASSERT_EQUAL_MEMORY(
        &element_id, &fake_start_element_id, sizeof(fwk_id_t));
}

/**
 * @brief Verify that `smcf_stop_sampling()` forwards the element ID and
 * propagates the underlying return status.
 *
 * Sets `platform_smcf_ctx.data_api` to a fake API that records calls/arguments.
 * Exercises two paths:
 * - Success: fake returns `FWK_SUCCESS` and wrapper must return `FWK_SUCCESS`,
 *   call exactly once, and forward `element_id` unchanged.
 * - Error propagation: fake returns `FWK_E_PARAM` and wrapper must return
 *   `FWK_E_PARAM`, still calling exactly once and forwarding `element_id`.
 */
void test_platform_smcf_stop_sampling(void)
{
    int status;
    fwk_id_t element_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0);

    platform_smcf_ctx.data_api = &fake_data_api;

    /* Success path: validates call-through and argument passing */
    fake_stop_calls = 0;
    fake_stop_return_status = FWK_SUCCESS;

    status = smcf_stop_sampling(element_id);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(1, fake_stop_calls);
    TEST_ASSERT_EQUAL_MEMORY(
        &element_id, &fake_stop_element_id, sizeof(fwk_id_t));

    /* Error propagation path */
    fake_stop_calls = 0;
    fake_stop_return_status = FWK_E_PARAM;

    status = smcf_stop_sampling(element_id);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
    TEST_ASSERT_EQUAL(1, fake_stop_calls);
    TEST_ASSERT_EQUAL_MEMORY(
        &element_id, &fake_stop_element_id, sizeof(fwk_id_t));
}

int platform_smcf_test_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_platform_smcf_init);
    RUN_TEST(test_platform_smcf_bind_round0_success);
    RUN_TEST(test_platform_smcf_bind_round0_fail);
    RUN_TEST(test_platform_smcf_bind_round1);
    RUN_TEST(test_platform_smcf_bind_request_success);
    RUN_TEST(test_platform_smcf_bind_request_fail);
    RUN_TEST(test_platform_smcf_start_sampling);
    RUN_TEST(test_platform_smcf_stop_sampling);

    return UNITY_END();
}

#if !defined(TEST_ON_TARGET)
int main(void)
{
    return platform_smcf_test_main();
}
#endif
