/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "unity.h"

#include <Mockfwk_id.h>
#include <Mockfwk_module.h>

#include <fwk_module_idx.h>
#include UNIT_TEST_SRC
#include "config_ras_defines.h"

#include "si0_cfgd_ssu.h"
#include "si0_cfgd_transport.h"

void setUp(void)
{
    /* Do Nothing */
}

void tearDown(void)
{
    /* Do Nothing */
}

void test_ras_handlers_init(void)
{
    int status;

    /* Tests the IP count sanity */
    status = mod_ras_handler_init(fwk_module_id_ras_handlers, IPCOUNT, NULL);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

void test_ras_handlers_failed_init(void)
{
    int status;

    /* Tests the IP count sanity */
    status = mod_ras_handler_init(fwk_module_id_ras_handlers, 0, NULL);
    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

void test_ras_handlers_search_by_intr_id(void)
{
    int status;
    status = mod_ras_handler_init(fwk_module_id_ras_handlers, IPCOUNT , NULL);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Simulate Element Initialisation */
    for (unsigned int idx = RAS_CLUSTERX_IP_IDX; idx < IPCOUNT; idx++) {
        fwk_id_t element_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_TEST_MODULE, idx);
        fwk_id_get_element_idx_ExpectAndReturn(element_id, idx);

        status = mod_ras_handler_elements_init(
            element_id, IPCOUNT, &valid_intr_desc[idx]);
        TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

        unsigned fidx = find_descriptor_idx(TEST_RAS_CLUSTERX_INTR_IDX + idx);
        /* The result should be the same index since we are searching the same
         * list*/
        TEST_ASSERT_EQUAL(idx, fidx);
    }

}

void test_ras_handlers_request_bind_success(void)
{
    fwk_id_get_api_idx_ExpectAnyArgsAndReturn(0);
    fwk_id_t signal_api_id =
        FWK_ID_API_INIT(FWK_MODULE_IDX_RAS_HANDLERS, MOD_RAS_API_IDX_SIGNALS);

    const void *api_out = (void *)0x0;
    int status = ras_handler_bind_request(
        fwk_module_id_transport,
        fwk_module_id_ras_handlers,
        signal_api_id,
        &api_out);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

void test_ras_handlers_request_bind_fail(void)
{
    fwk_id_get_api_idx_ExpectAnyArgsAndReturn(-1);
    /* Add a fake API index in this case lets consider count */
    fwk_id_t signal_api_id = FWK_ID_API_INIT(FWK_MODULE_IDX_RAS_HANDLERS, 1);

    const void *api_out = (void *)0x0;
    int status = ras_handler_bind_request(
        fwk_module_id_transport,
        fwk_module_id_ras_handlers,
        signal_api_id,
        &api_out);

    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

void test_bind_all_binds_success(void)
{
    struct mod_ras_config cfg = {
        .ssu_sys_elem_id =
            FWK_ID_ELEMENT(FWK_MODULE_IDX_SSU, CONFIG_SSU_ELEMENT_IDX),
        .timer_elem_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
        .transport_elem_id = FWK_ID_ELEMENT_INIT(
            FWK_MODULE_IDX_TRANSPORT, SI0_CFGD_MOD_TRANSPORT_EIDX_RAS),
    };

    ras_ctx.ras_config = &cfg;

    // Stub the type-check to always pass
    fwk_id_is_type_ExpectAnyArgsAndReturn(true);
    fwk_id_is_type_ExpectAnyArgsAndReturn(true);
    fwk_id_is_type_ExpectAnyArgsAndReturn(true);

    fwk_module_bind_ExpectAnyArgsAndReturn(FWK_SUCCESS);

    fwk_module_bind_ExpectAnyArgsAndReturn(FWK_SUCCESS);

    fwk_module_bind_ExpectAnyArgsAndReturn(FWK_SUCCESS);


    int result = ras_handler_bind(fwk_module_id_ras_handlers, 0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, result);
}

int ras_handlers_test_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_ras_handlers_init);
    RUN_TEST(test_ras_handlers_failed_init);
    RUN_TEST(test_ras_handlers_search_by_intr_id);
    RUN_TEST(test_ras_handlers_request_bind_success);
    RUN_TEST(test_ras_handlers_request_bind_fail);
    RUN_TEST(test_bind_all_binds_success);
    /* TODO Request Bind and Bind functions need to be tested */

    return UNITY_END();
}

#if !defined(TEST_ON_TARGET)
int main(void)
{
    return ras_handlers_test_main();
}
#endif
