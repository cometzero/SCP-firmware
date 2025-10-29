/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <si0_smcf.h>

#include <mod_integration_test.h>
#include <mod_smcf_client.h>

#include <fwk_core.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_notification.h>
#include <fwk_time.h>

#include <string.h>

#define MOD_NAME "[TEST_SMCF] "

static struct mod_smcf_client_control_api *ctrl_api;

static int run(
    unsigned int case_idx,
    unsigned int step_idx,
    const struct fwk_event *event)
{
    int status = ctrl_api->toggle_print();
    FWK_LOG_INFO(MOD_NAME "Set SMCF print to %s", status ? "on" : "off");
    return FWK_SUCCESS;
}

static const char *test_name(unsigned int case_idx)
{
    return "test_smcf";
}

void test_smcf_setup(void)
{
}

void test_smcf_teardown(void)
{
}

static const struct mod_integration_test_api test_api = {
    .run = run,
    .test_name = test_name,
    .setup = test_smcf_setup,
    .teardown = test_smcf_teardown
};

static int test_smcf_init(
    fwk_id_t module_id,
    unsigned int unused,
    const void *data)
{
    return FWK_SUCCESS;
}

static int test_smcf_start(fwk_id_t id)
{
    return FWK_SUCCESS;
}

static int test_smcf_process_bind_request(
    fwk_id_t requester_id,
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void **api)
{
    if (fwk_id_get_api_idx(api_id) != MOD_INTEGRATION_TEST_API_IDX_TEST) {
        return FWK_E_PARAM;
    }
    *api = &test_api;
    return FWK_SUCCESS;
}

static int test_smcf_bind(fwk_id_t id, unsigned int round)
{
    int status;

    /* Only bind in the first round of calls */
    if (round > 0) {
        return FWK_SUCCESS;
    }

    status = fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF_CLIENT),
        FWK_ID_API(FWK_MODULE_IDX_SMCF_CLIENT, MOD_SMCF_CLIENT_API_IDX_CONTROL),
        &ctrl_api);
    if (status != FWK_SUCCESS) {
        fwk_unexpected();
        return status;
    }
    return FWK_SUCCESS;
}

#ifdef BUILD_HAS_NOTIFICATION
static int test_smcf_process_notification(
    const struct fwk_event *event,
    struct fwk_event *resp_event)
{
    return FWK_SUCCESS;
}
#endif /* BUILD_HAS_NOTIFICATION */

struct fwk_module_config config_test_smcf = { 0 };

const struct fwk_module module_test_smcf = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .init = test_smcf_init,
    .start = test_smcf_start,
    .process_bind_request = test_smcf_process_bind_request,
    .api_count = MOD_INTEGRATION_TEST_API_COUNT,
    .bind = test_smcf_bind,
#ifdef BUILD_HAS_NOTIFICATION
    .process_notification = test_smcf_process_notification,
#endif /* BUILD_HAS_NOTIFICATION */
};
