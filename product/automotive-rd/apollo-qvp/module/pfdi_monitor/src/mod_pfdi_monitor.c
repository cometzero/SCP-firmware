/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mod_pfdi_monitor.h>
#include <mod_power_domain.h>
#include <mod_timer.h>

#include <fwk_assert.h>
#include <fwk_core.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_notification.h>

#define MOD_NAME "[PFDI_MONITOR] "

enum pfdi_monitor_event_idx {
    PFDI_MONITOR_EVENT_IDX_OOR_STATUS,
    PFDI_MONITOR_EVENT_IDX_ONL_STATUS,
    PFDI_MONITOR_EVENT_IDX_TIMEOUT,
    PFDI_MONITOR_EVENT_IDX_COUNT
};

enum pfdi_monitor_core_state {
    PFDI_MONITOR_STATE_WAIT_FOR_OOR,
    PFDI_MONITOR_STATE_WAIT_FOR_BOOT,
    PFDI_MONITOR_STATE_WAIT_FOR_ONL,
};

/* Core context */
struct pfdi_monitor_core_context {
    /* Timer alarm API */
    const struct mod_timer_alarm_api *alarm_api;
    /* Wait for boot flag */
    enum pfdi_monitor_core_state core_state;
    /* PFDI core configuration data */
    const struct mod_pfdi_monitor_core_config *core_cfg;
    unsigned int generation;
    bool powered_off;
    bool online_seen;
    uint32_t last_status;
    uint32_t faults;
};

struct pfdi_monitor_event_params {
    uint32_t status;
    unsigned int generation;
};

/* Module context */
struct pfdi_monitor_ctx {
    /* PFDI core context data */
    struct pfdi_monitor_core_context *core_ctx_table;
    /* Number of cores running the PFDI */
    uint32_t core_count;
};

static struct pfdi_monitor_ctx ctx;

static fwk_id_t pd_transition_notification_id = FWK_ID_NOTIFICATION_INIT(
    FWK_MODULE_IDX_POWER_DOMAIN,
    MOD_PD_NOTIFICATION_IDX_POWER_STATE_TRANSITION);

static int pfdi_monitor_oor_status(fwk_id_t id, uint32_t status)
{
    int ret;
    unsigned int element_idx = fwk_id_get_element_idx(id);
    struct fwk_event event = {
        .source_id = FWK_ID_MODULE(FWK_MODULE_IDX_PFDI_MONITOR),
        .target_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_PFDI_MONITOR, element_idx),
        .id = FWK_ID_EVENT_INIT(
            FWK_MODULE_IDX_PFDI_MONITOR, PFDI_MONITOR_EVENT_IDX_OOR_STATUS),
    };

    if (element_idx >= ctx.core_count) {
        return FWK_E_PARAM;
    }

    ((struct pfdi_monitor_event_params *)event.params)->status = status;
    ((struct pfdi_monitor_event_params *)event.params)->generation =
        ctx.core_ctx_table[element_idx].generation;
    ret = fwk_put_event(&event);
    if (ret != FWK_SUCCESS) {
        /*
         * Do not return an error. Simply discard the event with
         * an error message, to prevent a malicious PFDI caller
         * from overwhelming the PFDI monitor causing
         * a Denial of Service attack.
         */
        FWK_LOG_ERR(
            MOD_NAME "Error! Failed to add OoR event for %s",
            fwk_module_get_element_name(id));
    }

    return FWK_SUCCESS;
}

static int pfdi_monitor_onl_status(fwk_id_t id, uint32_t status)
{
    int ret;
    unsigned int element_idx = fwk_id_get_element_idx(id);
    struct fwk_event event = {
        .source_id = FWK_ID_MODULE(FWK_MODULE_IDX_PFDI_MONITOR),
        .target_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_PFDI_MONITOR, element_idx),
        .id = FWK_ID_EVENT_INIT(
            FWK_MODULE_IDX_PFDI_MONITOR, PFDI_MONITOR_EVENT_IDX_ONL_STATUS),
    };

    if (element_idx >= ctx.core_count) {
        return FWK_E_PARAM;
    }

    ((struct pfdi_monitor_event_params *)event.params)->status = status;
    ((struct pfdi_monitor_event_params *)event.params)->generation =
        ctx.core_ctx_table[element_idx].generation;
    ret = fwk_put_event(&event);
    if (ret != FWK_SUCCESS) {
        /*
         * Do not return an error. Simply discard the event with
         * an error message, to prevent a malicious PFDI caller
         * from overwhelming the PFDI monitor causing
         * a Denial of Service attack.
         */
        FWK_LOG_ERR(
            MOD_NAME "Error! Failed to add Onl event for %s",
            fwk_module_get_element_name(id));
    }

    return FWK_SUCCESS;
}

static struct mod_pfdi_monitor_api pfdi_monitor_api = {
    .oor_status = pfdi_monitor_oor_status,
    .onl_status = pfdi_monitor_onl_status,
};

static void pfdi_monitor_timeout(uintptr_t id)
{
    int status;
    uint32_t element_idx = (uint32_t)id;
    struct fwk_event event = {
        .source_id = FWK_ID_MODULE(FWK_MODULE_IDX_PFDI_MONITOR),
        .target_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_PFDI_MONITOR, element_idx),
        .id = FWK_ID_EVENT_INIT(
            FWK_MODULE_IDX_PFDI_MONITOR, PFDI_MONITOR_EVENT_IDX_TIMEOUT),
    };

    if (element_idx >= ctx.core_count) {
        fwk_trap();
    }

    ((struct pfdi_monitor_event_params *)event.params)->generation =
        ctx.core_ctx_table[element_idx].generation;
    status = fwk_put_event(&event);

    if (status != FWK_SUCCESS) {
        /*
         * Do not return an error. Simply discard the event with
         * an error message, to prevent a malicious PFDI caller
         * from overwhelming the PFDI monitor causing
         * a Denial of Service attack.
         */
        FWK_LOG_ERR(
            MOD_NAME "Error! Failed to add timeout event for %s",
            fwk_module_get_element_name(fwk_id_build_element_id(
                fwk_module_id_pfdi_monitor, element_idx)));
    }
}

static int pfdi_monitor_init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *data)
{
    if (element_count == 0) {
        return FWK_E_PARAM;
    }

    ctx.core_ctx_table =
        fwk_mm_calloc(element_count, sizeof(struct pfdi_monitor_core_context));

    ctx.core_count = element_count;

    return FWK_SUCCESS;
}

static int pfdi_monitor_element_init(
    fwk_id_t id,
    unsigned int sub_element_count,
    const void *data)
{
    struct pfdi_monitor_core_context *core_ctx;
    const struct mod_pfdi_monitor_core_config *core_cfg;
    unsigned int element_idx = fwk_id_get_element_idx(id);

    if (element_idx >= ctx.core_count) {
        return FWK_E_PARAM;
    }

    core_cfg = (struct mod_pfdi_monitor_core_config *)data;

    if (core_cfg == NULL) {
        return FWK_E_DATA;
    }

    core_ctx = &ctx.core_ctx_table[element_idx];
    core_ctx->core_cfg = core_cfg;
    core_ctx->core_state = PFDI_MONITOR_STATE_WAIT_FOR_OOR;

    return FWK_SUCCESS;
}

static int pfdi_monitor_start(fwk_id_t id)
{
    int status;
    struct pfdi_monitor_core_context *core_ctx;
    const struct mod_pfdi_monitor_core_config *core_cfg;
    unsigned int element_idx;

    if (fwk_id_is_type(id, FWK_ID_TYPE_MODULE)) {
        return FWK_SUCCESS;
    }

    element_idx = fwk_id_get_element_idx(id);

    if (element_idx >= ctx.core_count) {
        return FWK_E_PARAM;
    }

    core_ctx = &ctx.core_ctx_table[element_idx];
    core_cfg = core_ctx->core_cfg;

    status = fwk_notification_subscribe(
        pd_transition_notification_id, core_cfg->pd_source_id, id);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(
            MOD_NAME "Failed to subscribe to Power Domain notification for %s",
            fwk_module_get_element_name(id));
    } else {
        FWK_LOG_DEBUG(
            MOD_NAME "Subscribed to Power Domain notifications for %s",
            fwk_module_get_element_name(id));
    }

    status = core_ctx->alarm_api->start(
        core_cfg->alarm_id,
        core_cfg->oor_pfdi_period_us,
        MOD_TIMER_ALARM_TYPE_ONCE,
        pfdi_monitor_timeout,
        (uintptr_t)fwk_id_get_element_idx(id));

    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(
            MOD_NAME "Error! Failed to start PFDI monitoring for %s",
            fwk_module_get_element_name(id));
    } else {
        FWK_LOG_INFO(
            MOD_NAME "Started PFDI monitoring for %s",
            fwk_module_get_element_name(id));
        FWK_LOG_DEBUG(
            MOD_NAME "%s waiting for OoR PFDI status ...",
            fwk_module_get_element_name(id));
    }

    return status;
}

static int pfdi_monitor_bind(fwk_id_t id, unsigned int round)
{
    unsigned int element_idx;
    struct pfdi_monitor_core_context *core_ctx;
    const struct mod_pfdi_monitor_core_config *core_cfg;

    if (round != 0) {
        return FWK_SUCCESS;
    }

    if (fwk_id_is_type(id, FWK_ID_TYPE_MODULE)) {
        return FWK_SUCCESS;
    }

    element_idx = fwk_id_get_element_idx(id);

    if (element_idx >= ctx.core_count) {
        return FWK_E_PARAM;
    }

    core_ctx = &ctx.core_ctx_table[element_idx];
    core_cfg = core_ctx->core_cfg;

    return fwk_module_bind(
        core_cfg->alarm_id, MOD_TIMER_API_ID_ALARM, &core_ctx->alarm_api);
}

static int pfdi_monitor_prepare_restart(fwk_id_t power_domain_id)
{
    struct pfdi_monitor_core_context *core;
    unsigned int i;
    int status;

    for (i = 0; i < ctx.core_count; i++) {
        core = &ctx.core_ctx_table[i];
        if (!fwk_id_is_equal(core->core_cfg->pd_source_id, power_domain_id))
            continue;
        if (!core->powered_off)
            return FWK_E_STATE;
        status = core->alarm_api->stop(core->core_cfg->alarm_id);
        if (status != FWK_SUCCESS && status != FWK_E_STATE)
            return status;
        core->generation++;
        core->online_seen = false;
        core->faults = 0;
        core->last_status = 0;
        core->core_state = PFDI_MONITOR_STATE_WAIT_FOR_OOR;
        return FWK_SUCCESS;
    }
    return FWK_E_PARAM;
}

static const struct mod_pfdi_monitor_restart_api restart_api = {
    .prepare = pfdi_monitor_prepare_restart,
};

static int pfdi_monitor_get_status(
    fwk_id_t core_id, struct mod_pfdi_monitor_status *status)
{
    unsigned int index = fwk_id_get_element_idx(core_id);
    const struct pfdi_monitor_core_context *core;

    if (status == NULL || index >= ctx.core_count)
        return FWK_E_PARAM;
    core = &ctx.core_ctx_table[index];
    *status = (struct mod_pfdi_monitor_status) {
        .generation = core->generation,
        .last_status = core->last_status,
        .faults = core->faults,
        .online = !core->powered_off && core->online_seen &&
            core->core_state == PFDI_MONITOR_STATE_WAIT_FOR_ONL,
        .powered_off = core->powered_off,
    };
    return FWK_SUCCESS;
}

static const struct mod_pfdi_monitor_status_api status_api = {
    .get = pfdi_monitor_get_status,
};

static int pfdi_monitor_process_bind_request(
    fwk_id_t source_id,
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void **api)
{
    int status;
    enum mod_pfdi_monitor_api_idx api_idx;

    if (!api) {
        return FWK_E_PARAM;
    }

    api_idx = (enum mod_pfdi_monitor_api_idx)fwk_id_get_api_idx(api_id);

    switch (api_idx) {
    case MOD_PFDI_MONITOR_API_IDX_STATUS:
        *api = &status_api;
        return FWK_SUCCESS;
    case MOD_PFDI_MONITOR_API_IDX_RESTART:
        if (!fwk_id_is_equal(source_id, FWK_ID_MODULE(FWK_MODULE_IDX_SI0_PLATFORM)))
            return FWK_E_ACCESS;
        *api = &restart_api;
        return FWK_SUCCESS;
    case MOD_PFDI_MONITOR_API_IDX_PFDI_MONITOR:
        *api = &pfdi_monitor_api;
        status = FWK_SUCCESS;
        break;

    default:
        status = FWK_E_PARAM;
        break;
    }

    return status;
}

static int pfdi_monitor_process_event(
    const struct fwk_event *event,
    struct fwk_event *resp_event)
{
    struct pfdi_monitor_core_context *core_ctx;
    const struct mod_pfdi_monitor_core_config *core_cfg;
    unsigned int element_idx;
    int status;

    if (!event) {
        return FWK_E_PARAM;
    }

    element_idx = fwk_id_get_element_idx(event->target_id);
    if (element_idx >= ctx.core_count) {
        return FWK_E_PARAM;
    }

    core_ctx = &ctx.core_ctx_table[element_idx];
    core_cfg = core_ctx->core_cfg;

    if (((const struct pfdi_monitor_event_params *)event->params)->generation !=
        core_ctx->generation)
        return FWK_SUCCESS; /* Queued status/timeout from the previous boot. */

    switch (fwk_id_get_event_idx(event->id)) {
    case (unsigned int)PFDI_MONITOR_EVENT_IDX_OOR_STATUS:

        /* Check if the core is in a wrong state */
        if (core_ctx->core_state != PFDI_MONITOR_STATE_WAIT_FOR_OOR) {
            core_ctx->faults |= MOD_PFDI_FAULT_PROTOCOL;
            FWK_LOG_ERR(
                MOD_NAME
                "Received OoR PFDI status for %s after Onl PFDI status",
                fwk_module_get_element_name(event->target_id));

            return FWK_E_ACCESS;
        }

        /* Stop the alarm */
        status = core_ctx->alarm_api->stop(core_cfg->alarm_id);
        if ((status != FWK_SUCCESS) && (status != FWK_E_STATE)) {
            FWK_LOG_ERR(
                MOD_NAME "Error! Failed to stop PFDI monitoring alarm for %s",
                fwk_module_get_element_name(event->target_id));

            return status;
        }

        /* Check if the OoR PFDI succeeded */
        core_ctx->last_status =
            ((const struct pfdi_monitor_event_params *)event->params)->status;
        if (core_ctx->last_status != 0) {
            core_ctx->faults |= MOD_PFDI_FAULT_OOR;
            FWK_LOG_ERR(
                MOD_NAME "OoR PFDI for %s failed, stopping PFDI monitoring",
                fwk_module_get_element_name(event->target_id));

            return FWK_SUCCESS;
        }

        /* Restart the alarm to wait for boot */
        status = core_ctx->alarm_api->start(
            core_cfg->alarm_id,
            core_cfg->boot_timeout_us,
            MOD_TIMER_ALARM_TYPE_ONCE,
            pfdi_monitor_timeout,
            (uintptr_t)element_idx);

        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR(
                MOD_NAME "Error! Failed to start PFDI monitoring alarm for %s",
                fwk_module_get_element_name(event->target_id));

            return status;
        }

        /* Change the state to wait for boot */
        core_ctx->core_state = PFDI_MONITOR_STATE_WAIT_FOR_BOOT;

        FWK_LOG_DEBUG(
            MOD_NAME "OoR PFDI for %s succeeded, waiting for boot ...",
            fwk_module_get_element_name(event->target_id));

        return FWK_SUCCESS;

    case (unsigned int)PFDI_MONITOR_EVENT_IDX_ONL_STATUS:

        /* Check if the core is in a wrong state */
        if (core_ctx->core_state == PFDI_MONITOR_STATE_WAIT_FOR_OOR) {
            core_ctx->faults |= MOD_PFDI_FAULT_PROTOCOL;
            FWK_LOG_ERR(
                MOD_NAME
                "Received Onl PFDI status for %s before OoR PFDI status",
                fwk_module_get_element_name(event->target_id));

            return FWK_E_ACCESS;
        }

        /* Stop the alarm */
        status = core_ctx->alarm_api->stop(core_cfg->alarm_id);
        if ((status != FWK_SUCCESS) && (status != FWK_E_STATE)) {
            FWK_LOG_ERR(
                MOD_NAME "Error! Failed to stop PFDI monitoring alarm for %s",
                fwk_module_get_element_name(event->target_id));

            return status;
        }

        /* Check if the Onl PFDI succeeded */
        core_ctx->last_status =
            ((const struct pfdi_monitor_event_params *)event->params)->status;
        if (core_ctx->last_status != 0) {
            core_ctx->faults |= MOD_PFDI_FAULT_ONLINE;
            FWK_LOG_ERR(
                MOD_NAME "Onl PFDI for %s failed, stopping PFDI monitoring",
                fwk_module_get_element_name(event->target_id));

            return FWK_SUCCESS;
        }

        /* Restart the alarm to wait for Onl PFDI again */
        status = core_ctx->alarm_api->start(
            core_cfg->alarm_id,
            core_cfg->onl_pfdi_period_us,
            MOD_TIMER_ALARM_TYPE_ONCE,
            pfdi_monitor_timeout,
            (uintptr_t)element_idx);
        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR(
                MOD_NAME "Error! Failed to start PFDI monitoring alarm for %s",
                fwk_module_get_element_name(event->target_id));

            return status;
        }

        /* Change the state to wait for Onl PFDI */
        core_ctx->core_state = PFDI_MONITOR_STATE_WAIT_FOR_ONL;
        core_ctx->online_seen = true;

        FWK_LOG_DEBUG(
            MOD_NAME "Onl PFDI for %s succeeded",
            fwk_module_get_element_name(event->target_id));

        return FWK_SUCCESS;

    case (unsigned int)PFDI_MONITOR_EVENT_IDX_TIMEOUT:
        core_ctx->faults |= MOD_PFDI_FAULT_TIMEOUT;
        FWK_LOG_ERR(
            MOD_NAME "Error! PFDI monitor timeout for %s",
            fwk_module_get_element_name(event->target_id));

        return FWK_SUCCESS;

    default:
        FWK_LOG_ERR(
            MOD_NAME "Invalid PFDI monitor event: %s", FWK_ID_STR(event->id));

        return FWK_E_PARAM;
    }
}

int pfdi_monitor_process_notificiation(
    const struct fwk_event *event,
    struct fwk_event *resp_event)
{
    struct mod_pd_power_state_transition_notification_params *params;
    struct pfdi_monitor_core_context *core_ctx;
    const struct mod_pfdi_monitor_core_config *core_cfg;
    unsigned int element_idx;
    int status;

    if (fwk_id_is_type(event->target_id, FWK_ID_TYPE_MODULE)) {
        return FWK_E_PARAM;
    }

    if (fwk_id_is_equal(event->id, pd_transition_notification_id)) {
        element_idx = fwk_id_get_element_idx(event->target_id);

        if (element_idx >= ctx.core_count) {
            return FWK_E_PARAM;
        }

        core_ctx = &ctx.core_ctx_table[element_idx];
        core_cfg = core_ctx->core_cfg;

        params = (struct mod_pd_power_state_transition_notification_params *)
                     event->params;

        switch (params->state) {
        case (unsigned int)MOD_PD_STATE_OFF:
        case (unsigned int)MOD_PD_STATE_OFF_0:
        case (unsigned int)MOD_PD_STATE_OFF_1:
        case (unsigned int)MOD_PD_STATE_OFF_2:
        case (unsigned int)MOD_PD_STATE_SLEEP:
            FWK_LOG_INFO(
                MOD_NAME
                "%s has been turned off, switching off PFDI monitoring",
                fwk_module_get_element_name(event->target_id));
            /* Stop the alarm */
            core_ctx->powered_off = true;
            core_ctx->online_seen = false;
            status = core_ctx->alarm_api->stop(core_cfg->alarm_id);
            if ((status != FWK_SUCCESS) && (status != FWK_E_STATE)) {
                FWK_LOG_ERR(
                    MOD_NAME
                    "Error! Failed to stop PFDI monitoring alarm for %s",
                    fwk_module_get_element_name(event->target_id));

                return status;
            }
            break;
        case (unsigned int)MOD_PD_STATE_ON:
            core_ctx->powered_off = false;
            core_ctx->online_seen = false;
            FWK_LOG_INFO(
                MOD_NAME "%s has been turned on, switching on PFDI monitoring",
                fwk_module_get_element_name(event->target_id));
            status = core_ctx->alarm_api->start(
                core_cfg->alarm_id,
                core_cfg->boot_timeout_us,
                MOD_TIMER_ALARM_TYPE_ONCE,
                pfdi_monitor_timeout,
                (uintptr_t)element_idx);
            if (status != FWK_SUCCESS) {
                FWK_LOG_ERR(
                    MOD_NAME
                    "Error! Failed to start PFDI monitoring alarm for %s",
                    fwk_module_get_element_name(event->target_id));

                return status;
            }
            break;
        default:
            /* Unsupported Power Domain notification, do nothing */
            break;
        }
    } else {
        return FWK_E_PARAM;
    }

    return FWK_SUCCESS;
}

/* Module description */
const struct fwk_module module_pfdi_monitor = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .api_count = MOD_PFDI_MONITOR_API_IDX_COUNT,
    .init = pfdi_monitor_init,
    .element_init = pfdi_monitor_element_init,
    .start = pfdi_monitor_start,
    .bind = pfdi_monitor_bind,
    .process_bind_request = pfdi_monitor_process_bind_request,
    .process_event = pfdi_monitor_process_event,
    .process_notification = pfdi_monitor_process_notificiation,
    .event_count = PFDI_MONITOR_EVENT_IDX_COUNT,
};
