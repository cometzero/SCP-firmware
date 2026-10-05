/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     SCP Platform Support - implements support for communication with RSE.
 */

#include <internal/si0_platform.h>

#include <mod_si0_platform.h>
#include <mod_scmi_system_power.h>
#include <mod_timer.h>
#include <mod_transport.h>

#include <fwk_id.h>
#include <fwk_core.h>
#include <fwk_interrupt.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_status.h>

#include <stdbool.h>

/* Platform RSE context */
struct platform_rse_ctx {
    /* Pointer to the module config data */
    const struct mod_si0_platform_config *config;

    /* Transport API to send/respond to a message */
    const struct mod_transport_firmware_api *transport_api;

    /* Timer API */
    const struct mod_timer_api *timer_api;

    const struct mod_scmi_system_power_platform_api *sys_power_api;
    const struct mod_timer_alarm_api *alarm_api;
    unsigned int generation;
    uint64_t deadline;
    uint64_t ack_counter;
    bool recovery_pending;
    bool completion_ready;
    bool alarm_armed;

    /* Flag to indicate that the RSE doorbell has been received */
    volatile bool rse_doorbell_received;
};

static struct platform_rse_ctx ctx;

static void stop_recovery_alarm(void)
{
    unsigned int flags = fwk_interrupt_global_disable();
    /* A fired one-shot is already inactive; do not stop the shared timer. */
    if (ctx.alarm_armed) {
        ctx.alarm_armed = false;
        ctx.alarm_api->stop(ctx.config->rse_recovery_alarm_id);
    }
    fwk_interrupt_global_enable(flags);
}

struct recovery_result {
    unsigned int generation;
    int status;
};

static void recovery_alarm(uintptr_t generation)
{
    struct fwk_event event = {
        .id = FWK_ID_EVENT_INIT(
            FWK_MODULE_IDX_SI0_PLATFORM, MOD_SI0_PLATFORM_RSE_RECOVERY_POLL),
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
        .target_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
    };
    if (!ctx.recovery_pending || generation != ctx.generation)
        return;
    ctx.alarm_armed = false;
    *(unsigned int *)event.params = generation;
    if (fwk_put_event(&event) != FWK_SUCCESS) {
        ctx.recovery_pending = false;
        FWK_LOG_ERR(MOD_NAME "RSE poll enqueue failed; AP recovery will time out");
    }
}

static int schedule_recovery_poll(void)
{
    int status;
    ctx.alarm_armed = true;
    status = ctx.alarm_api->start(ctx.config->rse_recovery_alarm_id, 10000,
        MOD_TIMER_ALARM_TYPE_ONCE, recovery_alarm, ctx.generation);
    if (status != FWK_SUCCESS)
        ctx.alarm_armed = false;
    return status;
}

void poll_rse_recovery(unsigned int generation)
{
    struct fwk_event event = {
        .id = FWK_ID_EVENT_INIT(
            FWK_MODULE_IDX_SI0_PLATFORM, MOD_SI0_PLATFORM_RSE_RECOVERY_DONE),
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
        .target_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
    };
    struct recovery_result *result = (void *)event.params;
    uint64_t now;
    int status;

    if (!ctx.recovery_pending || generation != ctx.generation)
        return;
    status = ctx.timer_api->get_counter(ctx.config->timer_id, &now);
    if (status == FWK_SUCCESS) {
        if (!ctx.rse_doorbell_received && now < ctx.deadline) {
            status = schedule_recovery_poll();
            if (status == FWK_SUCCESS)
                return;
        } else {
            status = (ctx.rse_doorbell_received && ctx.ack_counter <= ctx.deadline) ?
                FWK_SUCCESS : FWK_E_TIMEOUT;
        }
    }
    ctx.recovery_pending = false;
    ctx.completion_ready = true;
    stop_recovery_alarm();
    result->generation = ctx.generation;
    result->status = status;
    status = fwk_put_event(&event);
    if (status != FWK_SUCCESS) {
        ctx.completion_ready = false;
        FWK_LOG_ERR(MOD_NAME "RSE recovery completion enqueue failed: %d", status);
    }
}

int start_rse_recovery(void)
{
    uint64_t now, duration;
    int status;

    if (ctx.recovery_pending || ctx.completion_ready)
        return FWK_E_BUSY;
    status = ctx.timer_api->get_counter(ctx.config->timer_id, &now);
    if (status != FWK_SUCCESS)
        return status;
    status = ctx.timer_api->time_to_timestamp(
        ctx.config->timer_id, ctx.config->rse_recovery_timeout_us, &duration);
    if (status != FWK_SUCCESS)
        return status;
    ctx.deadline = now + duration;
    ctx.generation++;
    ctx.rse_doorbell_received = false;
    ctx.recovery_pending = true;
    status = schedule_recovery_poll();
    if (status == FWK_SUCCESS)
        status = ctx.sys_power_api->notify_warm_reset();
    if (status != FWK_SUCCESS) {
        ctx.recovery_pending = false;
        stop_recovery_alarm();
        return status;
    }
    FWK_LOG_INFO(MOD_NAME "Waiting asynchronously for RSE BL2 reload (%u us)",
        ctx.config->rse_recovery_timeout_us);
    return FWK_SUCCESS;
}

int complete_rse_recovery(const struct fwk_event *event)
{
    const struct recovery_result *result = (const void *)event->params;

    if (!ctx.completion_ready || result->generation != ctx.generation)
        return FWK_E_STATE;
    ctx.completion_ready = false;
    return result->status;
}

/* Utility function to check if SCP platform has received doorbell from RSE */
static bool is_rse_doorbell_received(void *unused)
{
    (void)unused;

    return ctx.rse_doorbell_received;
}

/*
 * Module 'transport' signal interface implementation.
 */
static int signal_error(fwk_id_t unused)
{
    (void)unused;

    FWK_LOG_ERR(MOD_NAME "Error! Invalid response received from RSE");

    ctx.transport_api->release_transport_channel_lock(ctx.config->transport_id);

    return FWK_SUCCESS;
}

static int signal_message(fwk_id_t unused)
{
    int status;
    (void)unused;

    FWK_LOG_INFO(MOD_NAME "Received doorbell event from RSE");

    ctx.transport_api->release_transport_channel_lock(ctx.config->transport_id);

    if (ctx.recovery_pending) {
        status = ctx.timer_api->get_counter(ctx.config->timer_id, &ctx.ack_counter);
        if (status != FWK_SUCCESS)
            return status;
    }

    /* Set the flag to indicate that the RSE initialization is complete */
    ctx.rse_doorbell_received = true;

    return FWK_SUCCESS;
}

const struct mod_transport_firmware_signal_api
    platform_rse_transport_signal_api = {
        .signal_error = signal_error,
        .signal_message = signal_message,
    };

/*
 * Helper function to retrieve the 'transport' module signal API.
 */
const void *get_rse_platform_transport_signal_api(void)
{
    return &platform_rse_transport_signal_api;
}

/*
 * Wait for RSE to reload AP BL2 and acknowledge on the dedicated doorbell.
 * Local recovery sends the SCMI notification after AP cores are powered off;
 * the normal SCMI request path has already sent that notification.
 */
int notify_rse_and_wait_for_response(bool platform_origin)
{
    int status;

    ctx.rse_doorbell_received = false;

    /* The SCMI command path already notifies RSE. A local watchdog request
     * must do so explicitly, only after all AP cores have powered down. */
    if (platform_origin) {
        status = ctx.sys_power_api->notify_warm_reset();
        if (status != FWK_SUCCESS)
            return status;
    }

    FWK_LOG_INFO("[SI0 PLATFORM] Waiting for RSE AP BL2 reload acknowledgement...");

    status = ctx.timer_api->wait(
        ctx.config->timer_id,
        ctx.config->rse_sync_wait_us,
        is_rse_doorbell_received,
        NULL);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(
            "[SI0 PLATFORM] Error! No response from RSE within %u us",
            ctx.config->rse_sync_wait_us);
        return status;
    }

    FWK_LOG_INFO(
        "[SI0 PLATFORM] RSE doorbell received, proceeding with warm reset");

    return FWK_SUCCESS;
}

/*
 * Bind to timer and transport module to communicate with RSE.
 */
int platform_rse_bind(const struct mod_si0_platform_config *config)
{
    int status;
    fwk_id_t timer_api_id;
    fwk_id_t transport_api_id;

    ctx.config = config;

    if (config->ap_watchdog_irq != 0) {
        status = fwk_module_bind(
            config->rse_recovery_alarm_id, MOD_TIMER_API_ID_ALARM, &ctx.alarm_api);
        if (status != FWK_SUCCESS)
            return status;
        status = fwk_module_bind(
            FWK_ID_MODULE(FWK_MODULE_IDX_SCMI_SYSTEM_POWER),
            FWK_ID_API(FWK_MODULE_IDX_SCMI_SYSTEM_POWER,
                MOD_SCMI_SYSTEM_POWER_API_IDX_PLATFORM),
            &ctx.sys_power_api);
        if (status != FWK_SUCCESS)
            return status;
    }

    timer_api_id = FWK_ID_API(FWK_MODULE_IDX_TIMER, MOD_TIMER_API_IDX_TIMER);
    status = fwk_module_bind(config->timer_id, timer_api_id, &ctx.timer_api);
    if (status != FWK_SUCCESS) {
        return status;
    }

    transport_api_id =
        FWK_ID_API(FWK_MODULE_IDX_TRANSPORT, MOD_TRANSPORT_API_IDX_FIRMWARE);
    return fwk_module_bind(
        config->transport_id, transport_api_id, &ctx.transport_api);
}
