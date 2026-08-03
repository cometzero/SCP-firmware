/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <unity.h>

#include <mod_gicx00_multiview.h>
#include <mod_integration_test.h>
#include <mod_power_domain.h>

#include <fwk_core.h>
#include <fwk_event.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_status.h>

#include <stdbool.h>

#define TESTED_PE_COUNT        4U
#define GIC_AP_ELEMENT_IDX     1U
#define TEST_INTERRUPT_ID_BASE 988U
#define WARM_RESET_POLL_LIMIT  10000U
#define POWER_TEST_CASE_COUNT  ((2U * TESTED_PE_COUNT) + 2U)

enum test_gic_power_scenario {
    TEST_GIC_POWER_SCENARIO_PWRR,
    TEST_GIC_POWER_SCENARIO_WAKER,
    TEST_GIC_POWER_SCENARIO_PWRR_TIMEOUT_NEGATIVE,
    TEST_GIC_POWER_SCENARIO_PENDING_WARM_RESET,
    TEST_GIC_POWER_SCENARIO_COUNT,
};

static const struct mod_gicx00_multiview_power_api *gic_power_api;
static const struct mod_pd_restricted_api *pd_api;
static unsigned int warm_reset_poll_count;

struct marker_context {
    const char *scenario;
    const char *phase;
    unsigned int pe;
    unsigned int interrupt_id;
    int error;
};

static const char *scenario_name(enum test_gic_power_scenario scenario)
{
    static const char *const names[] = {
        [TEST_GIC_POWER_SCENARIO_PWRR] = "pwrr",
        [TEST_GIC_POWER_SCENARIO_WAKER] = "waker",
        [TEST_GIC_POWER_SCENARIO_PWRR_TIMEOUT_NEGATIVE] =
            "pwrr-timeout-negative",
        [TEST_GIC_POWER_SCENARIO_PENDING_WARM_RESET] = "pending-warm-reset",
    };

    if (scenario >= TEST_GIC_POWER_SCENARIO_COUNT) {
        return NULL;
    }

    return names[scenario];
}

static void emit_start_record(const char *scenario, unsigned int pe)
{
    fwk_log_printf(
        "GIC720AE_POWER_START scenario=%s pe=%u pass=1", scenario, pe);
}

static void emit_ready_record(const char *scenario, unsigned int pe)
{
    fwk_log_printf(
        "GIC720AE_POWER_READY scenario=%s pe=%u pass=1", scenario, pe);
}

static void emit_done_record(
    const char *scenario,
    unsigned int pe,
    bool pass)
{
    fwk_log_printf(
        "GIC720AE_POWER_DONE scenario=%s pe=%u pass=%u",
        scenario,
        pe,
        pass ? 1U : 0U);
}

static int emit_state_record(
    fwk_id_t gic_id,
    unsigned int redistributor_idx,
    const struct marker_context *marker)
{
    int interrupt_status;
    int redistributor_status;
    struct mod_gicx00_multiview_interrupt_state interrupt_state = { 0 };
    struct mod_gicx00_multiview_redistributor_state redistributor_state = { 0 };

    redistributor_status = gic_power_api->read_redistributor_state(
        gic_id, redistributor_idx, &redistributor_state);
    interrupt_status = gic_power_api->read_interrupt_state(
        gic_id, marker->interrupt_id, &interrupt_state);

    fwk_log_printf(
        "GIC720AE_POWER_STATE scenario=%s pe=%u phase=%s observed_power=%u "
        "observed_processor_sleep=%u observed_children_asleep=%u "
        "observed_pending=%u observed_active=%u "
        "raw_pwrr=0x%08x raw_waker=0x%08x raw_pending=0x%08x "
        "raw_active=0x%08x error=%d timeout=%u",
        marker->scenario,
        marker->pe,
        marker->phase,
        redistributor_state.powered_down ? 0U : 1U,
        redistributor_state.processor_sleep ? 1U : 0U,
        redistributor_state.children_asleep ? 1U : 0U,
        interrupt_state.pending ? 1U : 0U,
        interrupt_state.active ? 1U : 0U,
        redistributor_state.pwrr,
        redistributor_state.waker,
        interrupt_state.pending_raw,
        interrupt_state.active_raw,
        marker->error,
        marker->error == FWK_E_TIMEOUT ? 1U : 0U);

    if (redistributor_status != FWK_SUCCESS) {
        return redistributor_status;
    }
    return interrupt_status;
}

static int set_power_and_check(
    fwk_id_t gic_id,
    unsigned int redistributor_idx,
    enum mod_gicx00_multiview_redistributor_power_state expected)
{
    int status;
    enum mod_gicx00_multiview_redistributor_power_state observed;

    status = gic_power_api->set_redistributor_power_state(
        gic_id, redistributor_idx, expected);
    if (status != FWK_SUCCESS) {
        return status;
    }

    status = gic_power_api->read_redistributor_power_state(
        gic_id, redistributor_idx, &observed);
    if (status != FWK_SUCCESS) {
        return status;
    }

    return (observed == expected) ? FWK_SUCCESS : FWK_E_STATE;
}

static int prepare_awake(fwk_id_t gic_id, unsigned int redistributor_idx)
{
    int status;

    status = gic_power_api->set_interrupt_state(
        gic_id, TEST_INTERRUPT_ID_BASE + redistributor_idx, false, false);
    if (status != FWK_SUCCESS) {
        return status;
    }

    status = set_power_and_check(
        gic_id,
        redistributor_idx,
        MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_ON);
    if (status != FWK_SUCCESS) {
        return status;
    }

    return gic_power_api->set_processor_sleep(gic_id, redistributor_idx, false);
}

static int run_pwrr(
    fwk_id_t gic_id,
    unsigned int redistributor_idx,
    unsigned int pe)
{
    int status;
    struct marker_context marker = {
        .scenario = "pwrr",
        .pe = pe,
        .interrupt_id = TEST_INTERRUPT_ID_BASE + redistributor_idx,
    };

    status =
        gic_power_api->set_processor_sleep(gic_id, redistributor_idx, true);
    if (status != FWK_SUCCESS) {
        marker.phase = "sleep-timeout";
        marker.error = status;
        (void)emit_state_record(gic_id, redistributor_idx, &marker);
        return status;
    }

    marker.phase = "sleep-observed";
    status = emit_state_record(gic_id, redistributor_idx, &marker);
    if (status != FWK_SUCCESS) {
        return status;
    }

    status = set_power_and_check(
        gic_id,
        redistributor_idx,
        MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_OFF);
    if (status != FWK_SUCCESS) {
        marker.phase = "off-timeout";
        marker.error = status;
        (void)emit_state_record(gic_id, redistributor_idx, &marker);
        return status;
    }

    marker.phase = "off-observed";
    status = emit_state_record(gic_id, redistributor_idx, &marker);
    if (status != FWK_SUCCESS) {
        return status;
    }

    status = set_power_and_check(
        gic_id,
        redistributor_idx,
        MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_ON);
    if (status != FWK_SUCCESS) {
        marker.phase = "on-timeout";
        marker.error = status;
        (void)emit_state_record(gic_id, redistributor_idx, &marker);
        return status;
    }

    status =
        gic_power_api->set_processor_sleep(gic_id, redistributor_idx, false);
    marker.phase = status == FWK_SUCCESS ? "on-observed" : "wake-timeout";
    marker.error = status;
    (void)emit_state_record(gic_id, redistributor_idx, &marker);
    return status;
}

static int run_waker(
    fwk_id_t gic_id,
    unsigned int redistributor_idx,
    unsigned int pe)
{
    int status;
    struct marker_context marker = {
        .scenario = "waker",
        .pe = pe,
        .interrupt_id = TEST_INTERRUPT_ID_BASE + redistributor_idx,
    };

    status =
        gic_power_api->set_processor_sleep(gic_id, redistributor_idx, true);
    if (status != FWK_SUCCESS) {
        marker.phase = "sleep-timeout";
        marker.error = status;
        (void)emit_state_record(gic_id, redistributor_idx, &marker);
        return status;
    }

    marker.phase = "sleep-observed";
    status = emit_state_record(gic_id, redistributor_idx, &marker);
    if (status != FWK_SUCCESS) {
        return status;
    }

    status =
        gic_power_api->set_processor_sleep(gic_id, redistributor_idx, false);
    marker.phase = status == FWK_SUCCESS ? "awake-observed" : "wake-timeout";
    marker.error = status;
    (void)emit_state_record(gic_id, redistributor_idx, &marker);
    return status;
}

static int run_pwrr_timeout_negative(
    fwk_id_t gic_id,
    unsigned int redistributor_idx)
{
    int status;

    status = gic_power_api->set_redistributor_power_state(
        gic_id,
        redistributor_idx,
        MOD_GICX00_MULTIVIEW_REDISTRIBUTOR_POWER_STATE_OFF);
    if (status == FWK_E_TIMEOUT) {
        fwk_log_printf(
            "GIC720AE_POWER_NEGATIVE scenario=pwrr-timeout-negative "
            "verdict=PASS expected=FWK_E_TIMEOUT");
        return FWK_SUCCESS;
    }

    fwk_log_printf(
        "GIC720AE_POWER_NEGATIVE scenario=pwrr-timeout-negative "
        "verdict=FAIL expected=FWK_E_TIMEOUT actual=%d",
        status);
    return status == FWK_SUCCESS ? FWK_E_STATE : status;
}

static int schedule_warm_reset_poll(unsigned int step_idx)
{
    struct fwk_event event = {
        .id = MOD_INTEGRATION_TEST_EVENT_ID_STEP_CONTINUE,
        .source_id = FWK_ID_MODULE(FWK_MODULE_IDX_TEST_GIC_POWER),
        .target_id = FWK_ID_MODULE(FWK_MODULE_IDX_INTEGRATION_TEST),
        .params = { POWER_TEST_CASE_COUNT - 1U, step_idx },
    };

    return fwk_put_event(&event);
}

static int arm_warm_reset_epoch(fwk_id_t gic_id)
{
    int status;
    unsigned int redistributor_idx;
    struct marker_context marker = {
        .scenario = "pending-warm-reset",
        .phase = "pre-reset",
    };

    for (redistributor_idx = 0U; redistributor_idx < TESTED_PE_COUNT;
         redistributor_idx++) {
        marker.pe = redistributor_idx + 1U;
        marker.interrupt_id = TEST_INTERRUPT_ID_BASE + redistributor_idx;
        emit_start_record(marker.scenario, marker.pe);
        status = gic_power_api->set_interrupt_state(
            gic_id, marker.interrupt_id, true, true);
        if (status != FWK_SUCCESS) {
            marker.phase = "arm-timeout";
            marker.error = status;
            (void)emit_state_record(gic_id, redistributor_idx, &marker);
            return status;
        }
        status = emit_state_record(gic_id, redistributor_idx, &marker);
        if (status != FWK_SUCCESS) {
            return status;
        }
        emit_ready_record(marker.scenario, marker.pe);
    }

    warm_reset_poll_count = 0U;
    status = pd_api->system_shutdown(MOD_PD_SYSTEM_WARM_RESET);
    if (status != FWK_PENDING) {
        return status == FWK_SUCCESS ? FWK_E_STATE : status;
    }

    status = schedule_warm_reset_poll(1U);
    return status == FWK_SUCCESS ? FWK_PENDING : status;
}

static int observe_warm_reset_epoch(fwk_id_t gic_id)
{
    int status;
    bool stale = false;
    unsigned int redistributor_idx;
    struct mod_gicx00_multiview_interrupt_state interrupt_state;
    struct marker_context marker = {
        .scenario = "pending-warm-reset",
        .phase = "post-reset",
    };

    for (redistributor_idx = 0U; redistributor_idx < TESTED_PE_COUNT;
         redistributor_idx++) {
        status = gic_power_api->read_interrupt_state(
            gic_id,
            TEST_INTERRUPT_ID_BASE + redistributor_idx,
            &interrupt_state);
        if (status != FWK_SUCCESS) {
            return status;
        }
        stale |= interrupt_state.pending || interrupt_state.active;
    }

    if (stale && (++warm_reset_poll_count < WARM_RESET_POLL_LIMIT)) {
        status = schedule_warm_reset_poll(1U);
        return status == FWK_SUCCESS ? FWK_PENDING : status;
    }

    for (redistributor_idx = 0U; redistributor_idx < TESTED_PE_COUNT;
         redistributor_idx++) {
        marker.pe = redistributor_idx + 1U;
        marker.interrupt_id = TEST_INTERRUPT_ID_BASE + redistributor_idx;
        marker.phase = stale ? "stale-timeout" : "post-reset";
        marker.error = stale ? FWK_E_TIMEOUT : FWK_SUCCESS;
        status = emit_state_record(gic_id, redistributor_idx, &marker);
        if (status != FWK_SUCCESS) {
            return status;
        }
        emit_done_record(marker.scenario, marker.pe, !stale);
    }

    return stale ? FWK_E_TIMEOUT : FWK_SUCCESS;
}

static int run(
    unsigned int case_idx,
    unsigned int step_idx,
    const struct fwk_event *event)
{
    int status;
    unsigned int redistributor_idx;
    enum test_gic_power_scenario scenario;
    fwk_id_t gic_id =
        FWK_ID_ELEMENT(FWK_MODULE_IDX_GICX00_MULTIVIEW, GIC_AP_ELEMENT_IDX);

    if (case_idx >= POWER_TEST_CASE_COUNT) {
        return FWK_E_PARAM;
    }

    if (case_idx < (2U * TESTED_PE_COUNT)) {
        scenario = case_idx / TESTED_PE_COUNT;
    } else if (case_idx == (2U * TESTED_PE_COUNT)) {
        scenario = TEST_GIC_POWER_SCENARIO_PWRR_TIMEOUT_NEGATIVE;
    } else {
        scenario = TEST_GIC_POWER_SCENARIO_PENDING_WARM_RESET;
    }
    redistributor_idx = case_idx % TESTED_PE_COUNT;
    if (scenario == TEST_GIC_POWER_SCENARIO_PENDING_WARM_RESET) {
        if (step_idx > 1U) {
            return FWK_E_PARAM;
        }
        status = step_idx == 0U ? arm_warm_reset_epoch(gic_id) :
                                  observe_warm_reset_epoch(gic_id);
        if ((status != FWK_SUCCESS) && (status != FWK_PENDING)) {
            TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
        }
        return status;
    }

    if (step_idx != 0U) {
        return FWK_E_PARAM;
    }

    emit_start_record(scenario_name(scenario), redistributor_idx + 1U);
    status = prepare_awake(gic_id, redistributor_idx);
    if (status != FWK_SUCCESS) {
        emit_done_record(
            scenario_name(scenario), redistributor_idx + 1U, false);
        TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
        return status;
    }
    emit_ready_record(scenario_name(scenario), redistributor_idx + 1U);

    switch (scenario) {
    case TEST_GIC_POWER_SCENARIO_PWRR:
        status = run_pwrr(gic_id, redistributor_idx, redistributor_idx + 1U);
        break;
    case TEST_GIC_POWER_SCENARIO_WAKER:
        status = run_waker(gic_id, redistributor_idx, redistributor_idx + 1U);
        break;
    case TEST_GIC_POWER_SCENARIO_PWRR_TIMEOUT_NEGATIVE:
        status = run_pwrr_timeout_negative(gic_id, redistributor_idx);
        break;
    default:
        return FWK_E_PARAM;
    }

    emit_done_record(
        scenario_name(scenario), redistributor_idx + 1U, status == FWK_SUCCESS);
    if (status != FWK_SUCCESS) {
        TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
        return status;
    }

    return FWK_SUCCESS;
}

static const char *test_name(unsigned int case_idx)
{
    if (case_idx >= POWER_TEST_CASE_COUNT) {
        return NULL;
    }

    if (case_idx < (2U * TESTED_PE_COUNT)) {
        return scenario_name(case_idx / TESTED_PE_COUNT);
    }

    return scenario_name(
        case_idx == (2U * TESTED_PE_COUNT) ?
            TEST_GIC_POWER_SCENARIO_PWRR_TIMEOUT_NEGATIVE :
            TEST_GIC_POWER_SCENARIO_PENDING_WARM_RESET);
}

static const struct mod_integration_test_api test_api = {
    .run = run,
    .test_name = test_name,
};

static int test_gic_power_init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *data)
{
    return FWK_SUCCESS;
}

static int test_gic_power_bind(fwk_id_t id, unsigned int round)
{
    int status;

    if (round > 0U) {
        return FWK_SUCCESS;
    }

    status = fwk_module_bind(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_GICX00_MULTIVIEW, GIC_AP_ELEMENT_IDX),
        FWK_ID_API(
            FWK_MODULE_IDX_GICX00_MULTIVIEW,
            MOD_GICX00_MULTIVIEW_API_IDX_POWER),
        (const void **)&gic_power_api);
    if (status != FWK_SUCCESS) {
        return status;
    }

    return fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_POWER_DOMAIN),
        FWK_ID_API(FWK_MODULE_IDX_POWER_DOMAIN, MOD_PD_API_IDX_RESTRICTED),
        (const void **)&pd_api);
}

static int test_gic_power_process_bind_request(
    fwk_id_t requester_id,
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void **api)
{
    if ((api == NULL) ||
        (fwk_id_get_api_idx(api_id) != MOD_INTEGRATION_TEST_API_IDX_TEST)) {
        return FWK_E_PARAM;
    }

    *api = &test_api;
    return FWK_SUCCESS;
}

struct fwk_module_config config_test_gic_power = { 0 };

const struct fwk_module module_test_gic_power = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .init = test_gic_power_init,
    .bind = test_gic_power_bind,
    .process_bind_request = test_gic_power_process_bind_request,
    .api_count = MOD_INTEGRATION_TEST_API_COUNT,
};
