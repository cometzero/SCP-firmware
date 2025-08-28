/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     SCP platform sub-system initialization support.
 */

#include "internal/si0_platform.h"
#include "platform_core.h"
#include "si0_cfgd_power_domain.h"

#include <mod_power_domain.h>
#include <mod_ppu_v1.h>
#include <mod_si0_platform.h>

#include <fwk_core.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_notification.h>
#include <fwk_status.h>

/* Module context */
struct si0_platform_ctx {
    /* Pointer to the Interrupt Service Routine API of the PPU_V1 module */
    const struct ppu_v1_isr_api *ppu_v1_isr_api;

    /* Power domain module restricted API pointer */
    struct mod_pd_restricted_api *mod_pd_restricted_api;

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
    status = validate_config_data(config);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR("[SI0 PLATFORM] Configuration data is invalid");
        return status;
    }

    si0_platform_ctx.config = config;

    return FWK_SUCCESS;
}

static int si0_platform_bind(fwk_id_t id, unsigned int round)
{
    int status;

    if (round > 0) {
        return FWK_SUCCESS;
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
        mod_pd_notification_id_pre_warm_reset,
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

    /* Event for checking power domain status */
    struct fwk_event_light check_pd_off_event = {
        .id = mod_si0_platform_event_check_ppu_off,
        .target_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_SI0_PLATFORM),
    };

    fwk_assert(fwk_id_is_type(event->target_id, FWK_ID_TYPE_MODULE));
    if (fwk_id_is_equal(event->id, mod_pd_notification_id_pre_warm_reset)) {
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
    }

    return FWK_SUCCESS;
}
#endif

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
