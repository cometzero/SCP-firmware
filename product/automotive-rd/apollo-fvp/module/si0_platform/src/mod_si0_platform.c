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
#include <mod_ppu_v1.h>
#include <mod_scmi.h>
#include <mod_sds.h>
#include <mod_si0_platform.h>
#include <mod_transport.h>

#include <fwk_core.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_notification.h>
#include <fwk_status.h>

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

    /* Config containig data required for platform initialization */
    const struct mod_si0_platform_config *config;

    /* Count of number of warm reset completion check iterations */
    unsigned int warm_reset_check_cnt;
};
static struct si0_platform_ctx si0_platform_ctx;

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

    /* Bind to modules required for handshaking with RSE */
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

static int si0_platform_start(fwk_id_t id)
{
    int status;
    struct fwk_event event = { 0 };
    unsigned int event_count = 0U;

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

    return status;
}

static void power_off_all_cores(void)
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
        fwk_assert(status == FWK_SUCCESS);
    }
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
    case MOD_SI0_PLATFORM_CHECK_PD_OFF:
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
            /* Handshake with RSE via dedicated channel (DBCH[2]/FLAG 3) */
            status = notify_rse_and_wait_for_response();
            if (status != FWK_SUCCESS) {
                FWK_LOG_ERR(MOD_NAME "Error! SCP-RSE handshake failed");
                return FWK_E_PANIC;
            }

            reset_scmi_mailboxes();

            status = update_sds_reset_syndrome(SI0_WARM_RESET_SYNDROME_VALUE);
            if (status != FWK_SUCCESS) {
                FWK_LOG_ERR(
                    "[SI0 PLATFORM] Failed to update SDS reset syndrome, "
                    "returned %d",
                    status);
                return status;
            }

            /*
             * All the CPU power domain are powered off. Start the process to
             * power on the first application core to complete the AP reboot
             * sequence.
             */
            boot_primary_core();
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
        power_off_all_cores();

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
        return (pd_transition_ap_platform_hook(params->state));
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
