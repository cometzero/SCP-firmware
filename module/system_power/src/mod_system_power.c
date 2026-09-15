/*
 * Arm SCP/MCP Software
 * Copyright (c) 2015-2024, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     System Power Support.
 */

#include <mod_power_domain.h>
#include <mod_system_power.h>
#ifdef BUILD_HAS_MOD_TIMER
#    include <mod_timer.h>
#endif

#include <fwk_assert.h>
#include <fwk_id.h>
#include <fwk_interrupt.h>
#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_status.h>

#include <stdbool.h>
#include <stdint.h>

/* SoC wakeup composite state */
#define MOD_SYSTEM_POWER_SOC_WAKEUP_STATE \
    MOD_PD_COMPOSITE_STATE( \
        (unsigned int)MOD_PD_LEVEL_2, \
        0U, \
        (unsigned int)MOD_PD_STATE_ON, \
        (unsigned int)MOD_PD_STATE_ON, \
        (unsigned int)MOD_PD_STATE_ON)

/* Element context */
struct system_power_dev_ctx {
    /* Element configuration data pointer */
    const struct mod_system_power_dev_config *config;

    /* Power domain driver API pointer */
    const struct mod_pd_driver_api *sys_ppu_api;
};

/* Module context */
struct mod_system_power_ctx {
    /* System power element context table */
    struct system_power_dev_ctx *dev_ctx_table;

    /* Number of elements */
    unsigned int dev_count;

    /* Pointer to array of extended PPU power domain driver APIs */
    const struct mod_pd_driver_api **ext_ppu_apis;

    /* Power domain module restricted API pointer */
    const struct mod_pd_restricted_api *pd_restricted_api;

    /* Power domain module driver input API pointer */
    const struct mod_pd_driver_input_api *pd_driver_input_api;

    /* Driver API pointer */
    const struct mod_system_power_driver_api *driver_api;

    /* Power domain module identifier of the system power domain */
    fwk_id_t mod_pd_system_id;

    /* Current system-level power state */
    unsigned int state;

    /* Requested power state */
    unsigned int requested_state;

    /* Pointer to module config */
    const struct mod_system_power_config *config;

    /*
     * Identifier of the power domain of the last standing core before system
     * suspend.
     */
    fwk_id_t last_core_pd_id;
#ifdef BUILD_HAS_MOD_TIMER
    const struct mod_timer_alarm_api *test_wake_alarm;
#endif
    bool test_wake_armed;
    bool test_retention_pending;
    bool test_retention_off_verified;
};

static struct mod_system_power_ctx system_power_ctx;
static int set_system_power_state(unsigned int state);

#ifdef BUILD_HAS_MOD_TIMER
static void test_suspend_wake_callback(uintptr_t unused)
{
    int status;

    (void)unused;
    if (!system_power_ctx.test_wake_armed)
        return;
    system_power_ctx.test_wake_armed = false;
    FWK_LOG_INFO("[SYS-POW TEST] Timer wake: requesting AP ON");
    status = system_power_ctx.pd_restricted_api->set_state(
        system_power_ctx.last_core_pd_id, false,
        MOD_SYSTEM_POWER_SOC_WAKEUP_STATE);
    if (status != FWK_SUCCESS)
        FWK_LOG_ERR("[SYS-POW TEST] AP wake request failed: %d", status);
}

static int test_suspend_power_off(void)
{
    int status;
    unsigned int i, actual_state;
    struct system_power_dev_ctx *dev_ctx;

    if (system_power_ctx.test_wake_alarm == NULL)
        return FWK_E_SUPPORT;
    status = system_power_ctx.pd_driver_input_api->get_last_core_pd_id(
        &system_power_ctx.last_core_pd_id);
    if (status != FWK_SUCCESS)
        return status;

    if (system_power_ctx.config->test_keep_systop_on) {
        if (system_power_ctx.config->test_children_off_check == NULL)
            return FWK_E_SUPPORT;
        status = system_power_ctx.config->test_children_off_check();
        if (status != FWK_SUCCESS)
            return status;
    }

    if (!system_power_ctx.config->test_keep_systop_on &&
        system_power_ctx.config->test_retention_check != NULL) {
        /* Never fingerprint an inaccessible domain, including on retries. */
        for (i = 0; i < system_power_ctx.dev_count; i++) {
            dev_ctx = &system_power_ctx.dev_ctx_table[i];
            status = dev_ctx->sys_ppu_api->get_state(
                dev_ctx->config->sys_ppu_id, &actual_state);
            if (status != FWK_SUCCESS)
                return status;
            if (actual_state != MOD_PD_STATE_ON)
                return FWK_E_STATE;
        }
        system_power_ctx.test_retention_off_verified = false;
        status = system_power_ctx.config->test_retention_check(true);
        if (status != FWK_SUCCESS)
            return status;
        system_power_ctx.test_retention_pending = true;
    }

    status = set_system_power_state(MOD_SYSTEM_POWER_POWER_STATE_SLEEP0);
    if (status != FWK_SUCCESS)
        return status;

    /* Do not arm wake or claim OFF on a mere accepted power request. */
    for (i = 0; i < system_power_ctx.dev_count; i++) {
        dev_ctx = &system_power_ctx.dev_ctx_table[i];
        status = dev_ctx->sys_ppu_api->get_state(
            dev_ctx->config->sys_ppu_id, &actual_state);
        if (status != FWK_SUCCESS || actual_state !=
            (system_power_ctx.config->test_keep_systop_on ?
                MOD_PD_STATE_ON : MOD_PD_STATE_OFF)) {
            FWK_LOG_ERR("[SYS-POW TEST] SYS%u requested physical state not observed (%d)", i, status);
            return FWK_E_STATE;
        }
    }
    if (system_power_ctx.config->test_keep_systop_on) {
        FWK_LOG_INFO("[SYS-POW TEST] AP cores/clusters OFF; SYSTOP remains ON; arming timer (logical SLEEP0)");
    } else {
        FWK_LOG_INFO("[SYS-POW TEST] AP SYS0 PPU OFF observed; arming timer");
        system_power_ctx.test_retention_off_verified = true;
    }
    system_power_ctx.test_wake_armed = true;
    status = system_power_ctx.test_wake_alarm->start(
        system_power_ctx.config->test_wake_alarm_id,
        system_power_ctx.config->test_wake_delay_us,
        MOD_TIMER_ALARM_TYPE_ONCE, test_suspend_wake_callback, 0);
    if (status != FWK_SUCCESS) {
        system_power_ctx.test_wake_armed = false;
        FWK_LOG_ERR("[SYS-POW TEST] Wake alarm arm failed: %d", status);
    }
    return status;
}
#endif

/*
 * Static helpers
 */

static void ext_ppus_set_state(enum mod_pd_state state)
{
    unsigned int i;
    int status;

    for (i = 0; i < system_power_ctx.config->ext_ppus_count; i++) {
        status = system_power_ctx.ext_ppu_apis[i]->set_state(
            system_power_ctx.config->ext_ppus[i].ppu_id, state);
        if (status != FWK_SUCCESS) {
            FWK_LOG_DEBUG("[SYS-POW] ext-ppu%i %s @%d", i, __func__, __LINE__);
        }
    }
}

static void ext_ppus_shutdown(enum mod_pd_system_shutdown system_shutdown)
{
    unsigned int i;
    const struct mod_pd_driver_api *api;
    fwk_id_t ppu_id;
    int status;

    /* Shutdown external PPUs */
    for (i = 0; i < system_power_ctx.config->ext_ppus_count; i++) {
        api = system_power_ctx.ext_ppu_apis[i];
        ppu_id = system_power_ctx.config->ext_ppus[i].ppu_id;

        if (api->shutdown != NULL) {
            status = api->shutdown(ppu_id, system_shutdown);
        } else {
            status = api->set_state(ppu_id, MOD_PD_STATE_OFF);
        }
        if (status != FWK_SUCCESS) {
            FWK_LOG_DEBUG("[SYS-POW] ext-ppu%i %s @%d", i, __func__, __LINE__);
        }
    }
}

static int set_system_power_state(unsigned int state)
{
    int status;
    unsigned int i;
    struct system_power_dev_ctx *dev_ctx;
    const uint8_t *sys_state_table;

    for (i = 0; i < system_power_ctx.dev_count; i++) {
        dev_ctx = &system_power_ctx.dev_ctx_table[i];

        sys_state_table = dev_ctx->config->sys_state_table;

        status = dev_ctx->sys_ppu_api->set_state(dev_ctx->config->sys_ppu_id,
                                                 sys_state_table[state]);
        if (status != FWK_SUCCESS) {
            return status;
        }
    }

    return FWK_SUCCESS;
}

static int shutdown_system_power_ppus(
    enum mod_pd_system_shutdown system_shutdown)
{
    unsigned int i;
    struct system_power_dev_ctx *dev_ctx;
    const struct mod_pd_driver_api *api;
    fwk_id_t ppu_id;
    unsigned int state;
    int status;

    for (i = 0; i < system_power_ctx.dev_count; i++) {
        dev_ctx = &system_power_ctx.dev_ctx_table[i];

        api = dev_ctx->sys_ppu_api;
        ppu_id = dev_ctx->config->sys_ppu_id;

        if (api->shutdown != NULL) {
            status = api->shutdown(ppu_id, system_shutdown);
        } else {
            state = dev_ctx->config->sys_state_table[MOD_PD_STATE_OFF];

            status = api->set_state(ppu_id, state);
        }
        if (status != FWK_SUCCESS) {
            return status;
        }
    }

    return FWK_SUCCESS;
}

static int disable_all_irqs(void)
{
    int status = FWK_SUCCESS;

    if (system_power_ctx.config->soc_wakeup_irq != FWK_INTERRUPT_NONE) {
        status = fwk_interrupt_disable(system_power_ctx.config->soc_wakeup_irq);
        if (status != FWK_SUCCESS) {
            return FWK_E_DEVICE;
        }
    }

    if (system_power_ctx.driver_api->platform_interrupts != NULL) {
        status = system_power_ctx.driver_api->platform_interrupts(
            MOD_SYSTEM_POWER_PLATFORM_INTERRUPT_CMD_DISABLE);
        if (status != FWK_SUCCESS) {
            status = FWK_E_DEVICE;
        }
    }

    return status;
}

static int shutdown(
    fwk_id_t pd_id,
    enum mod_pd_system_shutdown system_shutdown)
{
    int status;

    status = disable_all_irqs();
    if (status != FWK_SUCCESS) {
        return status;
    }

    /* Shutdown external PPUs */
    ext_ppus_shutdown(system_shutdown);

    system_power_ctx.requested_state = (unsigned int)MOD_PD_STATE_OFF;

    /* Shutdown system PPUs */
    status = shutdown_system_power_ppus(system_shutdown);
    if (status != FWK_SUCCESS) {
        return status;
    }

    return FWK_SUCCESS;
}

/*
 * Functions fulfilling the Power Domain module's driver API
 */

static int system_power_set_state(fwk_id_t pd_id, unsigned int state)
{
    int status;
    unsigned int soc_wakeup_irq;

    if (!fwk_expect(state < MOD_SYSTEM_POWER_POWER_STATE_COUNT)) {
        return FWK_E_PARAM;
    }

    soc_wakeup_irq = system_power_ctx.config->soc_wakeup_irq;

    system_power_ctx.requested_state = state;

    switch (state) {
    case (unsigned int)MOD_PD_STATE_ON:
#ifdef BUILD_HAS_MOD_TIMER
        if (system_power_ctx.test_wake_armed) {
            system_power_ctx.test_wake_alarm->stop(
                system_power_ctx.config->test_wake_alarm_id);
            system_power_ctx.test_wake_armed = false;
        }
#endif
        status = disable_all_irqs();
        if (status != FWK_SUCCESS) {
            return status;
        }

        status = set_system_power_state(state);
        if (status != FWK_SUCCESS) {
            return status;
        }

        ext_ppus_set_state(MOD_PD_STATE_ON);

        break;

    case (unsigned int)MOD_SYSTEM_POWER_POWER_STATE_SLEEP0:
#ifdef BUILD_HAS_MOD_TIMER
        if (system_power_ctx.config->test_wake_delay_us != 0)
            return test_suspend_power_off();
#endif
        ext_ppus_set_state(MOD_PD_STATE_OFF);

        status = fwk_interrupt_clear_pending(soc_wakeup_irq);
        if (status != FWK_SUCCESS) {
            return status;
        }

        if (system_power_ctx.driver_api->platform_interrupts != NULL) {
            status =
                system_power_ctx.driver_api->platform_interrupts(
                    MOD_SYSTEM_POWER_PLATFORM_INTERRUPT_CMD_CLEAR_PENDING);
            if (status != FWK_SUCCESS) {
                return FWK_E_DEVICE;
            }
        }

        status = set_system_power_state(state);
        if (status != FWK_SUCCESS) {
            return status;
        }

        /* Store the identifier of the power domain of the last standing core */
        status = system_power_ctx.pd_driver_input_api->get_last_core_pd_id(
            &system_power_ctx.last_core_pd_id);
        if (status != FWK_SUCCESS) {
            return status;
        }

        status = fwk_interrupt_enable(soc_wakeup_irq);
        if (status != FWK_SUCCESS) {
            return status;
        }

        if (system_power_ctx.driver_api->platform_interrupts != NULL) {
            status = system_power_ctx.driver_api->platform_interrupts(
                MOD_SYSTEM_POWER_PLATFORM_INTERRUPT_CMD_ENABLE);
            if (status != FWK_SUCCESS) {
                return FWK_E_DEVICE;
            }
        }

        break;

    case (unsigned int)MOD_PD_STATE_OFF:
        status = disable_all_irqs();
        if (status != FWK_SUCCESS) {
            return status;
        }

        ext_ppus_set_state(MOD_PD_STATE_OFF);

        status = set_system_power_state(state);
        if (status != FWK_SUCCESS) {
            return status;
        }

        break;

    default:
        return FWK_E_SUPPORT;
    }

    return FWK_SUCCESS;
}

static int system_power_get_state(fwk_id_t pd_id, unsigned int *state)
{
    *state = system_power_ctx.state;

    return FWK_SUCCESS;
}

static int system_power_reset(fwk_id_t pd_id)
{
    return FWK_E_SUPPORT;
}

static int system_power_shutdown(fwk_id_t pd_id,
                                enum mod_pd_system_shutdown system_shutdown)
{
    int status;

    status = shutdown(pd_id, system_shutdown);
    if (status != FWK_SUCCESS) {
        return status;
    }

    return system_power_ctx.driver_api->system_shutdown(system_shutdown);
}

static void soc_wakeup_handler(void)
{
    int status;
    uint32_t state = MOD_SYSTEM_POWER_SOC_WAKEUP_STATE;

    status = disable_all_irqs();
    if (status != FWK_SUCCESS) {
        fwk_trap();
    }

    status = system_power_ctx.pd_restricted_api->set_state(
        system_power_ctx.last_core_pd_id, false, state);
    fwk_check(status == FWK_SUCCESS);
}

static const struct mod_pd_driver_api system_power_power_domain_driver_api = {
    .set_state = system_power_set_state,
    .get_state = system_power_get_state,
    .reset = system_power_reset,
    .shutdown = system_power_shutdown
};

/*
 * Functions fulfilling the Power Domain module's driver input API
 */

static int system_power_report_power_state_transition(fwk_id_t dev_id,
    unsigned int state)
{
    static unsigned int sys_ppu_transition_count = 0;
    unsigned int i, actual_state;
    int status;

    sys_ppu_transition_count++;

    if (sys_ppu_transition_count < system_power_ctx.dev_count) {
        return FWK_SUCCESS;
    }

    sys_ppu_transition_count = 0;

    /* Validate SRAM before publishing ON to the PD tree. Publishing first
     * could release a child CPU before its retained memory is checked.
     */
    if (system_power_ctx.test_retention_pending &&
        system_power_ctx.requested_state == MOD_PD_STATE_ON) {
        if (!system_power_ctx.test_retention_off_verified)
            return FWK_E_STATE;
        for (i = 0; i < system_power_ctx.dev_count; i++) {
            struct system_power_dev_ctx *ctx = &system_power_ctx.dev_ctx_table[i];

            status = ctx->sys_ppu_api->get_state(ctx->config->sys_ppu_id,
                                                &actual_state);
            if (status != FWK_SUCCESS)
                return status;
            if (actual_state != MOD_PD_STATE_ON)
                return FWK_E_STATE;
        }
        status = system_power_ctx.config->test_retention_check(false);
        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR("[SYS-POW TEST] SRAM retention failed; CPU release blocked: %d", status);
            return status;
        }
        system_power_ctx.test_retention_pending = false;
    }

    system_power_ctx.state = system_power_ctx.requested_state;

    return system_power_ctx.pd_driver_input_api->report_power_state_transition(
        system_power_ctx.mod_pd_system_id, system_power_ctx.state);
}

static const struct mod_pd_driver_input_api
    system_power_power_domain_driver_input_api = {
    .report_power_state_transition = system_power_report_power_state_transition
};

/*
 * Functions fulfilling the framework's module interface
 */

static int system_power_mod_init(fwk_id_t module_id,
                                unsigned int element_count,
                                const void *data)
{
    fwk_assert(data != NULL);

    system_power_ctx.config = data;
    system_power_ctx.mod_pd_system_id = FWK_ID_NONE;
    system_power_ctx.dev_count = element_count;

    if (element_count > 0) {
        system_power_ctx.dev_ctx_table =
            fwk_mm_calloc(element_count, sizeof(struct system_power_dev_ctx));
    }

    if (system_power_ctx.config->ext_ppus_count > 0) {
        system_power_ctx.ext_ppu_apis = fwk_mm_calloc(
            system_power_ctx.config->ext_ppus_count,
            sizeof(system_power_ctx.ext_ppu_apis[0]));
    }

    if (system_power_ctx.config->soc_wakeup_irq != FWK_INTERRUPT_NONE) {
        return fwk_interrupt_set_isr(system_power_ctx.config->soc_wakeup_irq,
                                     soc_wakeup_handler);
    }

    return FWK_SUCCESS;
}

static int system_power_mod_element_init(fwk_id_t element_id,
                                         unsigned int unused,
                                         const void *data)
{
    struct system_power_dev_ctx *dev_ctx;

    fwk_assert(data != NULL);

    dev_ctx =
        system_power_ctx.dev_ctx_table + fwk_id_get_element_idx(element_id);

    dev_ctx->config = data;

    /* Ensure a system state table is provided */
    if (dev_ctx->config->sys_state_table == NULL) {
        return FWK_E_DATA;
    }

    return FWK_SUCCESS;
}

static int system_power_bind(fwk_id_t id, unsigned int round)
{
    int status;
    unsigned int i;
    const struct mod_system_power_config *config;
    struct system_power_dev_ctx *dev_ctx;

    if (round == 1) {
        if (!fwk_id_is_type(id, FWK_ID_TYPE_MODULE)) {
            return FWK_SUCCESS;
        }

        /*
         * During the first round of binding, the power domain module should
         * have bound to the power domain driver API provided by the present
         * module. Bind back to the power domain driver input API provided by
         * the system_power_ctx.mod_pd_system_id power domain module element to
         * report power state transitions of the system power domains.
         */
        return fwk_module_bind(
            system_power_ctx.mod_pd_system_id,
            mod_pd_api_id_driver_input,
            &system_power_ctx.pd_driver_input_api);
    }

    if (fwk_id_is_type(id, FWK_ID_TYPE_MODULE)) {

        config = system_power_ctx.config;

#ifdef BUILD_HAS_MOD_TIMER
        if (config->test_wake_delay_us != 0) {
            if (config->soc_wakeup_irq != FWK_INTERRUPT_NONE)
                return FWK_E_PARAM;
            status = fwk_module_bind(config->test_wake_alarm_id,
                MOD_TIMER_API_ID_ALARM, &system_power_ctx.test_wake_alarm);
            if (status != FWK_SUCCESS)
                return status;
        }
#else
        if (config->test_wake_delay_us != 0)
            return FWK_E_SUPPORT;
#endif

        for (i = 0; i < config->ext_ppus_count; i++) {
            status = fwk_module_bind(
                config->ext_ppus[i].ppu_id,
                config->ext_ppus[i].api_id,
                &system_power_ctx.ext_ppu_apis[i]);
            if (status != FWK_SUCCESS) {
                return status;
            }
        }

        status = fwk_module_bind(config->driver_id,
            config->driver_api_id,
            &system_power_ctx.driver_api);
        if (status != FWK_SUCCESS) {
            return status;
        }

        return fwk_module_bind(
            fwk_module_id_power_domain,
            mod_pd_api_id_restricted,
            &system_power_ctx.pd_restricted_api);
    }

    dev_ctx = system_power_ctx.dev_ctx_table + fwk_id_get_element_idx(id);

    return fwk_module_bind(dev_ctx->config->sys_ppu_id,
                           dev_ctx->config->api_id,
                           &dev_ctx->sys_ppu_api);
}

static int system_power_process_bind_request(fwk_id_t requester_id,
                                             fwk_id_t pd_id,
                                             fwk_id_t api_id,
                                             const void **api)
{
    unsigned int dev_idx;
    struct system_power_dev_ctx *dev_ctx;

    if (fwk_id_is_equal(api_id, mod_system_power_api_id_pd_driver)) {
        if (!fwk_id_is_equal(
                fwk_id_build_module_id(requester_id),
                fwk_module_id_power_domain)) {
            return FWK_E_ACCESS;
        }

        *api = &system_power_power_domain_driver_api;
         system_power_ctx.mod_pd_system_id = requester_id;
    } else {
        for (dev_idx = 0; dev_idx < system_power_ctx.dev_count; dev_idx++) {
            dev_ctx = &system_power_ctx.dev_ctx_table[dev_idx];

            /*
             * If requester_id refers to a system PPU configured by any one of
             * our elements, break when dev_idx reaches that element.
             */
            if (fwk_id_is_equal(requester_id, dev_ctx->config->sys_ppu_id)) {
                break;
            }
        }
        if (dev_idx >= system_power_ctx.dev_count) {
            /* Requester_id does not refer to any configured system PPU */
             return FWK_E_ACCESS;
        }

        *api = &system_power_power_domain_driver_input_api;
    }

    return FWK_SUCCESS;
}

static int system_power_start(fwk_id_t id)
{
    int status;

    if (system_power_ctx.driver_api->platform_interrupts != NULL) {
        status = system_power_ctx.driver_api->platform_interrupts(
            MOD_SYSTEM_POWER_PLATFORM_INTERRUPT_CMD_INIT);
        if (status != FWK_SUCCESS) {
            return status;
        }
    }

    /* Configure initial power state */
    system_power_ctx.state =
        (unsigned int)system_power_ctx.config->initial_system_power_state;

    return FWK_SUCCESS;
}

const struct fwk_module module_system_power = {
    .type = FWK_MODULE_TYPE_DRIVER,
    .api_count = (unsigned int)MOD_SYSTEM_POWER_API_COUNT,
    .init = system_power_mod_init,
    .element_init = system_power_mod_element_init,
    .bind = system_power_bind,
    .start = system_power_start,
    .process_bind_request = system_power_process_bind_request,
};
