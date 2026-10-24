/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "internal/fmu_common.h"
#include "scp_unity.h"
#include "unity.h"

#include <Mockfwk_interrupt.h>
#include <Mockfwk_core.h>
#include <internal/Mockfwk_core_internal.h>
#include <Mockfwk_module.h>
#include <Mockfwk_notification.h>

#include <fwk_macros.h>
#include <fwk_mm.h>
#include <fwk_string.h>

#include UNIT_TEST_SRC
#include <config_fmu.h>

#include <mod_fmu.h>

/* Framework tests exercise real all-or-nothing enqueue. This adapter lets
 * dispatcher tests inject its result using the existing notification mock. */
int fwk_notification_notify_atomic(struct fwk_event *event, unsigned int *count)
{
    return fwk_notification_notify(event, count);
}

static unsigned int remaining;
static unsigned int ack_calls;
static int ack_result;
static bool last_critical;
static bool remaining_critical;
static bool simulate_refresh;
static int refresh_result;
static unsigned int refresh_calls;
static bool interrupt_context;
static bool enqueue_from_isr;
static unsigned int enqueue_calls;
static int enqueue_result;
static bool fault_on_enable;

static bool system_fault_peek(
    const struct mod_fmu_dev_config *config,
    bool critical,
    unsigned int *node_idx)
{
    last_critical = critical;
    *node_idx = 33;
    return remaining != 0 && critical == remaining_critical;
}

static int system_fault_ack(
    const struct mod_fmu_dev_config *config,
    struct mod_fmu_fault *fault,
    unsigned int node_idx,
    bool *fault_tracked,
    struct mod_fmu_ack_state *state)
{
    if (!state->acknowledged) {
        ack_calls++;
        if (ack_result != FWK_SUCCESS) {
            return ack_result;
        }
        remaining--;
        fault->node_idx = 33;
        fault->sm_idx = 17;
        fault->block_id = 1;
        *fault_tracked = true;
        state->acknowledged = true;
        state->refresh_pending = simulate_refresh;
    }
    if (state->refresh_pending) {
        refresh_calls++;
        if (refresh_result != FWK_SUCCESS) {
            return refresh_result;
        }
        state->refresh_pending = false;
    }
    return FWK_SUCCESS;
}

static int system_inject(
    const struct mod_fmu_dev_config *config,
    const struct mod_fmu_fault *fault)
{
    return FWK_SUCCESS;
}

static uint8_t s_threshold;

static int system_set_threshold(
    const struct mod_fmu_dev_config *config,
    uint16_t node_id,
    uint8_t threshold)
{
    s_threshold = threshold;

    return FWK_SUCCESS;
}

static int system_get_threshold(
    const struct mod_fmu_dev_config *config,
    uint16_t node_id,
    uint8_t *threshold)
{
    *threshold = s_threshold;

    return FWK_SUCCESS;
}

struct mod_fmu_impl_api mod_fmu_system_api = {
    .fault_peek = system_fault_peek,
    .fault_ack = system_fault_ack,
    .inject = system_inject,
    .set_threshold = system_set_threshold,
    .get_threshold = system_get_threshold,
};

struct mod_fmu_impl_api mod_fmu_gic_mhu_api = { 0 };
struct mod_fmu_impl_api mod_ni710ae_fmu_api = { 0 };

void setUp(void)
{
    fwk_interrupt_enable_StubWithCallback(NULL);
    __fwk_put_event_StubWithCallback(NULL);
    fwk_notification_notify_StubWithCallback(NULL);
    remaining = 0;
    remaining_critical = false;
    ack_calls = 0;
    ack_result = refresh_result = FWK_SUCCESS;
    simulate_refresh = false;
    refresh_calls = 0;
    interrupt_context = false;
    enqueue_from_isr = true;
    enqueue_calls = 0;
    enqueue_result = FWK_SUCCESS;
    fault_on_enable = false;
    fwk_str_memset(ctx.drain, 0, sizeof(ctx.drain));
}

void tearDown(void)
{
    /* Do nothing */
}

void test_fmu_init(void)
{
    int status;

    status = fmu_init(fwk_module_id_fmu, SCP_FMU_COUNT, config_fmu.data);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    TEST_ASSERT_EQUAL(SCP_FMU_COUNT, ctx.num_devices);
}

void test_fmu_device_init(void)
{
    int status;
    unsigned int idx;
    fwk_id_t element_id;

    for (idx = 0; idx < SCP_FMU_COUNT; idx++) {
        element_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_FMU, idx);
        status = fmu_device_init(element_id, 0, fmu_devices[idx].data);
        TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

        TEST_ASSERT_EQUAL(fmu_devices[idx].data, ctx.device_config[idx]);
    }
}

void test_fmu_start(void)
{
    int status;

    fwk_module_get_data_ExpectAndReturn(fwk_module_id_fmu, config_fmu.data);

    fwk_interrupt_set_isr_ExpectAndReturn(12, fmu_isr_critical, FWK_SUCCESS);
    fwk_interrupt_set_isr_ExpectAndReturn(
        13, fmu_isr_non_critical, FWK_SUCCESS);
    fwk_interrupt_enable_ExpectAndReturn(12, FWK_SUCCESS);
    fwk_interrupt_enable_ExpectAndReturn(13, FWK_SUCCESS);

    status = fmu_start(fwk_module_id_fmu);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

void test_fmu_process_bind_request(void)
{
    int status;
    uintptr_t api;

    status = fmu_process_bind_request(
        fwk_module_id_fake,
        fwk_module_id_fmu,
        FWK_ID_API(FWK_MODULE_IDX_FMU, MOD_FMU_DEVICE_API_IDX),
        (void *)&api);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(&fmu_api, api);
}

void test_fmu_isr_only_masks_and_queues(void)
{
    remaining = 1;
    fwk_interrupt_disable_ExpectAndReturn(12, FWK_SUCCESS);
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    fmu_isr(true);
    TEST_ASSERT_EQUAL_UINT(0, ack_calls);
    TEST_ASSERT_TRUE(ctx.drain[true].queued);
    TEST_ASSERT_TRUE(ctx.drain[true].status.masked);
}

static struct fwk_event drain_event(bool critical)
{
    struct fwk_event event = { .params = { critical } };
    return event;
}

void test_fmu_event_preserves_fault_identity(void)
{
    struct fwk_event event = { 0 };
    struct fwk_event input = drain_event(true);
    struct mod_fmu_fault_notification_params *params;
    remaining = 1;
    remaining_critical = true;
    event.id = mod_fmu_notification_id_fault;
    event.source_id = fwk_module_id_fmu;
    params = (struct mod_fmu_fault_notification_params *)event.params;
    params->critical = true;
    params->fault.node_idx = 33;
    params->fault.sm_idx = 17;
    params->fault.block_id = 1;
    fwk_notification_notify_ExpectAndReturn(&event, NULL, FWK_SUCCESS);
    fwk_notification_notify_IgnoreArg_count();
    fwk_interrupt_enable_ExpectAndReturn(12, FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(1, ack_calls);
    TEST_ASSERT_TRUE(last_critical);
    TEST_ASSERT_FALSE(ctx.drain[true].status.masked);
}

void test_fmu_ack_failure_keeps_irq_masked_and_can_recover(void)
{
    struct fwk_event input = drain_event(false);
    struct mod_fmu_drain_status state;
    remaining = 1;
    ack_result = FWK_E_TIMEOUT;
    ctx.drain[false].status.masked = true;
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(1, ack_calls);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, get_drain_status(false, &state));
    TEST_ASSERT_TRUE(state.masked);
    TEST_ASSERT_EQUAL(FWK_E_TIMEOUT, state.last_error);
    TEST_ASSERT_EQUAL_UINT(0, state.drained);
    TEST_ASSERT_EQUAL_UINT(1, remaining);
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, recover(false));
}

void test_fmu_queue_failure_is_observable(void)
{
    fwk_interrupt_disable_ExpectAndReturn(12, FWK_SUCCESS);
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_E_NOMEM);
    fmu_isr(true);
    TEST_ASSERT_TRUE(ctx.drain[true].status.masked);
    TEST_ASSERT_FALSE(ctx.drain[true].queued);
    TEST_ASSERT_EQUAL(FWK_E_NOMEM, ctx.drain[true].status.last_error);
}

static int check_drain_enqueue(struct fwk_event *event, int call_count)
{
    /* The native framework routes fwk_put_event by the caller's context. */
    TEST_ASSERT_EQUAL(enqueue_from_isr, interrupt_context);
    TEST_ASSERT_TRUE(fwk_id_is_equal(
        FWK_ID_EVENT(FWK_MODULE_IDX_FMU, FMU_EVENT_DRAIN), event->id));
    enqueue_calls++;
    return enqueue_result;
}

static int enable_with_pending_level(unsigned int irq, int call_count)
{
    bool critical = irq == ctx.config->irq_critical;
    TEST_ASSERT_FALSE(ctx.drain[critical].status.masked);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ctx.drain[critical].status.last_error);
    if (fault_on_enable) {
        remaining++;
        fault_on_enable = false;
    }
    if (remaining != 0) {
        interrupt_context = true;
        fmu_isr(critical);
        interrupt_context = false;
    }
    return FWK_SUCCESS;
}

void test_fmu_event_budget_rearms_for_isr_continuation(void)
{
    struct fwk_event input = drain_event(false);
    remaining = FMU_DRAIN_BUDGET + 1;
    ctx.drain[false].status.masked = true;
    fwk_notification_notify_IgnoreAndReturn(FWK_SUCCESS);
    fwk_interrupt_enable_StubWithCallback(enable_with_pending_level);
    fwk_interrupt_disable_ExpectAndReturn(13, FWK_SUCCESS);
    __fwk_put_event_StubWithCallback(check_drain_enqueue);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(FMU_DRAIN_BUDGET, ack_calls);
    TEST_ASSERT_EQUAL_UINT(1, remaining);
    TEST_ASSERT_EQUAL_UINT(1, enqueue_calls);
    TEST_ASSERT_TRUE(ctx.drain[false].status.masked);
    TEST_ASSERT_TRUE(ctx.drain[false].queued);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(FMU_DRAIN_BUDGET + 1, ack_calls);
    TEST_ASSERT_EQUAL_UINT(1, enqueue_calls);
    TEST_ASSERT_EQUAL_UINT(0, remaining);
    TEST_ASSERT_FALSE(ctx.drain[false].status.masked);
    TEST_ASSERT_FALSE(ctx.drain[false].queued);
}

void test_fmu_final_peek_rearm_preserves_new_fault_isr_state(void)
{
    struct fwk_event input = drain_event(true);
    ctx.drain[true].status.masked = true;
    fault_on_enable = true;
    remaining_critical = true;
    fwk_interrupt_enable_StubWithCallback(enable_with_pending_level);
    fwk_interrupt_disable_ExpectAndReturn(12, FWK_SUCCESS);
    __fwk_put_event_StubWithCallback(check_drain_enqueue);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(0, ack_calls);
    TEST_ASSERT_EQUAL_UINT(1, remaining);
    TEST_ASSERT_EQUAL_UINT(1, enqueue_calls);
    TEST_ASSERT_TRUE(ctx.drain[true].status.masked);
    TEST_ASSERT_TRUE(ctx.drain[true].queued);
    fwk_notification_notify_IgnoreAndReturn(FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(1, ack_calls);
    TEST_ASSERT_EQUAL_UINT(0, remaining);
    TEST_ASSERT_FALSE(ctx.drain[true].status.masked);
}

void test_fmu_budget_rearm_queue_failure_retains_mask_for_recovery(void)
{
    struct fwk_event input = drain_event(false);
    remaining = FMU_DRAIN_BUDGET + 1;
    ctx.drain[false].status.masked = true;
    enqueue_result = FWK_E_NOMEM;
    fwk_notification_notify_IgnoreAndReturn(FWK_SUCCESS);
    fwk_interrupt_enable_StubWithCallback(enable_with_pending_level);
    fwk_interrupt_disable_ExpectAndReturn(13, FWK_SUCCESS);
    __fwk_put_event_StubWithCallback(check_drain_enqueue);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(FMU_DRAIN_BUDGET, ack_calls);
    TEST_ASSERT_EQUAL_UINT(1, remaining);
    TEST_ASSERT_TRUE(ctx.drain[false].status.masked);
    TEST_ASSERT_FALSE(ctx.drain[false].queued);
    TEST_ASSERT_EQUAL(FWK_E_NOMEM, ctx.drain[false].status.last_error);

    enqueue_from_isr = false;
    enqueue_result = FWK_SUCCESS;
    TEST_ASSERT_EQUAL(FWK_SUCCESS, recover(false));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(FMU_DRAIN_BUDGET + 1, ack_calls);
    TEST_ASSERT_EQUAL_UINT(0, remaining);
    TEST_ASSERT_FALSE(ctx.drain[false].status.masked);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ctx.drain[false].status.last_error);
}

void test_fmu_rearm_failure_keeps_irq_masked(void)
{
    struct fwk_event input = drain_event(true);
    ctx.drain[true].status.masked = true;
    fwk_interrupt_enable_ExpectAndReturn(12, FWK_E_DEVICE);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_TRUE(ctx.drain[true].status.masked);
    TEST_ASSERT_EQUAL(FWK_E_DEVICE, ctx.drain[true].status.last_error);
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, recover(true));
}

void test_fmu_notification_failure_is_observable(void)
{
    struct fwk_event input = drain_event(false);
    remaining = 1;
    ctx.drain[false].status.masked = true;
    fwk_notification_notify_ExpectAnyArgsAndReturn(FWK_E_NOMEM);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL(FWK_E_NOMEM, ctx.drain[false].status.last_error);
    TEST_ASSERT_TRUE(ctx.drain[false].status.masked);
}

void test_fmu_notification_retry_does_not_reack_or_duplicate(void)
{
    struct fwk_event input = drain_event(false);
    struct fwk_event expected = {
        .id = mod_fmu_notification_id_fault,
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_FMU),
    };
    struct mod_fmu_fault_notification_params *params = (void *)expected.params;
    params->fault.node_idx = 33;
    params->fault.sm_idx = 17;
    params->fault.block_id = 1;
    remaining = 1;
    ctx.drain[false].status.masked = true;
    fwk_notification_notify_ExpectAndReturn(&expected, NULL, FWK_E_NOMEM);
    fwk_notification_notify_IgnoreArg_count();
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(1, ack_calls);
    TEST_ASSERT_EQUAL_UINT(0, remaining);
    TEST_ASSERT_TRUE(ctx.drain[false].notification_pending);
    TEST_ASSERT_EQUAL_UINT(1, ctx.drain[false].pending_fault.block_id);
    TEST_ASSERT_EQUAL_UINT(0, ctx.drain[false].status.drained);

    /* Recover itself can fail to queue; the ACKed identity must survive. */
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_E_NOMEM);
    TEST_ASSERT_EQUAL(FWK_E_NOMEM, recover(false));
    TEST_ASSERT_TRUE(ctx.drain[false].notification_pending);
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, recover(false));
    fwk_notification_notify_ExpectAndReturn(&expected, NULL, FWK_E_NOMEM);
    fwk_notification_notify_IgnoreArg_count();
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(1, ack_calls);
    TEST_ASSERT_TRUE(ctx.drain[false].notification_pending);

    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, recover(false));
    fwk_notification_notify_ExpectAndReturn(&expected, NULL, FWK_SUCCESS);
    fwk_notification_notify_IgnoreArg_count();
    fwk_interrupt_enable_ExpectAndReturn(13, FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(1, ack_calls);
    TEST_ASSERT_EQUAL_UINT(1, ctx.drain[false].status.drained);
    TEST_ASSERT_FALSE(ctx.drain[false].notification_pending);
    TEST_ASSERT_FALSE(ctx.drain[false].status.masked);
    /* An empty follow-up event does not emit the delivered notification again. */
    fwk_interrupt_enable_ExpectAndReturn(13, FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(1, ack_calls);
}

void test_fmu_resume_partial_ack_before_notification(void)
{
    struct fwk_event input = drain_event(true);
    struct fwk_event expected = {
        .id = mod_fmu_notification_id_fault,
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_FMU),
    };
    struct mod_fmu_fault_notification_params *params = (void *)expected.params;
    params->critical = true;
    params->fault.node_idx = 33;
    params->fault.sm_idx = 17;
    params->fault.block_id = 1;
    remaining = 1;
    remaining_critical = true;
    simulate_refresh = true;
    refresh_result = FWK_E_TIMEOUT;
    ctx.drain[true].status.masked = true;
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(1, ack_calls);
    TEST_ASSERT_EQUAL_UINT(1, refresh_calls);
    TEST_ASSERT_EQUAL_UINT(0, remaining);
    TEST_ASSERT_EQUAL_UINT(1, ctx.drain[true].depth);
    TEST_ASSERT_TRUE(ctx.drain[true].path[0].ack.acknowledged);
    TEST_ASSERT_EQUAL_UINT(1, ctx.drain[true].pending_fault.block_id);
    TEST_ASSERT_FALSE(ctx.drain[true].notification_pending);

    refresh_result = FWK_SUCCESS;
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, recover(true));
    fwk_notification_notify_ExpectAndReturn(&expected, NULL, FWK_SUCCESS);
    fwk_notification_notify_IgnoreArg_count();
    fwk_interrupt_enable_ExpectAndReturn(12, FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(1, ack_calls);
    TEST_ASSERT_EQUAL_UINT(2, refresh_calls);
    TEST_ASSERT_EQUAL_UINT(0, ctx.drain[true].depth);
    TEST_ASSERT_EQUAL_UINT(1, ctx.drain[true].status.drained);
    TEST_ASSERT_FALSE(ctx.drain[true].status.masked);
}

void test_fmu_set_count_unsupported(void)
{
    int status;
    fwk_id_t id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_FMU, SCP_FMU_ROOT);

    status = fmu_api.set_count(id, 12, 13);
    TEST_ASSERT_EQUAL(FWK_E_SUPPORT, status);
}

void test_fmu_set_threshold(void)
{
    int status;
    uint8_t count;
    fwk_id_t id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_FMU, SCP_FMU_ROOT);

    status = fmu_api.set_threshold(id, 34, 25);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);

    status = fmu_api.get_threshold(id, 34, &count);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL(25, count);
}

static bool route_root[2], route_parent[4], route_leaf;
static unsigned int route_leaf_acks, route_parent_acks;
static unsigned int route_root_acks[2];
static bool route_fail_fhi, route_relatch_root;
static const struct mod_fmu_dev_config route_configs[] = {
    { .base = 0, .parent = MOD_FMU_PARENT_NONE },
    { .base = 1, .parent = 0, .parent_cr_index = 0, .parent_ncr_index = 1 },
    { .base = 2, .parent = 1, .has_parent_sources = true,
      .parent_source = { 1, 2, 3 } },
};
static bool route_peek(const struct mod_fmu_dev_config *cfg,
                       bool critical, unsigned int *node)
{
    if (cfg->base == 0) {
        *node = critical ? 0 : 1;
        return route_root[*node];
    }
    *node = critical ? 1 : 2;
    return route_parent[*node];
}
static bool route_source_peek(const struct mod_fmu_dev_config *cfg,
                              unsigned int parent_node, unsigned int *node)
{
    TEST_ASSERT_TRUE(parent_node == 1 || parent_node == 2);
    *node = 11;
    return route_leaf;
}
static int route_ack(const struct mod_fmu_dev_config *cfg,
                     struct mod_fmu_fault *fault, unsigned int node,
                     bool *tracked, struct mod_fmu_ack_state *progress)
{
    if (progress->acknowledged)
        return FWK_SUCCESS;
    if (cfg->base == 2) {
        TEST_ASSERT_TRUE(route_leaf);
        route_leaf = false;
        route_leaf_acks++;
        fault->node_idx = 11;
        fault->sm_idx = 13;
        *tracked = true;
        progress->parent_clear_mask = 3;
    } else if (cfg->base == 1) {
        if (node == 2 && route_fail_fhi)
            return FWK_E_DEVICE;
        route_parent_acks += route_parent[node];
        route_parent[node] = false;
    } else {
        route_root_acks[node]++;
        route_root[node] = false;
        if (!*tracked) {
            fault->node_idx = node;
            fault->sm_idx = 16;
            *tracked = true;
        }
        if (node == 0 && route_relatch_root) {
            route_relatch_root = false;
            route_root[node] = true;
        }
    }
    progress->acknowledged = true;
    return FWK_SUCCESS;
}
static const struct mod_fmu_impl_api route_system_api = {
    .fault_peek = route_peek, .fault_ack = route_ack,
};
static const struct mod_fmu_impl_api route_ni_api = {
    .fault_peek_source = route_source_peek, .fault_ack = route_ack,
};
static const struct mod_fmu_dev_config *route_devices[] = {
    &route_configs[0], &route_configs[1], &route_configs[2],
};
static const struct mod_fmu_impl_api *route_apis[] = {
    &route_system_api, &route_system_api, &route_ni_api,
};
static void run_route_cleanup(bool fail_first, bool nc_first)
{
    const struct mod_fmu_dev_config **saved_devices = ctx.device_config;
    const struct mod_fmu_impl_api **saved_apis = ctx.impl_apis;
    struct fwk_event input = drain_event(true);
    ctx.device_config = route_devices;
    ctx.impl_apis = route_apis;
    route_root[0] = route_root[1] = true;
    route_parent[1] = route_parent[2] = true;
    route_leaf = true;
    route_leaf_acks = route_parent_acks = 0;
    route_root_acks[0] = route_root_acks[1] = 0;
    route_fail_fhi = fail_first;
    route_relatch_root = false;
    ctx.drain[true].status.masked = true;
    for (unsigned int source = 1; source <= 3; source++)
        TEST_ASSERT_EQUAL_UINT(2, find_next_fmu(1, source));
    if (nc_first) {
        struct fwk_event old_nc = drain_event(false);
        ctx.drain[false].status.masked = true;
        TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&old_nc, NULL));
        TEST_ASSERT_TRUE(ctx.drain[false].waiting_critical);
        TEST_ASSERT_TRUE(ctx.drain[false].status.masked);
        TEST_ASSERT_EQUAL_UINT(0, route_leaf_acks);
    }
    if (fail_first) {
        TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
        TEST_ASSERT_TRUE(ctx.drain[true].status.masked);
        TEST_ASSERT_EQUAL(FWK_E_DEVICE, ctx.drain[true].status.last_error);
        TEST_ASSERT_EQUAL_UINT(1, route_leaf_acks);
        TEST_ASSERT_EQUAL_UINT(1, route_parent_acks);
        if (nc_first)
            TEST_ASSERT_TRUE(ctx.drain[false].waiting_critical);
        route_fail_fhi = false;
        __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
        TEST_ASSERT_EQUAL(FWK_SUCCESS, recover(true));
    }
    struct fwk_event notification = {
        .id = mod_fmu_notification_id_fault,
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_FMU),
    };
    struct mod_fmu_fault_notification_params *p = (void *)notification.params;
    p->critical = true;
    p->fault.device_idx = 2;
    p->fault.node_idx = 11;
    p->fault.sm_idx = 13;
    fwk_notification_notify_ExpectAndReturn(&notification, NULL, FWK_SUCCESS);
    fwk_notification_notify_IgnoreArg_count();
    fwk_interrupt_enable_ExpectAndReturn(12, FWK_SUCCESS);
    if (nc_first)
        fwk_interrupt_enable_ExpectAndReturn(13, FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(1, route_leaf_acks);
    TEST_ASSERT_EQUAL_UINT(2, route_parent_acks);
    TEST_ASSERT_FALSE(route_root[0]);
    TEST_ASSERT_FALSE(route_root[1]);
    TEST_ASSERT_FALSE(ctx.drain[false].waiting_critical);
    /* A previously queued noncritical IRQ has no stale parent to report. */
    input = drain_event(false);
    fwk_interrupt_enable_ExpectAndReturn(13, FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&input, NULL));
    TEST_ASSERT_EQUAL_UINT(0, ctx.drain[false].status.drained);
    ctx.device_config = saved_devices;
    ctx.impl_apis = saved_apis;
}
void test_ni_snapshot_clears_sibling_and_root_routes_once(void)
{
    run_route_cleanup(false, false);
}
void test_ni_upstream_ack_failure_resumes_without_duplicate_snapshot(void)
{
    run_route_cleanup(true, false);
}

void test_noncritical_park_releases_after_critical_drain_and_ignores_stale_event(void)
{
    run_route_cleanup(false, true);
}
void test_noncritical_park_survives_critical_ack_failure_until_recovery(void)
{
    run_route_cleanup(true, true);
}

void test_ni_ancestor_new_fault_is_not_acknowledged_by_saved_path(void)
{
    const struct mod_fmu_dev_config **saved_devices = ctx.device_config;
    const struct mod_fmu_impl_api **saved_apis = ctx.impl_apis;
    ctx.device_config = route_devices;
    ctx.impl_apis = route_apis;
    route_root[0] = route_root[1] = true;
    route_parent[1] = route_parent[2] = true;
    route_leaf = true;
    route_leaf_acks = route_parent_acks = 0;
    route_root_acks[0] = route_root_acks[1] = 0;
    route_fail_fhi = false;
    route_relatch_root = true;

    struct fwk_event notification = {
        .id = mod_fmu_notification_id_fault,
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_FMU),
    };
    struct mod_fmu_fault_notification_params *p = (void *)notification.params;
    p->critical = true;
    p->fault.device_idx = 2;
    p->fault.node_idx = 11;
    p->fault.sm_idx = 13;
    fwk_notification_notify_ExpectAndReturn(&notification, NULL, FWK_SUCCESS);
    fwk_notification_notify_IgnoreArg_count();
    TEST_ASSERT_EQUAL(FWK_SUCCESS, next_fault(true));
    TEST_ASSERT_EQUAL_UINT(1, route_root_acks[0]);
    TEST_ASSERT_TRUE(route_root[0]);
    TEST_ASSERT_EQUAL_UINT(0, ctx.drain[true].depth);

    /* The newly latched root pulse must get its own notification. */
    p->fault.device_idx = 0;
    p->fault.node_idx = 0;
    p->fault.sm_idx = 16;
    fwk_notification_notify_ExpectAndReturn(&notification, NULL, FWK_SUCCESS);
    fwk_notification_notify_IgnoreArg_count();
    TEST_ASSERT_EQUAL(FWK_SUCCESS, next_fault(true));
    TEST_ASSERT_EQUAL_UINT(2, route_root_acks[0]);
    TEST_ASSERT_FALSE(route_root[0]);
    TEST_ASSERT_EQUAL_UINT(1, route_leaf_acks);
    ctx.device_config = saved_devices;
    ctx.impl_apis = saved_apis;
}

static unsigned int route_notifications;
static bool route_notification_critical;
static int capture_route_notification(struct fwk_event *event,
                                      unsigned int *count, int call_count)
{
    const struct mod_fmu_fault_notification_params *p = (void *)event->params;
    route_notifications++;
    route_notification_critical = p->critical;
    return FWK_SUCCESS;
}

void test_queued_noncritical_drain_defers_shared_leaf_to_pending_critical(void)
{
    const struct mod_fmu_dev_config **saved_devices = ctx.device_config;
    const struct mod_fmu_impl_api **saved_apis = ctx.impl_apis;
    ctx.device_config = route_devices;
    ctx.impl_apis = route_apis;
    route_root[0] = route_root[1] = true;
    route_parent[1] = route_parent[2] = true;
    route_leaf = true;
    route_leaf_acks = route_parent_acks = route_notifications = 0;
    route_root_acks[0] = route_root_acks[1] = 0;
    route_fail_fhi = route_relatch_root = false;
    ctx.drain[false].status.masked = true;
    fwk_notification_notify_StubWithCallback(capture_route_notification);

    /* An older NC event runs before the fresh C IRQ's event. Both parent
     * routes refer to the same NI UE snapshot; do not report it as NC. */
    TEST_ASSERT_EQUAL(FWK_PENDING, next_fault(false));
    TEST_ASSERT_EQUAL_UINT(0, route_leaf_acks);
    TEST_ASSERT_EQUAL_UINT(0, route_notifications);
    TEST_ASSERT_TRUE(route_root[0]);
    TEST_ASSERT_TRUE(route_root[1]);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, next_fault(true));
    TEST_ASSERT_EQUAL_UINT(1, route_leaf_acks);
    TEST_ASSERT_EQUAL_UINT(1, route_notifications);
    TEST_ASSERT_TRUE(route_notification_critical);
    TEST_ASSERT_FALSE(route_root[0]);
    TEST_ASSERT_FALSE(route_root[1]);
    TEST_ASSERT_EQUAL(FWK_PENDING, next_fault(false));
    TEST_ASSERT_EQUAL_UINT(1, route_notifications);
    ctx.device_config = saved_devices;
    ctx.impl_apis = saved_apis;
}

void test_noncritical_park_releases_after_failed_critical_queue_and_removed_source(void)
{
    const struct mod_fmu_dev_config **saved_devices = ctx.device_config;
    const struct mod_fmu_impl_api **saved_apis = ctx.impl_apis;
    ctx.device_config = route_devices;
    ctx.impl_apis = route_apis;
    route_root[0] = route_root[1] = true;
    route_parent[1] = route_parent[2] = true;
    route_leaf = true;
    route_leaf_acks = route_parent_acks = route_notifications = 0;
    route_root_acks[0] = route_root_acks[1] = 0;
    route_fail_fhi = route_relatch_root = false;
    fwk_notification_notify_StubWithCallback(capture_route_notification);
    fwk_interrupt_disable_ExpectAndReturn(12, FWK_SUCCESS);
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_E_NOMEM);
    fmu_isr(true);
    struct fwk_event nc_event = drain_event(false);
    ctx.drain[false].status.masked = true;
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&nc_event, NULL));
    TEST_ASSERT_TRUE(ctx.drain[false].waiting_critical);
    TEST_ASSERT_EQUAL_UINT(0, route_notifications);
    /* The critical route is removed before retry, but FHI remains pending. */
    route_root[0] = route_parent[1] = false;
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, recover(true));
    struct fwk_event c_event = drain_event(true);
    fwk_interrupt_enable_ExpectAndReturn(12, FWK_SUCCESS);
    fwk_interrupt_enable_ExpectAndReturn(13, FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&c_event, NULL));
    TEST_ASSERT_FALSE(ctx.drain[false].waiting_critical);
    TEST_ASSERT_FALSE(ctx.drain[false].status.masked);
    fwk_interrupt_enable_ExpectAndReturn(13, FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&nc_event, NULL));
    TEST_ASSERT_EQUAL_UINT(1, route_leaf_acks);
    TEST_ASSERT_EQUAL_UINT(1, route_notifications);
    TEST_ASSERT_FALSE(route_notification_critical);
    ctx.device_config = saved_devices;
    ctx.impl_apis = saved_apis;
}

void test_acknowledged_noncritical_notification_retry_precedes_priority_park(void)
{
    struct fwk_event nc_event = drain_event(false);
    struct fwk_event c_event = drain_event(true);
    struct fwk_event notification = {
        .id = mod_fmu_notification_id_fault,
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_FMU),
    };
    struct mod_fmu_fault_notification_params *p = (void *)notification.params;
    p->fault.node_idx = 99;
    p->fault.sm_idx = 42;
    ctx.drain[false].pending_fault = p->fault;
    ctx.drain[false].notification_pending = true;
    ctx.drain[false].status.masked = true;
    remaining = 1;
    remaining_critical = true;
    fwk_notification_notify_ExpectAndReturn(&notification, NULL, FWK_E_NOMEM);
    fwk_notification_notify_IgnoreArg_count();
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&nc_event, NULL));
    TEST_ASSERT_TRUE(ctx.drain[false].notification_pending);
    TEST_ASSERT_FALSE(ctx.drain[false].waiting_critical);
    __fwk_put_event_ExpectAnyArgsAndReturn(FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, recover(false));
    fwk_notification_notify_ExpectAndReturn(&notification, NULL, FWK_SUCCESS);
    fwk_notification_notify_IgnoreArg_count();
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&nc_event, NULL));
    TEST_ASSERT_FALSE(ctx.drain[false].notification_pending);
    TEST_ASSERT_TRUE(ctx.drain[false].waiting_critical);
    TEST_ASSERT_EQUAL_UINT(1, ctx.drain[false].status.drained);
    TEST_ASSERT_EQUAL_UINT(0, ack_calls);
    p->critical = true;
    p->fault.node_idx = 33;
    p->fault.sm_idx = 17;
    p->fault.block_id = 1;
    fwk_notification_notify_ExpectAndReturn(&notification, NULL, FWK_SUCCESS);
    fwk_notification_notify_IgnoreArg_count();
    fwk_interrupt_enable_ExpectAndReturn(12, FWK_SUCCESS);
    fwk_interrupt_enable_ExpectAndReturn(13, FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&c_event, NULL));
    TEST_ASSERT_EQUAL_UINT(1, ack_calls);
    TEST_ASSERT_FALSE(ctx.drain[false].status.masked);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, ctx.drain[false].status.last_error);
}

void test_exact_critical_budget_releases_parked_noncritical_without_another_irq(void)
{
    struct fwk_event event = drain_event(true);
    remaining = FMU_DRAIN_BUDGET;
    remaining_critical = true;
    ctx.drain[false].status.masked = true;
    ctx.drain[false].waiting_critical = true;
    fwk_notification_notify_IgnoreAndReturn(FWK_SUCCESS);
    fwk_interrupt_enable_ExpectAndReturn(12, FWK_SUCCESS);
    fwk_interrupt_enable_ExpectAndReturn(13, FWK_SUCCESS);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, process_event(&event, NULL));
    TEST_ASSERT_EQUAL_UINT(FMU_DRAIN_BUDGET, ack_calls);
    TEST_ASSERT_FALSE(ctx.drain[false].waiting_critical);
    TEST_ASSERT_FALSE(ctx.drain[false].status.masked);
}

int fmu_test_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fmu_init);
    RUN_TEST(test_fmu_device_init);
    RUN_TEST(test_fmu_start);
    RUN_TEST(test_fmu_process_bind_request);
    RUN_TEST(test_fmu_isr_only_masks_and_queues);
    RUN_TEST(test_fmu_event_preserves_fault_identity);
    RUN_TEST(test_fmu_ack_failure_keeps_irq_masked_and_can_recover);
    RUN_TEST(test_fmu_queue_failure_is_observable);
    RUN_TEST(test_fmu_event_budget_rearms_for_isr_continuation);
    RUN_TEST(test_fmu_final_peek_rearm_preserves_new_fault_isr_state);
    RUN_TEST(test_fmu_budget_rearm_queue_failure_retains_mask_for_recovery);
    RUN_TEST(test_fmu_rearm_failure_keeps_irq_masked);
    RUN_TEST(test_fmu_notification_failure_is_observable);
    RUN_TEST(test_fmu_notification_retry_does_not_reack_or_duplicate);
    RUN_TEST(test_fmu_resume_partial_ack_before_notification);
    RUN_TEST(test_fmu_set_count_unsupported);
    RUN_TEST(test_fmu_set_threshold);
    RUN_TEST(test_ni_snapshot_clears_sibling_and_root_routes_once);
    RUN_TEST(test_ni_upstream_ack_failure_resumes_without_duplicate_snapshot);
    RUN_TEST(test_ni_ancestor_new_fault_is_not_acknowledged_by_saved_path);
    RUN_TEST(test_queued_noncritical_drain_defers_shared_leaf_to_pending_critical);
    RUN_TEST(test_noncritical_park_releases_after_critical_drain_and_ignores_stale_event);
    RUN_TEST(test_noncritical_park_survives_critical_ack_failure_until_recovery);
    RUN_TEST(test_noncritical_park_releases_after_failed_critical_queue_and_removed_source);
    RUN_TEST(test_acknowledged_noncritical_notification_retry_precedes_priority_park);
    RUN_TEST(test_exact_critical_budget_releases_parked_noncritical_without_another_irq);

    return UNITY_END();
}

#if !defined(TEST_ON_TARGET)
int main(void)
{
    return fmu_test_main();
}
#endif
