/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     SCP platform sub-system initialization support.
 */

#include "internal/si0_platform.h"
#include "platform_core.h"
#include "si0_cfgd_power_domain.h"
#include "si0_cfgd_scmi.h"
#include "si0_cfgd_sds.h"

#include <mod_power_domain.h>
#include <mod_apcontext.h>
#include <mod_pfdi_monitor.h>
#include <mod_ppu_v1.h>
#include <mod_scmi.h>
#include <mod_sds.h>
#include <mod_si0_platform.h>
#include <mod_transport.h>
#include <mod_timer.h>

#ifdef BUILD_HAS_MOD_PMIC
#include <mod_pmic.h>
#endif

#include <fwk_core.h>
#include <fwk_id.h>
#include <fwk_interrupt.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_notification.h>
#include <fwk_status.h>
#include <inttypes.h>

#ifdef BUILD_HAS_NOTIFICATION
static const fwk_id_t mod_pd_notification_id_pre_warmreset =
    FWK_ID_NOTIFICATION_INIT(
        FWK_MODULE_IDX_POWER_DOMAIN,
        MOD_PD_NOTIFICATION_IDX_PRE_WARM_RESET);

static fwk_id_t pd_transition_notification_id = FWK_ID_NOTIFICATION_INIT(
    FWK_MODULE_IDX_POWER_DOMAIN,
    MOD_PD_NOTIFICATION_IDX_POWER_STATE_TRANSITION);
#endif /* BUILD_HAS_NOTIFICATION */

#define SI0_WARM_RESET_SYNDROME_VALUE (0x8U)

static const fwk_id_t sds_reset_syndrome_id = FWK_ID_ELEMENT_INIT(
    FWK_MODULE_IDX_SDS,
    SI0_CFGD_MOD_SDS_EIDX_RESET_SYNDROME);

/* Module context */
struct si0_platform_ctx {
    /* Pointer to the Interrupt Service Routine API of the PPU_V1 module */
    const struct ppu_v1_isr_api *ppu_v1_isr_api;

    /* Power domain module restricted API pointer */
    struct mod_pd_restricted_api *mod_pd_restricted_api;

    /* SDS API pointer */
    const struct mod_sds_api *sds_api;
    const struct mod_apcontext_reset_api *apcontext_api;
    const struct mod_pfdi_monitor_restart_api *pfdi_restart_api;
    const struct mod_timer_api *rearm_timer_api;
    const struct mod_timer_alarm_api *rearm_alarm_api;

#ifdef BUILD_HAS_MOD_PMIC
    const struct mod_pmic_api *pmic_api;
#endif

    /* Config containig data required for platform initialization */
    const struct mod_si0_platform_config *config;

    /* Count of number of warm reset completion check iterations */
    unsigned int warm_reset_check_cnt;
};
static struct si0_platform_ctx si0_platform_ctx;
static bool ap_watchdog_recovery;
static struct {
    bool pending;
    unsigned int generation;
    unsigned int attempts;
    uint64_t deadline;
    bool last_pending;
} rearm_ctx;

static enum mod_si0_recovery_state recovery_state;
static enum mod_si0_ap_power_state ap_power_state;
static struct {
    bool active;
    bool stopping_ap;
    bool log_first_poll;
    bool alarm_armed;
    uint64_t callback_time;
    uint64_t deadline;
    unsigned int generation;
    const struct mod_timer_alarm_api *alarm;
} control;

static int power_off_all_cores(void);
static int check_power_off_all_cores(void);

static void control_stop_alarm(void)
{
    unsigned int flags = fwk_interrupt_global_disable();
    if (control.alarm_armed) {
        control.alarm_armed = false;
        control.alarm->stop(si0_platform_ctx.config->control_alarm_id);
    }
    fwk_interrupt_global_enable(flags);
}

static void control_fail(int status)
{
    uint64_t now = 0;
    if (si0_platform_ctx.rearm_timer_api != NULL)
        (void)si0_platform_ctx.rearm_timer_api->get_counter(
            si0_platform_ctx.config->timer_id, &now);
    FWK_LOG_ERR(MOD_NAME "CTRL fail e=%d g=%u p=%u r=%u s=%u t=%" PRIu64 " d=%" PRIu64,
        status, control.generation, ap_power_state, recovery_state,
        control.stopping_ap, now, control.deadline);
    recovery_state = SI0_RECOVERY_FAILED;
    ap_power_state = SI0_AP_FAILED;
    control.active = false;
    control.stopping_ap = false;
    rearm_ctx.pending = false;
    rearm_ctx.generation++;
    control_stop_alarm();
}

/* Completion events can already be queued when the overall deadline expires.
 * Check the transaction again before releasing AP or publishing COMPLETE. */
static bool recovery_completion_allowed(enum mod_si0_recovery_state expected)
{
    uint64_t now;
    int status;

    if (!control.active || recovery_state != expected)
        return false;
    status = si0_platform_ctx.rearm_timer_api->get_counter(
        si0_platform_ctx.config->timer_id, &now);
    if (status != FWK_SUCCESS || now >= control.deadline) {
        control_fail(status == FWK_SUCCESS ? FWK_E_TIMEOUT : status);
        return false;
    }
    return true;
}

static void control_callback(uintptr_t generation)
{
    struct fwk_event event = {
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
        .target_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
        .id = FWK_ID_EVENT_INIT(FWK_MODULE_IDX_SI0_PLATFORM,
            MOD_SI0_PLATFORM_CONTROL_POLL),
    };
    if (!control.active || generation != control.generation)
        return;
    control.alarm_armed = false;
    (void)si0_platform_ctx.rearm_timer_api->get_counter(
        si0_platform_ctx.config->timer_id, &control.callback_time);
    *(unsigned int *)event.params = generation;
    if (fwk_put_event(&event) != FWK_SUCCESS)
        control_fail(FWK_E_BUSY);
}

static int control_schedule(void)
{
    int status;
    control.alarm_armed = true;
    status = control.alarm->start(si0_platform_ctx.config->control_alarm_id,
        10000, MOD_TIMER_ALARM_TYPE_ONCE, control_callback, control.generation);
    if (status != FWK_SUCCESS)
        control.alarm_armed = false;
    return status;
}

static int control_start(unsigned int timeout_us)
{
    uint64_t now, duration;
    int status;
    const struct mod_si0_platform_config *config = si0_platform_ctx.config;
    if (control.alarm == NULL)
        return FWK_E_SUPPORT;
    control_stop_alarm();
    status = si0_platform_ctx.rearm_timer_api->get_counter(config->timer_id, &now);
    if (status != FWK_SUCCESS)
        return status;
    status = si0_platform_ctx.rearm_timer_api->time_to_timestamp(
        config->timer_id, timeout_us, &duration);
    if (status != FWK_SUCCESS)
        return status;
    control.deadline = now + duration;
    control.generation++;
    control.active = true;
    control.log_first_poll = true;
    control.callback_time = 0;
    FWK_LOG_INFO(MOD_NAME "CTRL start g=%u us=%u p=%u r=%u s=%u t=%" PRIu64 " d=%" PRIu64,
        control.generation, timeout_us, ap_power_state, recovery_state,
        control.stopping_ap, now, control.deadline);
    status = control_schedule();
    if (status != FWK_SUCCESS)
        control.active = false;
    return status;
}

static unsigned int recovery_status(void) { return recovery_state; }
static unsigned int power_status(void) { return ap_power_state; }

static int request_recovery(void)
{
    struct fwk_event event = {
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
        .target_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
        .id = FWK_ID_EVENT_INIT(FWK_MODULE_IDX_SI0_PLATFORM,
            MOD_SI0_PLATFORM_AP_WATCHDOG),
    };
    int status;
    if (si0_platform_ctx.config->ap_watchdog_irq == 0)
        return FWK_E_SUPPORT;
    if (ap_watchdog_recovery || control.active)
        return FWK_E_BUSY;
    status = fwk_interrupt_disable(si0_platform_ctx.config->ap_watchdog_irq);
    if (status != FWK_SUCCESS)
        return status;
    ap_watchdog_recovery = true;
    recovery_state = SI0_RECOVERY_PENDING;
    ap_power_state = SI0_AP_WAKING;
    status = fwk_put_event(&event);
    if (status != FWK_SUCCESS) {
        ap_watchdog_recovery = false;
        recovery_state = SI0_RECOVERY_FAILED;
        ap_power_state = SI0_AP_FAILED;
        (void)fwk_interrupt_enable(si0_platform_ctx.config->ap_watchdog_irq);
    }
    return status;
}

static int power_arm(bool arm)
{
    int status;
    if (!arm) {
        if (ap_power_state != SI0_AP_ARMED)
            return FWK_E_STATE;
        control_stop_alarm();
        control.active = false;
        ap_power_state = SI0_AP_RUN;
        return FWK_SUCCESS;
    }
    if (ap_power_state != SI0_AP_RUN || ap_watchdog_recovery)
        return FWK_E_BUSY;
    status = control_start(30000000);
    if (status == FWK_SUCCESS)
        ap_power_state = SI0_AP_ARMED;
    return status;
}

int si0_vmcu_accept_shutdown(void)
{
    int status;
    if (ap_power_state != SI0_AP_ARMED)
        return ap_power_state == SI0_AP_RUN ? FWK_E_STATE : FWK_E_ACCESS;
    status = fwk_interrupt_disable(si0_platform_ctx.config->ap_watchdog_irq);
    if (status == FWK_SUCCESS)
        status = control_start(10000000);
    if (status == FWK_SUCCESS)
        status = power_off_all_cores();
    if (status != FWK_SUCCESS) {
        control_fail(status);
        return status;
    }
    control.stopping_ap = true;
    ap_power_state = SI0_AP_QUIESCING;
    FWK_LOG_INFO(MOD_NAME "AP shutdown accepted from PSCI; retaining SYSTOP/SI/RSE "
        "g=%u p=%u s=%u", control.generation,
        ap_power_state, control.stopping_ap);
    return FWK_SUCCESS;
}

static int wake_ap(void)
{
    if (ap_power_state != SI0_AP_OFF)
        return FWK_E_STATE;
    return request_recovery();
}

static const struct mod_si0_vmcu_api vmcu_control_api = {
    .recover = request_recovery,
    .recovery_status = recovery_status,
    .power_arm = power_arm,
    .power_status = power_status,
    .wake = wake_ap,
};

static int control_update(unsigned int generation)
{
    uint64_t now;
    int status;
    if (!control.active || generation != control.generation)
        return FWK_SUCCESS;
    status = si0_platform_ctx.rearm_timer_api->get_counter(
        si0_platform_ctx.config->timer_id, &now);
    if (control.log_first_poll && status == FWK_SUCCESS) {
        control.log_first_poll = false;
        FWK_LOG_INFO(MOD_NAME "CTRL poll g=%u p=%u r=%u s=%u t=%" PRIu64 " d=%" PRIu64,
            generation, ap_power_state, recovery_state, control.stopping_ap,
            now, control.deadline);
        FWK_LOG_INFO(MOD_NAME "CTRL callback g=%u t=%" PRIu64,
            generation, control.callback_time);
    }
    if (status != FWK_SUCCESS || now >= control.deadline) {
        control_fail(status == FWK_SUCCESS ? FWK_E_TIMEOUT : status);
        return FWK_SUCCESS;
    }
    if (ap_power_state == SI0_AP_ARMED)
        return FWK_SUCCESS;
    if (!control.stopping_ap && recovery_state != SI0_RECOVERY_PENDING)
        return FWK_SUCCESS;
    status = check_power_off_all_cores();
    if (status == FWK_PENDING)
        return FWK_SUCCESS;
    if (status != FWK_SUCCESS) {
        control_fail(status);
        return FWK_SUCCESS;
    }
    if (control.stopping_ap) {
        control.stopping_ap = false;
        control.active = false;
        control_stop_alarm();
        ap_power_state = SI0_AP_OFF;
        recovery_state = SI0_RECOVERY_OFF;
        FWK_LOG_INFO(MOD_NAME "AP cores OFF verified; IST_DONE_N may assert "
            "gen=%u now=%" PRIu64, generation, now);
        return FWK_SUCCESS;
    }
    recovery_state = SI0_RECOVERY_RELOAD;
    status = start_rse_recovery();
    if (status != FWK_SUCCESS)
        control_fail(status);
    return FWK_SUCCESS;
}

static int control_poll(unsigned int generation)
{
    int status = control_update(generation);
    /* Schedule from dispatch completion, never replay elapsed polling ticks. */
    if (control.active && generation == control.generation) {
        int alarm_status = control_schedule();
        if (alarm_status != FWK_SUCCESS)
            control_fail(alarm_status);
    }
    return status;
}

static void ap_watchdog_isr(void)
{
    struct fwk_event_light event = {
        .id = FWK_ID_EVENT_INIT(
            FWK_MODULE_IDX_SI0_PLATFORM, MOD_SI0_PLATFORM_AP_WATCHDOG),
        /* ISR events cannot inherit the framework's current event source. */
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
        .target_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
    };
    int status;

    /* WS1 is level-triggered. Keep it masked until CPU0 power-on has reset
     * the AP watchdog; never perform the RSE handshake in interrupt context. */
    status = fwk_interrupt_disable(si0_platform_ctx.config->ap_watchdog_irq);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(MOD_NAME "AP watchdog IRQ mask failed: %d", status);
        fwk_unexpected();
        return;
    }
    if (ap_watchdog_recovery)
        return;
    ap_watchdog_recovery = true;
    recovery_state = SI0_RECOVERY_PENDING;
    ap_power_state = SI0_AP_WAKING;
    status = fwk_put_event(&event);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(MOD_NAME "AP watchdog recovery enqueue failed: %d", status);
        fwk_unexpected();
    }
}

static int ap_watchdog_rearm(void)
{
    bool pending;
    bool enabled;
    int status;
    unsigned int irq = si0_platform_ctx.config->ap_watchdog_irq;

    status = fwk_interrupt_clear_pending(irq);
    if (status != FWK_SUCCESS)
        return status;
    status = fwk_interrupt_is_pending(irq, &pending);
    if (status != FWK_SUCCESS)
        return status;
    rearm_ctx.last_pending = pending;
    if (pending) {
        return FWK_PENDING;
    }
    /* Unmask may immediately enter the ISR. Publish readiness first and do
     * not overwrite a new recovery claimed by that ISR on return. */
    ap_watchdog_recovery = false;
    status = fwk_interrupt_enable(irq);
    if (status != FWK_SUCCESS) {
        ap_watchdog_recovery = true;
        return status;
    }
    status = fwk_interrupt_is_enabled(irq, &enabled);
    if (status != FWK_SUCCESS)
        goto readback_failed;
    status = fwk_interrupt_is_pending(irq, &pending);
    if (status != FWK_SUCCESS)
        goto readback_failed;
    rearm_ctx.last_pending = pending;
    /* Diagnostic snapshot only: an IRQ can arrive between these reads.
     * Never change recovery state based on a transient register snapshot. */
    FWK_LOG_INFO(MOD_NAME "AP watchdog IRQ %u snapshot: enabled=%u pending=%u recovery=%u",
        irq, enabled, pending, ap_watchdog_recovery);
    if (!ap_watchdog_recovery && enabled && !pending) {
        FWK_LOG_INFO(MOD_NAME "AP watchdog recovery: boot CPU on, IRQ rearmed");
    } else if (ap_watchdog_recovery) {
        FWK_LOG_INFO(MOD_NAME "AP watchdog IRQ retriggered during rearm");
    }
    return status;
readback_failed:
    ap_watchdog_recovery = true;
    {
        int mask_status = fwk_interrupt_disable(irq);
        if (mask_status != FWK_SUCCESS) {
            FWK_LOG_ERR(MOD_NAME "Watchdog rearm readback failed; IRQ mask failed: %d",
                mask_status);
            return mask_status;
        }
    }
    return status;
}

static void rearm_alarm_callback(uintptr_t generation)
{
    struct fwk_event event = {
        .id = FWK_ID_EVENT_INIT(FWK_MODULE_IDX_SI0_PLATFORM,
            MOD_SI0_PLATFORM_WATCHDOG_REARM),
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
        .target_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
    };
    int status;

    if (!rearm_ctx.pending || generation != rearm_ctx.generation)
        return;
    *(unsigned int *)event.params = generation;
    status = fwk_put_event(&event);
    if (status != FWK_SUCCESS) {
        rearm_ctx.pending = false;
        FWK_LOG_ERR(MOD_NAME "Watchdog rearm enqueue failed: %d", status);
    }
}

static int watchdog_rearm_poll(unsigned int generation)
{
    const struct mod_si0_platform_config *config = si0_platform_ctx.config;
    uint64_t now;
    bool before;
    int status;

    if (!rearm_ctx.pending || generation != rearm_ctx.generation)
        return FWK_SUCCESS;
    if (!recovery_completion_allowed(SI0_RECOVERY_BOOT))
        return FWK_SUCCESS;
    status = si0_platform_ctx.rearm_timer_api->get_counter(config->timer_id, &now);
    if (status != FWK_SUCCESS)
        goto fail;
    if (now >= rearm_ctx.deadline) {
        status = FWK_E_TIMEOUT;
        goto fail;
    }
    status = fwk_interrupt_is_pending(config->ap_watchdog_irq, &before);
    if (status != FWK_SUCCESS)
        goto fail;
    rearm_ctx.attempts++;
    status = ap_watchdog_rearm();
    FWK_LOG_INFO(MOD_NAME "Watchdog rearm attempt=%u before=%u after=%u status=%d",
        rearm_ctx.attempts, before, rearm_ctx.last_pending, status);
    if (status == FWK_SUCCESS) {
        rearm_ctx.pending = false;
        /* Unmasking may have queued a new WS1 recovery in the ISR. */
        if (ap_watchdog_recovery)
            return FWK_SUCCESS;
        recovery_state = SI0_RECOVERY_COMPLETE;
        ap_power_state = SI0_AP_RUN;
        if (control.active) {
            control.active = false;
            control_stop_alarm();
        }
        return FWK_SUCCESS;
    }
    if (status != FWK_PENDING)
        goto fail;
    status = si0_platform_ctx.rearm_alarm_api->start(
        config->watchdog_rearm_alarm_id, 10000, MOD_TIMER_ALARM_TYPE_ONCE,
        rearm_alarm_callback, rearm_ctx.generation);
    if (status == FWK_SUCCESS)
        return FWK_SUCCESS;
fail:
    control_fail(status);
    FWK_LOG_ERR(MOD_NAME "Watchdog rearm FAILED after %u attempts: %d",
        rearm_ctx.attempts, status);
    return status;
}

static int start_watchdog_rearm(void)
{
    const struct mod_si0_platform_config *config = si0_platform_ctx.config;
    uint64_t now, duration;
    int status;

    if (!recovery_completion_allowed(SI0_RECOVERY_BOOT))
        return FWK_SUCCESS;
    if (rearm_ctx.pending)
        return FWK_E_BUSY;
    status = si0_platform_ctx.rearm_timer_api->get_counter(config->timer_id, &now);
    if (status != FWK_SUCCESS)
        return status;
    status = si0_platform_ctx.rearm_timer_api->time_to_timestamp(
        config->timer_id, config->watchdog_rearm_timeout_us, &duration);
    if (status != FWK_SUCCESS)
        return status;
    rearm_ctx.deadline = now + duration;
    rearm_ctx.generation++;
    rearm_ctx.attempts = 0;
    rearm_ctx.pending = true;
    return watchdog_rearm_poll(rearm_ctx.generation);
}

/*
 * Helper function to check if a cpu is in isolated CPU MPID list.
 */
static bool is_cpu_isolated(
    const struct mod_si0_platform_config *config,
    uint64_t cpu_mpid)
{
    uint64_t isolated_cpu_count;
    uint64_t *isolated_cpu_mpid_list;

    isolated_cpu_count = config->isolated_cpu_info.isolated_cpu_count;
    isolated_cpu_mpid_list = config->isolated_cpu_info.isolated_cpu_mpid_list;

    while (isolated_cpu_count != 0) {
        if (isolated_cpu_mpid_list[isolated_cpu_count - 1] == cpu_mpid) {
            return true;
        }
        isolated_cpu_count--;
    }

    return false;
}

/*
 * Helper function to validate the configuration data received during init.
 */
static int validate_config_data(const struct mod_si0_platform_config *config)
{
    if (config->pmic_rail_count != 0) {
#ifdef BUILD_HAS_MOD_PMIC
        if (!fwk_id_is_type(config->pmic_id, FWK_ID_TYPE_ELEMENT) ||
            (fwk_id_get_module_idx(config->pmic_id) != FWK_MODULE_IDX_PMIC))
            return FWK_E_PARAM;
#else
        return FWK_E_SUPPORT;
#endif
    }

    if (is_cpu_isolated(config, config->primary_cpu_mpid)) {
        FWK_LOG_ERR("[SI0 PLATFORM] Found primary CPU in isolated CPU list");
        return FWK_E_PARAM;
    }

    return FWK_SUCCESS;
}

/*
 * Framework handlers
 */
static int si0_platform_mod_init(
    fwk_id_t module_id,
    unsigned int unused,
    const void *data)
{
    int status;

    const struct mod_si0_platform_config *config;

    config = (const struct mod_si0_platform_config *)data;

    if (config == NULL) {
        FWK_LOG_ERR("[SI0 PLATFORM] NULL config in mod_init");
        return FWK_E_PARAM;
    }

    if (!fwk_id_type_is_valid(config->timer_id) ||
        !fwk_id_type_is_valid(config->transport_id)) {
        return FWK_E_DATA;
    }

    status = validate_config_data(config);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR("[SI0 PLATFORM] Configuration data is invalid");
        return status;
    }

    si0_platform_ctx.config = config;

    return FWK_SUCCESS;
}

static int update_sds_reset_syndrome(uint32_t reset_syndrome)
{
    int status;
    const struct mod_sds_structure_desc *sds_structure_desc =
        fwk_module_get_data(sds_reset_syndrome_id);

    if (sds_structure_desc == NULL) {
        return FWK_E_DATA;
    }

    status = si0_platform_ctx.sds_api->struct_write(
        sds_structure_desc->id, 0, &reset_syndrome, sizeof(reset_syndrome));
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(
            "[SI0 PLATFORM] SDS reset syndrome write failed, status=%d",
            status);
        return status;
    }

    FWK_LOG_INFO("[SI0 PLATFORM] SDS reset syndrome updated successfully");

    return FWK_SUCCESS;
}

static int si0_platform_bind(fwk_id_t id, unsigned int round)
{
    int status;

    if (round > 0) {
        return FWK_SUCCESS;
    }

#ifdef BUILD_HAS_MOD_PMIC
    if (si0_platform_ctx.config->pmic_rail_count != 0) {
        status = fwk_module_bind(
            si0_platform_ctx.config->pmic_id,
            FWK_ID_API(FWK_MODULE_IDX_PMIC, MOD_PMIC_API_IDX_PMIC),
            &si0_platform_ctx.pmic_api);
        if (status != FWK_SUCCESS)
            return status;
    }
#endif

    /* Bind to modules required for handshaking with RSE */
    if (si0_platform_ctx.config->ap_watchdog_irq != 0) {
        status = fwk_module_bind(si0_platform_ctx.config->timer_id,
            FWK_ID_API(FWK_MODULE_IDX_TIMER, MOD_TIMER_API_IDX_TIMER),
            &si0_platform_ctx.rearm_timer_api);
        if (status != FWK_SUCCESS)
            return status;
        status = fwk_module_bind(si0_platform_ctx.config->control_alarm_id,
            MOD_TIMER_API_ID_ALARM, &control.alarm);
        if (status != FWK_SUCCESS)
            return status;
        status = fwk_module_bind(si0_platform_ctx.config->watchdog_rearm_alarm_id,
            MOD_TIMER_API_ID_ALARM, &si0_platform_ctx.rearm_alarm_api);
        if (status != FWK_SUCCESS)
            return status;
        status = fwk_module_bind(
            FWK_ID_MODULE(FWK_MODULE_IDX_PFDI_MONITOR),
            FWK_ID_API(FWK_MODULE_IDX_PFDI_MONITOR, MOD_PFDI_MONITOR_API_IDX_RESTART),
            &si0_platform_ctx.pfdi_restart_api);
        if (status != FWK_SUCCESS)
            return status;
        status = fwk_module_bind(
            FWK_ID_MODULE(FWK_MODULE_IDX_APCONTEXT),
            FWK_ID_API(FWK_MODULE_IDX_APCONTEXT, MOD_APCONTEXT_API_IDX_RESET),
            &si0_platform_ctx.apcontext_api);
        if (status != FWK_SUCCESS)
            return status;
    }
    status = platform_rse_bind(si0_platform_ctx.config);
    if (status != FWK_SUCCESS) {
        return status;
    }

    /* Bind to modules required for power management */
    status = platform_power_mgmt_bind();
    if (status != FWK_SUCCESS) {
        return status;
    }
    status = fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_POWER_DOMAIN),
        FWK_ID_API(FWK_MODULE_IDX_POWER_DOMAIN, MOD_PD_API_IDX_RESTRICTED),
        &si0_platform_ctx.mod_pd_restricted_api);
    if (status != FWK_SUCCESS) {
        return status;
    }

    status = fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_PPU_V1),
        FWK_ID_API(FWK_MODULE_IDX_PPU_V1, MOD_PPU_V1_API_IDX_ISR),
        &si0_platform_ctx.ppu_v1_isr_api);
    if (status != FWK_SUCCESS) {
        return status;
    }

    status = fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_SDS),
        FWK_ID_API(FWK_MODULE_IDX_SDS, 0),
        &si0_platform_ctx.sds_api);
    if (status != FWK_SUCCESS) {
        return status;
    }

    return status;
}

static int si0_platform_process_bind_request(
    fwk_id_t requester_id,
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void **api)
{
    int status;
    enum mod_si0_platform_api_idx api_id_type;

    api_id_type = (enum mod_si0_platform_api_idx)fwk_id_get_api_idx(api_id);

    switch (api_id_type) {
    case MOD_SI0_PLATFORM_API_IDX_VMCU_CONTROL:
#ifdef BUILD_HAS_MOD_VMCU_SAFETY
        if (!fwk_id_is_equal(requester_id,
                FWK_ID_MODULE(FWK_MODULE_IDX_VMCU_SAFETY)))
            return FWK_E_ACCESS;
#else
        return FWK_E_SUPPORT;
#endif
        *api = &vmcu_control_api;
        return FWK_SUCCESS;
    case MOD_SI0_PLATFORM_API_IDX_SCMI_POWER_DOWN:
        *api = get_platform_scmi_power_down_api();
        status = FWK_SUCCESS;
        break;

    case MOD_SI0_PLATFORM_API_IDX_SYSTEM_POWER_DRIVER:
        *api = get_platform_system_power_driver_api();
        status = FWK_SUCCESS;
        break;

    case MOD_SCP_PLATFORM_API_IDX_TRANSPORT_SIGNAL:
        *api = get_rse_platform_transport_signal_api();
        status = FWK_SUCCESS;
        break;

    default:
        status = FWK_E_PARAM;
    }

    return status;
}

#ifdef BUILD_HAS_MOD_PMIC
static int check_pmic_rails(void)
{
    const struct mod_si0_platform_config *config = si0_platform_ctx.config;
    unsigned int rail;
    uint32_t uv;
    bool enabled;
    int status;

    for (rail = 0; rail < config->pmic_rail_count; rail++) {
        status = si0_platform_ctx.pmic_api->get_enabled(
            config->pmic_id, rail, &enabled);
        if (status != FWK_SUCCESS)
            goto error;
        status = si0_platform_ctx.pmic_api->get_voltage(
            config->pmic_id, rail, &uv);
        if (status != FWK_SUCCESS)
            goto error;

        FWK_LOG_INFO(
            "[SI0 PLATFORM] PMIC rail=%u enabled=%u programmed_uv=%u",
            rail,
            (unsigned int)enabled,
            (unsigned int)uv);
    }

    return FWK_SUCCESS;

error:
    FWK_LOG_ERR(
        "[SI0 PLATFORM] PMIC rail=%u read failed: %d", rail, status);
    return status;
}
#endif

static int si0_platform_start(fwk_id_t id)
{
    int status;
    struct fwk_event event = { 0 };
    unsigned int event_count = 0U;

#ifdef BUILD_HAS_MOD_PMIC
    status = check_pmic_rails();
    if (status != FWK_SUCCESS)
        return status;
#endif

#ifdef BUILD_HAS_NOTIFICATION
    fwk_id_t pd_transition_source_id =
        fwk_id_build_element_id(fwk_module_id_power_domain, 0);

    status = fwk_notification_subscribe(
        pd_transition_notification_id, pd_transition_source_id, id);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(
            MOD_NAME "Failed to subscribe to Power Domain notification for %s",
            fwk_module_get_element_name(pd_transition_source_id));
    } else {
        FWK_LOG_DEBUG(
            MOD_NAME "Subscribed to Power Domain notifications for %s",
            fwk_module_get_element_name(pd_transition_source_id));
    }
#endif /* BUILD_HAS_NOTIFICATION */

    /* SI0 subsystem initialization completion notification */
    event.id = mod_si0_platform_notification_subsys_init;
    event.source_id = id;

    status = fwk_notification_notify(&event, &event_count);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(MOD_NAME "Error! Subsystem init notification failed");
        return FWK_E_PANIC;
    }

#ifdef BUILD_HAS_NOTIFICATION
    /* Subscribe to warm reset notifications */
    status = fwk_notification_subscribe(
        mod_pd_notification_id_pre_warmreset,
        FWK_ID_MODULE(FWK_MODULE_IDX_POWER_DOMAIN),
        id);
    if (status != FWK_SUCCESS) {
        FWK_LOG_WARN(
            "[SI0 PLATFORM] failed to subscribe to warm reset "
            "notification\n");
    }
#endif

    FWK_LOG_INFO(MOD_NAME "SCP started");

    if (si0_platform_ctx.config->ap_watchdog_irq != 0U) {
        status = fwk_interrupt_set_isr(
            si0_platform_ctx.config->ap_watchdog_irq, ap_watchdog_isr);
        if (status != FWK_SUCCESS)
            return status;
        status = fwk_interrupt_enable(si0_platform_ctx.config->ap_watchdog_irq);
    }

    return status;
}

static int power_off_all_cores(void)
{
    unsigned int pd_idx;
    unsigned int core_count;
    int status;
    struct mod_pd_restricted_api *mod_pd_restricted_api =
        si0_platform_ctx.mod_pd_restricted_api;

    core_count = platform_get_core_count();

    for (pd_idx = 0; pd_idx < core_count; pd_idx++) {
        FWK_LOG_INFO(
            "[SI0 PLATFORM] Powering down %s\n",
            fwk_module_get_element_name(
                FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, pd_idx)));

        status = mod_pd_restricted_api->set_state(
            FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, pd_idx),
            false,
            MOD_PD_COMPOSITE_STATE(MOD_PD_LEVEL_0, 0, 0, 0, MOD_PD_STATE_OFF));

        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR(
                "[SI0 PLATFORM] Power down of %s failed\n",
                fwk_module_get_element_name(
                    FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, pd_idx)));
        }
        if (status != FWK_SUCCESS)
            return status;
    }
    return FWK_SUCCESS;
}

static int check_power_off_all_cores(void)
{
    unsigned int core_count;
    unsigned int power_state;
    unsigned int pd_idx;
    int status = 0;
    struct mod_pd_restricted_api *mod_pd_restricted_api =
        si0_platform_ctx.mod_pd_restricted_api;

    core_count = platform_get_core_count();

    /* Check if all the CPU power domain are powered down */
    for (pd_idx = 0; pd_idx < core_count; pd_idx++) {
        status = mod_pd_restricted_api->get_state(
            FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, pd_idx), &power_state);
        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR(
                "[SI0 PLATFORM] failed to get state of %s",
                fwk_module_get_element_name(
                    FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, pd_idx)));
            return status;
        }

        /* Exit if any core is not powered down */
        if ((power_state &
             (MOD_PD_CS_STATE_MASK << MOD_PD_CS_LEVEL_0_STATE_SHIFT)) !=
            MOD_PD_STATE_OFF) {
            FWK_LOG_INFO(
                "[SI0 PLATFORM] %s not yet powered down",
                fwk_module_get_element_name(
                    FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, pd_idx)));
            return FWK_PENDING;
        }
    }
    return status;
}

static bool scmi_service_is_ap_facing(const struct mod_scmi_service_config *cfg)
{
    /* Reset only services whose remote agent is on the AP side */
    for (size_t i = 0; i < SI0_AP_FACING_SCMI_AGENT_COUNT; i++) {
        if (cfg->scmi_agent_id == si0_ap_facing_scmi_agents[i]) {
            return true;
        }
    }
    return false;
}

static void reset_scmi_service_mailbox(
    const struct mod_scmi_service_config *svc_cfg)
{
    const struct mod_transport_channel_config *chan_cfg =
        fwk_module_get_data(svc_cfg->transport_id);
    if (chan_cfg == NULL) {
        FWK_LOG_ERR("[SI0_PLATFORM] transport channel cfg is NULL");
        return;
    }

    if (chan_cfg->out_band_mailbox_address == 0U) {
        /* Not an out-of-band mailbox service; nothing to reset */
        return;
    }

    struct mod_transport_buffer *mbx =
        (struct mod_transport_buffer *)chan_cfg->out_band_mailbox_address;

    /*
     * AP is rebooting; force mailbox state to FREE so AP won't observe BUSY
     * on boot.
     */
    mbx->reserved0 = 0;
    mbx->status = SCMI_SHMEM_CHAN_STAT_FREE;
    mbx->reserved1 = 0;
    mbx->flags = 0;
    mbx->length = 0;
    mbx->message_header = 0;
}

static void reset_scmi_mailboxes(void)
{
    size_t scmi_service_count;
    fwk_id_t scmi_module_id = FWK_ID_MODULE(FWK_MODULE_IDX_SCMI);

    fwk_module_get_element_count(scmi_module_id, &scmi_service_count);

    for (unsigned int i = 0; i < scmi_service_count; i++) {
        fwk_id_t scmi_eid = FWK_ID_ELEMENT(FWK_MODULE_IDX_SCMI, i);
        const struct mod_scmi_service_config *svc_cfg =
            fwk_module_get_data(scmi_eid);
        if (svc_cfg == NULL) {
            continue;
        }

        if (!scmi_service_is_ap_facing(svc_cfg)) {
            continue;
        }
        reset_scmi_service_mailbox(svc_cfg);
    }

    FWK_LOG_INFO(
        "[SI0_PLATFORM] AP-facing SCMI mailboxes reset for warm reboot");
}

static void boot_primary_core(void)
{
    int status;
    struct mod_pd_restricted_api *mod_pd_restricted_api =
        si0_platform_ctx.mod_pd_restricted_api;

    FWK_LOG_INFO(
        "[SI0 PLATFORM] Warm reset complete. Powering up "
        "boot cpu...");

    status = mod_pd_restricted_api->set_state(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, PD_STATIC_DEV_IDX_SYSTOP),
        false,
        MOD_PD_COMPOSITE_STATE(
            MOD_PD_LEVEL_2,
            MOD_PD_STATE_ON,
            MOD_PD_STATE_ON,
            MOD_PD_STATE_ON,
            MOD_PD_STATE_ON));

    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR("[SI0 PLATFORM] Failed to power up boot cpu");
        fwk_assert(status == FWK_SUCCESS);
    }
}

static int finish_warm_reset(void)
{
    int status;

    if (ap_watchdog_recovery) {
        /* PFDI configuration is truncated to configured AP cores; the PD
         * topology may retain all 16 physical core slots. */
        if (si0_platform_ctx.config->ap_pfdi_core_count == 0 ||
            si0_platform_ctx.config->ap_pfdi_core_count > platform_get_core_count())
            return FWK_E_DATA;
        for (unsigned int i = 0; i < si0_platform_ctx.config->ap_pfdi_core_count; i++) {
            status = si0_platform_ctx.pfdi_restart_api->prepare(
                FWK_ID_ELEMENT(FWK_MODULE_IDX_POWER_DOMAIN, i));
            if (status != FWK_SUCCESS)
                return status;
        }
        /* RSE reloads BL2 only. Reinitialize the SCP-owned AP context,
         * including the trusted mailbox, before releasing the boot CPU. */
        status = si0_platform_ctx.apcontext_api->reset();
        if (status != FWK_SUCCESS)
            return status;
    }
    reset_scmi_mailboxes();
    status = update_sds_reset_syndrome(SI0_WARM_RESET_SYNDROME_VALUE);
    if (status != FWK_SUCCESS)
        return status;
    recovery_state = SI0_RECOVERY_BOOT;
    boot_primary_core();
    return FWK_SUCCESS;
}

static int si0_platform_process_event(
    const struct fwk_event *event,
    struct fwk_event *resp)
{
    int status;

    /* Event for checking power domain status */
    struct fwk_event_light check_pd_off_event = {
        .id = mod_si0_platform_event_check_ppu_off,
        .target_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
    };

    switch (fwk_id_get_event_idx(event->id)) {
    case MOD_SI0_PLATFORM_CONTROL_POLL:
        return control_poll(*(const unsigned int *)event->params);
    case MOD_SI0_PLATFORM_RSE_RECOVERY_POLL:
        poll_rse_recovery(*(const unsigned int *)event->params);
        return FWK_SUCCESS;
    case MOD_SI0_PLATFORM_WATCHDOG_REARM:
        status = watchdog_rearm_poll(*(const unsigned int *)event->params);
        break;
    case MOD_SI0_PLATFORM_RSE_RECOVERY_DONE:
        status = complete_rse_recovery(event);
        if (!recovery_completion_allowed(SI0_RECOVERY_RELOAD))
            return FWK_SUCCESS;
        if (status != FWK_SUCCESS) {
            /* Fail closed: AP remains off and WS1 stays masked. */
            control_fail(status);
            FWK_LOG_ERR(MOD_NAME "RSE recovery not completed: %d", status);
            return FWK_SUCCESS;
        }
        status = finish_warm_reset();
        if (status != FWK_SUCCESS)
            control_fail(status);
        break;
    case MOD_SI0_PLATFORM_AP_WATCHDOG:
        FWK_LOG_INFO(MOD_NAME "AP watchdog WS1: requesting coordinated warm reset");
        status = si0_platform_ctx.mod_pd_restricted_api->system_shutdown(
            MOD_PD_SYSTEM_WARM_RESET);
        if (status == FWK_PENDING)
            status = FWK_SUCCESS;
        else {
            control_fail(status);
            FWK_LOG_ERR(MOD_NAME "AP watchdog recovery request failed: %d", status);
        }
        break;
    case MOD_SI0_PLATFORM_CHECK_PD_OFF:
        if (ap_watchdog_recovery) {
            status = control_poll(control.generation);
            break;
        }
        status = check_power_off_all_cores();
        if (status != FWK_SUCCESS) {
            /*
             * Increment the retry count. The count is initialized in the warm
             * reset notification handler
             */
            si0_platform_ctx.warm_reset_check_cnt++;
            if (si0_platform_ctx.warm_reset_check_cnt >=
                WARM_RESET_MAX_RETRIES) {
                FWK_LOG_ERR(
                    "[SI0 PLATFORM] warm reset retries reached "
                    "maximum attempts and failed!");
                fwk_assert(
                    si0_platform_ctx.warm_reset_check_cnt <
                    WARM_RESET_MAX_RETRIES);
            }

            /*
             * Monitor core PPU states until all the core power domains
             * are powered down.
             */
            status = fwk_put_event(&check_pd_off_event);
            if (status != FWK_SUCCESS) {
                FWK_LOG_ERR(
                    "[SI0 PLATFORM] Failed to send event, returned %d", status);
            }
            fwk_assert(status == FWK_SUCCESS);
        } else {
            if (ap_watchdog_recovery) {
                status = start_rse_recovery();
                break;
            }
            /* Handshake with RSE via dedicated channel (DBCH[2]/FLAG 3) */
            status = notify_rse_and_wait_for_response(false);
            if (status != FWK_SUCCESS) {
                FWK_LOG_ERR(MOD_NAME "Error! SCP-RSE handshake failed");
                return FWK_E_PANIC;
            }

            status = finish_warm_reset();
        }
        break; /* MOD_SI0_PLATFORM_CHECK_PD_OFF */
    default:
        FWK_LOG_WARN(
            "[SI0 PLATFORM] unrecognized event received, event ignored");
        status = FWK_E_PARAM;
    }

    return status;
}

#ifdef BUILD_HAS_NOTIFICATION
int si0_platform_process_notification(
    const struct fwk_event *event,
    struct fwk_event *resp_event)
{
    int status;
    struct mod_pd_power_state_transition_notification_params *params;

    /* Event for checking power domain status */
    struct fwk_event_light check_pd_off_event = {
        .id = mod_si0_platform_event_check_ppu_off,
        .target_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
    };

    fwk_assert(fwk_id_is_type(event->target_id, FWK_ID_TYPE_MODULE));
    if (fwk_id_is_equal(event->id, mod_pd_notification_id_pre_warmreset)) {
        /* Requesting power off of all cores */
        if (ap_watchdog_recovery) {
            status = control_start(30000000);
            if (status != FWK_SUCCESS) {
                control_fail(status);
                return FWK_SUCCESS;
            }
            recovery_state = SI0_RECOVERY_PENDING;
            ap_power_state = SI0_AP_WAKING;
        }
        status = power_off_all_cores();
        if (status != FWK_SUCCESS) {
            control_fail(status);
            return FWK_SUCCESS;
        }

        si0_platform_ctx.warm_reset_check_cnt = 0;

        /* Raising an internal event for to check whether all cores are powered
         * down.
         */
        status = fwk_put_event(&check_pd_off_event);
        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR(
                "[SI0 PLATFORM] Failed to send PD power off check "
                "event, returned %d",
                status);
            fwk_assert(status == FWK_SUCCESS);
        }
    } else if (fwk_id_is_equal(event->id, pd_transition_notification_id)) {
        params = (struct mod_pd_power_state_transition_notification_params *)
                     event->params;
        status = pd_transition_ap_platform_hook(params->state);
        if (status == FWK_SUCCESS && params->state == MOD_PD_STATE_ON &&
            ap_watchdog_recovery) {
            /* CPU0 OFF/ON includes AP peripheral reset and clears WS1. */
            status = start_watchdog_rearm();
        }
        return status;
    } else {
        return FWK_E_PARAM;
    }

    return FWK_SUCCESS;
}
#endif /* BUILD_HAS_NOTIFICATION */

const struct fwk_module module_si0_platform = {
    .type = FWK_MODULE_TYPE_DRIVER,
    .api_count = MOD_SI0_PLATFORM_API_COUNT,
    .event_count = (unsigned int)MOD_SI0_PLATFORM_EVENT_COUNT,
    .init = si0_platform_mod_init,
    .bind = si0_platform_bind,
    .process_bind_request = si0_platform_process_bind_request,
    .process_event = si0_platform_process_event,
#ifdef BUILD_HAS_NOTIFICATION
    .notification_count = MOD_SI0_PLATFORM_NOTIFICATION_COUNT,
    .process_notification = si0_platform_process_notification,
#endif
    .start = si0_platform_start,
};
