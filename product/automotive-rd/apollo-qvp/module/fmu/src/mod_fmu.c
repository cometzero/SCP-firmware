/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "internal/fmu_common.h"

#include <mod_fmu.h>

#include <fwk_assert.h>
#include <fwk_core.h>
#include <fwk_id.h>
#include <fwk_interrupt.h>
#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_notification.h>
#include <fwk_status.h>

#define FMU_ROOT_IDX       0
#define FMU_MAX_TREE_DEPTH 4
#define FMU_DRAIN_BUDGET 16

enum fmu_event_idx { FMU_EVENT_DRAIN, FMU_EVENT_COUNT };

struct fault_entry {
    uint32_t dev;
    uint32_t node;
    struct mod_fmu_ack_state ack;
    struct mod_fmu_ack_state upstream_ack[FMU_MAX_TREE_DEPTH * MOD_FMU_SOURCE_COUNT];
};

struct drain_state {
    struct mod_fmu_drain_status status;
    bool queued;
    bool waiting_critical;
    struct fault_entry path[FMU_MAX_TREE_DEPTH];
    unsigned int depth;
    bool fault_tracked;
    struct mod_fmu_fault pending_fault;
#ifdef BUILD_HAS_NOTIFICATION
    bool notification_pending;
#endif
};

static struct {
    const struct mod_fmu_dev_config **device_config;
    const struct mod_fmu_impl_api **impl_apis;
    unsigned int num_devices;
    const struct mod_fmu_config *config;
    struct drain_state drain[2];
} ctx;

static unsigned int find_next_fmu(
    unsigned int device_idx,
    unsigned int node_idx)
{
    unsigned int idx;
    const struct mod_fmu_dev_config *device_config;

    if (device_idx == MOD_FMU_PARENT_NONE || node_idx == MOD_FMU_PARENT_NONE) {
        return MOD_FMU_PARENT_NONE;
    }

    for (idx = 0; idx < ctx.num_devices; idx++) {
        device_config = ctx.device_config[idx];
        if (device_config->parent != device_idx)
            continue;
        if (device_config->has_parent_sources) {
            for (unsigned int source = 0; source < MOD_FMU_SOURCE_COUNT; source++) {
                if (device_config->parent_source[source] == node_idx)
                    return idx;
            }
        } else if (device_config->parent_cr_index == node_idx ||
                   device_config->parent_ncr_index == node_idx) {
            return idx;
        }
    }

    return MOD_FMU_PARENT_NONE;
}

#ifdef BUILD_HAS_NOTIFICATION
static int notify_pending_fault(bool critical)
{
    struct drain_state *state = &ctx.drain[critical];
    unsigned int count;
    struct fwk_event event = {
        .id = mod_fmu_notification_id_fault,
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_FMU),
    };
    struct mod_fmu_fault_notification_params *params = (void *)event.params;
    params->critical = critical;
    params->fault = state->pending_fault;
    int status = fwk_notification_notify_atomic(&event, &count);
    if (status == FWK_SUCCESS) {
        state->notification_pending = false;
    }
    return status;
}
#endif

/* One NI RAS record can assert several outputs. Once its snapshot is ACKed,
 * clear the corresponding parent latches and both upstream summary classes.
 * Remaining source levels re-latch naturally; no discarded syndrome is replayed.
 * Each acknowledgement has retained progress so recovery cannot ACK twice. */
static int clear_upstream_routes(
    struct drain_state *state, struct fault_entry *entry)
{
    const struct mod_fmu_dev_config *child = ctx.device_config[entry->dev];
    if (!child->has_parent_sources)
        return FWK_SUCCESS;
    for (unsigned int depth = 0; child->parent != MOD_FMU_PARENT_NONE; depth++) {
        if (depth >= FMU_MAX_TREE_DEPTH)
            return FWK_E_DATA;
        unsigned int parent = child->parent;
        unsigned int count = child->has_parent_sources ? MOD_FMU_SOURCE_COUNT : 2;
        for (unsigned int source = 0; source < count; source++) {
            if (depth == 0 && !(entry->ack.parent_clear_mask & (1U << source)))
                continue;
            unsigned int node = child->has_parent_sources ?
                child->parent_source[source] :
                (source == 0 ? child->parent_cr_index : child->parent_ncr_index);
            struct mod_fmu_ack_state *ack =
                &entry->upstream_ack[depth * MOD_FMU_SOURCE_COUNT + source];
            /* The original walk will visit these ancestors again. Share its
             * progress so a new fault arriving after this ACK is retained. */
            for (unsigned int index = 0; index < state->depth; index++) {
                if (state->path[index].dev == parent &&
                    state->path[index].node == node) {
                    ack = &state->path[index].ack;
                    break;
                }
            }
            int status = ctx.impl_apis[parent]->fault_ack(
                ctx.device_config[parent], &state->pending_fault, node,
                &state->fault_tracked, ack);
            if (status != FWK_SUCCESS)
                return status;
        }
        child = ctx.device_config[parent];
    }
    return FWK_SUCCESS;
}

static int next_fault(bool critical)
{
    struct drain_state *state = &ctx.drain[critical];
    const struct mod_fmu_dev_config *cfg;
    unsigned int dev = FMU_ROOT_IDX;
    unsigned int node;
    unsigned int parent_node = MOD_FMU_PARENT_NONE;
    int status;
#ifdef BUILD_HAS_NOTIFICATION
    if (state->notification_pending) {
        return notify_pending_fault(critical);
    }
#endif

    if (state->depth == 0) {
        state->waiting_critical = false;
        /* ERI and FHI can refer to one NI snapshot. An older queued NC
         * event must not consume it before a currently pending C event. */
        if (!critical && ctx.impl_apis[FMU_ROOT_IDX]->fault_peek(
                ctx.device_config[FMU_ROOT_IDX], true, &node)) {
            state->waiting_critical = true;
            state->status.last_error = FWK_SUCCESS;
            return FWK_PENDING;
        }
        state->pending_fault = (struct mod_fmu_fault){ 0 };
        state->fault_tracked = false;
        /* Save the path so a failed leaf refresh or ancestor ACK can resume
         * without selecting a new fault or acknowledging the leaf twice. */
        while (dev != MOD_FMU_PARENT_NONE) {
            if (state->depth >= FMU_MAX_TREE_DEPTH) {
                return FWK_E_DATA;
            }
            cfg = ctx.device_config[dev];
            bool present = parent_node != MOD_FMU_PARENT_NONE &&
                ctx.impl_apis[dev]->fault_peek_source != NULL ?
                ctx.impl_apis[dev]->fault_peek_source(cfg, parent_node, &node) :
                ctx.impl_apis[dev]->fault_peek(cfg, critical, &node);
            if (!present)
                break;
            state->path[state->depth++] =
                (struct fault_entry){ .dev = dev, .node = node };
            parent_node = node;
            dev = find_next_fmu(dev, node);
        }
        if (state->depth == 0) {
            return FWK_PENDING;
        }
        state->pending_fault.device_idx = state->path[state->depth - 1].dev;
    }

    while (state->depth != 0) {
        struct fault_entry *entry = &state->path[state->depth - 1];
        cfg = ctx.device_config[entry->dev];
        status = ctx.impl_apis[entry->dev]->fault_ack(
            cfg, &state->pending_fault, entry->node, &state->fault_tracked,
            &entry->ack);
        if (status != FWK_SUCCESS) {
            return status;
        }
        status = clear_upstream_routes(state, entry);
        if (status != FWK_SUCCESS)
            return status;
        state->depth--;
    }

    FWK_LOG_INFO(
        MOD_NAME "%s fault received: Device: 0x%x, Node 0x%x, SM 0x%x, Block 0x%x, INJ %u",
        critical ? "Critical" : "Non-critical",
        state->pending_fault.device_idx,
        state->pending_fault.node_idx,
        state->pending_fault.sm_idx,
        state->pending_fault.block_id,
        state->pending_fault.injected);
#ifdef BUILD_HAS_NOTIFICATION
    state->notification_pending = true;
    return notify_pending_fault(critical);
#else
    return FWK_SUCCESS;
#endif
}

/*
 * Common interrupt handler
 */
static void drain_failed(bool critical, int status)
{
    ctx.drain[critical].status.last_error = status;
    FWK_LOG_ERR("FMU_DRAIN_FAIL critical=%u status=%d", critical, status);
}

static int queue_drain(bool critical)
{
    struct fwk_event event = {
        .id = FWK_ID_EVENT(FWK_MODULE_IDX_FMU, FMU_EVENT_DRAIN),
        .source_id = FWK_ID_MODULE(FWK_MODULE_IDX_FMU),
        .target_id = FWK_ID_MODULE(FWK_MODULE_IDX_FMU),
        .params = { critical },
    };
    int status;

    if (ctx.drain[critical].queued) {
        return FWK_SUCCESS;
    }
    status = fwk_put_event(&event);
    if (status == FWK_SUCCESS) {
        ctx.drain[critical].queued = true;
    } else {
        drain_failed(critical, status);
    }
    return status;
}

static void fmu_isr(bool critical)
{
    unsigned int irq = critical ? ctx.config->irq_critical :
                                 ctx.config->irq_non_critical;
    int status = fwk_interrupt_disable(irq);

    ctx.drain[critical].status.masked = true;
    if (status != FWK_SUCCESS) {
        drain_failed(critical, status);
        return;
    }
    (void)queue_drain(critical);
}

static void rearm_irq(bool critical)
{
    struct drain_state *state = &ctx.drain[critical];
    unsigned int irq = critical ? ctx.config->irq_critical :
                                 ctx.config->irq_non_critical;
    int status;

    /* A pending level may enter the ISR as soon as the IRQ is enabled.
     * Publish first, then preserve any masked/queued state set by that ISR. */
    state->status.masked = false;
    state->status.last_error = FWK_SUCCESS;
    state->waiting_critical = false;
    status = fwk_interrupt_enable(irq);
    if (status != FWK_SUCCESS) {
        state->status.masked = true;
        drain_failed(critical, status);
    }
}

static void release_noncritical_if_ready(void)
{
    unsigned int node;
    if (ctx.drain[false].waiting_critical &&
        !ctx.impl_apis[FMU_ROOT_IDX]->fault_peek(
            ctx.device_config[FMU_ROOT_IDX], true, &node))
        rearm_irq(false);
}

static int process_event(const struct fwk_event *event,
                         struct fwk_event *response)
{
    bool critical = event->params[0] != 0;
    struct drain_state *state = &ctx.drain[critical];
    int status;

    state->queued = false;
    for (unsigned int i = 0; i < FMU_DRAIN_BUDGET; i++) {
        status = next_fault(critical);
        if (status == FWK_PENDING) {
            /* Keep a deferred NC level masked until C has drained. Re-arming
             * it here would spin on NC IRQs while a failed C ACK awaits repair. */
            if (state->waiting_critical)
                return FWK_SUCCESS;
            /* The final peek observed no matching root GSR. A new level fault
             * after that observation retriggers as soon as the IRQ is enabled. */
            rearm_irq(critical);
            if (critical)
                release_noncritical_if_ready();
            return FWK_SUCCESS;
        }
        if (status != FWK_SUCCESS) {
            drain_failed(critical, status);
            return FWK_SUCCESS;
        }
        state->status.drained++;
    }
    /* Remaining faults keep the level asserted. Re-arm so the next batch is
     * queued by the ISR, behind other ISR work such as SCMI requests. Directly
     * requeueing here would keep the normal queue nonempty and starve ISR work. */
    rearm_irq(critical);
    if (critical)
        release_noncritical_if_ready();
    return FWK_SUCCESS;
}

static int recover(bool critical)
{
    if (!ctx.drain[critical].status.masked) {
        return FWK_E_STATE;
    }
    return queue_drain(critical);
}

static int get_drain_status(bool critical, struct mod_fmu_drain_status *status)
{
    if (status == NULL) {
        return FWK_E_PARAM;
    }
    *status = ctx.drain[critical].status;
    return FWK_SUCCESS;
}

/*
 * Critical fault ISR
 */
static void fmu_isr_critical(void)
{
    fmu_isr(true);
}

/*
 * Non-critical fault ISR
 */
static void fmu_isr_non_critical(void)
{
    fmu_isr(false);
}

/*
 * API Handlers
 */
static int inject(const struct mod_fmu_fault *fault)
{
    const struct mod_fmu_dev_config *config;
    int (*func)(
        const struct mod_fmu_dev_config *config,
        const struct mod_fmu_fault *fault);

    if (fault == NULL || fault->device_idx >= ctx.num_devices) {
        return FWK_E_PARAM;
    }

    func = ctx.impl_apis[fault->device_idx]->inject;
    if (func == NULL) {
        return FWK_E_SUPPORT;
    }
    config = ctx.device_config[fault->device_idx];

    return func(config, fault);
}

static int get_enabled(const struct mod_fmu_fault *fault, bool *enabled)
{
    const struct mod_fmu_dev_config *config;
    int (*func)(
        const struct mod_fmu_dev_config *config,
        const struct mod_fmu_fault *fault,
        bool *enabled);

    if (fault == NULL || enabled == NULL ||
        fault->device_idx >= ctx.num_devices) {
        return FWK_E_PARAM;
    }

    func = ctx.impl_apis[fault->device_idx]->get_enabled;
    if (func == NULL) {
        return FWK_E_SUPPORT;
    }
    config = ctx.device_config[fault->device_idx];

    return func(config, fault, enabled);
}

static int set_enabled(const struct mod_fmu_fault *fault, bool enabled)
{
    const struct mod_fmu_dev_config *config;
    int (*func)(
        const struct mod_fmu_dev_config *config,
        const struct mod_fmu_fault *fault,
        bool enabled);

    if (fault == NULL || fault->device_idx >= ctx.num_devices) {
        return FWK_E_PARAM;
    }

    func = ctx.impl_apis[fault->device_idx]->set_enabled;
    if (func == NULL) {
        return FWK_E_SUPPORT;
    }
    config = ctx.device_config[fault->device_idx];

    return func(config, fault, enabled);
}

static int get_count(fwk_id_t id, uint16_t node_id, uint8_t *count)
{
    const struct mod_fmu_dev_config *config;
    unsigned int idx;
    int (*func)(
        const struct mod_fmu_dev_config *config,
        uint16_t node_id,
        uint8_t *count);

    idx = fwk_id_get_element_idx(id);
    if (count == NULL || idx >= ctx.num_devices) {
        return FWK_E_PARAM;
    }

    func = ctx.impl_apis[idx]->get_count;
    if (func == NULL) {
        return FWK_E_SUPPORT;
    }
    config = ctx.device_config[idx];

    return func(config, node_id, count);
}

static int set_count(fwk_id_t id, uint16_t node_id, uint8_t count)
{
    const struct mod_fmu_dev_config *config;
    unsigned int idx;
    int (*func)(
        const struct mod_fmu_dev_config *config,
        uint16_t node_id,
        uint8_t count);

    idx = fwk_id_get_element_idx(id);
    if (idx >= ctx.num_devices) {
        return FWK_E_PARAM;
    }

    func = ctx.impl_apis[idx]->set_count;
    if (func == NULL) {
        return FWK_E_SUPPORT;
    }
    config = ctx.device_config[idx];

    return func(config, node_id, count);
}

static int get_threshold(fwk_id_t id, uint16_t node_id, uint8_t *threshold)
{
    const struct mod_fmu_dev_config *config;
    unsigned int idx;
    int (*func)(
        const struct mod_fmu_dev_config *config,
        uint16_t node_id,
        uint8_t *threshold);

    idx = fwk_id_get_element_idx(id);
    if (threshold == NULL || idx >= ctx.num_devices) {
        return FWK_E_PARAM;
    }

    func = ctx.impl_apis[idx]->get_threshold;
    if (func == NULL) {
        return FWK_E_SUPPORT;
    }
    config = ctx.device_config[idx];

    return func(config, node_id, threshold);
}

static int set_threshold(fwk_id_t id, uint16_t node_id, uint8_t threshold)
{
    const struct mod_fmu_dev_config *config;
    unsigned int idx;
    int (*func)(
        const struct mod_fmu_dev_config *config,
        uint16_t node_id,
        uint8_t threshold);

    idx = fwk_id_get_element_idx(id);
    if (idx >= ctx.num_devices) {
        return FWK_E_PARAM;
    }

    func = ctx.impl_apis[idx]->set_threshold;
    if (func == NULL) {
        return FWK_E_SUPPORT;
    }
    config = ctx.device_config[idx];

    return func(config, node_id, threshold);
}

static int get_upgrade_enabled(fwk_id_t id, uint16_t node_id, bool *enabled)
{
    const struct mod_fmu_dev_config *config;
    unsigned int idx;
    int (*func)(
        const struct mod_fmu_dev_config *config,
        uint16_t node_id,
        bool *enabled);

    idx = fwk_id_get_element_idx(id);
    if (enabled == NULL || idx >= ctx.num_devices) {
        return FWK_E_PARAM;
    }

    func = ctx.impl_apis[idx]->get_upgrade_enabled;
    if (func == NULL) {
        return FWK_E_SUPPORT;
    }
    config = ctx.device_config[idx];

    return func(config, node_id, enabled);
}

static int set_upgrade_enabled(fwk_id_t id, uint16_t node_id, bool enabled)
{
    const struct mod_fmu_dev_config *config;
    unsigned int idx;
    int (*func)(
        const struct mod_fmu_dev_config *config,
        uint16_t node_id,
        bool enabled);

    idx = fwk_id_get_element_idx(id);
    if (idx >= ctx.num_devices) {
        return FWK_E_PARAM;
    }

    func = ctx.impl_apis[idx]->set_upgrade_enabled;
    if (func == NULL) {
        return FWK_E_SUPPORT;
    }
    config = ctx.device_config[idx];

    return func(config, node_id, enabled);
}

static int set_critical(const struct mod_fmu_fault *fault, bool critical)
{
    const struct mod_fmu_dev_config *config;
    int (*func)(
        const struct mod_fmu_dev_config *config,
        const struct mod_fmu_fault *fault,
        bool critical);

    if (fault == NULL || fault->device_idx >= ctx.num_devices) {
        return FWK_E_PARAM;
    }

    func = ctx.impl_apis[fault->device_idx]->set_critical;
    if (func == NULL) {
        return FWK_E_SUPPORT;
    }
    config = ctx.device_config[fault->device_idx];

    return func(config, fault, critical);
}

struct mod_fmu_api fmu_api = {
    .inject = inject,
    .get_enabled = get_enabled,
    .set_enabled = set_enabled,
    .get_count = get_count,
    .set_count = set_count,
    .get_threshold = get_threshold,
    .set_threshold = set_threshold,
    .get_upgrade_enabled = get_upgrade_enabled,
    .set_upgrade_enabled = set_upgrade_enabled,
    .set_critical = set_critical,
    .recover = recover,
    .get_drain_status = get_drain_status,
};

/*
 * Framework Handlers
 */
static int fmu_init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *data)
{
    fwk_assert(data != NULL);
    fwk_assert(element_count > 0);

    ctx.config = data;
    ctx.num_devices = element_count;
    ctx.device_config =
        fwk_mm_calloc(element_count, sizeof(struct mod_fmu_dev_config *));
    ctx.impl_apis =
        fwk_mm_calloc(element_count, sizeof(struct mod_fmu_impl_api *));

    return FWK_SUCCESS;
}

extern struct mod_fmu_impl_api mod_fmu_system_api;
extern struct mod_fmu_impl_api mod_fmu_gic_mhu_api;
extern struct mod_fmu_impl_api mod_ni710ae_fmu_api;

struct mod_fmu_impl_api *implementation_apis[MOD_FMU_IMPL_COUNT] = {
    [MOD_FMU_SYSTEM_IMPL] = &mod_fmu_system_api,
    [MOD_FMU_GIC_MHU_IMPL] = &mod_fmu_gic_mhu_api,
    [MOD_FMU_NI710AE_IMPL] = &mod_ni710ae_fmu_api,
};

static int fmu_device_init(
    fwk_id_t device_id,
    unsigned int unused,
    const void *data)
{
    unsigned int element_idx = fwk_id_get_element_idx(device_id);
    int (*configure)(const struct mod_fmu_config *config);
    fwk_id_t module_id;

    fwk_assert(data != NULL);
    fwk_assert(element_idx < ctx.num_devices);

    ctx.device_config[element_idx] = data;
    ctx.impl_apis[element_idx] =
        implementation_apis[ctx.device_config[element_idx]->implementation];
    fwk_assert(ctx.impl_apis[element_idx]->fault_peek != NULL);
    fwk_assert(ctx.impl_apis[element_idx]->fault_ack != NULL);

    configure = ctx.impl_apis[element_idx]->configure;

    if (configure == NULL) {
        return FWK_SUCCESS;
    }

    module_id = FWK_ID_MODULE(fwk_id_get_module_idx(device_id));
    return configure(fwk_module_get_data(module_id));
}

static int fmu_start(fwk_id_t id)
{
    int status;
    const struct mod_fmu_config *config = fwk_module_get_data(id);

    if (!fwk_id_is_type(id, FWK_ID_TYPE_MODULE)) {
        return FWK_SUCCESS;
    }

    status = fwk_interrupt_set_isr(config->irq_critical, &fmu_isr_critical);
    if (status != FWK_SUCCESS) {
        return status;
    }

    status =
        fwk_interrupt_set_isr(config->irq_non_critical, &fmu_isr_non_critical);
    if (status != FWK_SUCCESS) {
        return status;
    }

#if (PLATFORM_VARIANT == APOLLO_FVP_VARIANT_FVP)
    status = fwk_interrupt_enable(config->irq_critical);
    if (status != FWK_SUCCESS) {
        return status;
    }

    status = fwk_interrupt_enable(config->irq_non_critical);
    if (status != FWK_SUCCESS) {
        return status;
    }
#else
    FWK_LOG_WARN(MOD_NAME
                 "FMU interrupts are disabled due to limitations in the RTL "
                 "implementation!");
#endif

    return FWK_SUCCESS;
}

static int fmu_bind(fwk_id_t id, unsigned int round)
{
    unsigned int element_idx;
    int (*func)(fwk_id_t id);

    if ((round != 0) || (fwk_id_is_type(id, FWK_ID_TYPE_MODULE))) {
        return FWK_SUCCESS;
    }

    element_idx = fwk_id_get_element_idx(id);
    func = ctx.impl_apis[element_idx]->bind;

    if (func == NULL) {
        return FWK_SUCCESS;
    }

    return func(id);
}

static int fmu_process_bind_request(
    fwk_id_t requester_id,
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void **api)
{
    switch (fwk_id_get_api_idx(api_id)) {
    case MOD_FMU_DEVICE_API_IDX:
        *api = &fmu_api;
        break;
    default:
        return FWK_E_PARAM;
    }
    return FWK_SUCCESS;
}

const struct fwk_module module_fmu = {
    .type = FWK_MODULE_TYPE_DRIVER,
    .init = fmu_init,
    .element_init = fmu_device_init,
    .start = fmu_start,
    .bind = fmu_bind,
    .api_count = MOD_FMU_API_COUNT,
    .event_count = FMU_EVENT_COUNT,
    .process_event = process_event,
    .process_bind_request = fmu_process_bind_request,
#ifdef BUILD_HAS_NOTIFICATION
    .notification_count = (unsigned int)MOD_FMU_NOTIFICATION_IDX_COUNT,
#endif
};
