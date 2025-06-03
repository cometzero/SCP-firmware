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
    status = mod_ras_handler_init(
        fwk_module_id_ras_handlers, IPCOUNT, NULL);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

void test_ras_handlers_failed_init(void)
{
    int status;

    /* Tests the IP count sanity */
    status = mod_ras_handler_init(
        fwk_module_id_ras_handlers,0,NULL);
    TEST_ASSERT_EQUAL(FWK_E_PARAM, status);
}

void test_ras_handlers_search_by_intr_id(void)
{
    int status;
    status = mod_ras_handler_init(
        fwk_module_id_ras_handlers, IPCOUNT, NULL);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    /* Simulate Element Initialisation */
    for (unsigned int idx = RAS_CLUSTERX_IP_IDX; idx < IPCOUNT; idx++) {
        fwk_id_t element_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_TEST_MODULE, idx);
        fwk_id_get_element_idx_ExpectAndReturn(element_id, idx);

        status = mod_ras_handler_elements_init(element_id, IPCOUNT,
            &valid_intr_desc[idx]);
        TEST_ASSERT_EQUAL(FWK_SUCCESS,status);

        unsigned fidx = find_descriptor_idx(TEST_RAS_CLUSTERX_INTR_IDX + idx);
        /* The result should be the same index since we are searching the same list*/
        TEST_ASSERT_EQUAL(idx,fidx);
    }

}

int ras_handlers_test_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_ras_handlers_init);
    RUN_TEST(test_ras_handlers_failed_init);
    RUN_TEST(test_ras_handlers_search_by_intr_id);

    return UNITY_END();
}

#if !defined(TEST_ON_TARGET)
int main(void)
{
    return ras_handlers_test_main();
}
#endif
