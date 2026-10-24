/*
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "si0_cfgd_mhu3.h"
#include "si0_cfgd_timer.h"
#include "si0_mmap.h"

#include <unity.h>

#include <mod_integration_test.h>
#include <mod_mhu3.h>
#include <mod_timer.h>
#include <mod_transport.h>

#include <fwk_core.h>
#include <fwk_interrupt.h>
#include <fwk_log.h>
#include <fwk_mmio.h>
#include <fwk_module.h>

static struct mod_transport_driver_api *mhu_api;
static const struct mod_mhu3_diagnostic_api *diagnostic_api;
static const struct mod_timer_alarm_api *alarm_api;
static struct mod_transport_fast_channel_addr receive_address;
static struct mod_transport_fast_channel_addr transmit_address;
static struct mod_mhu3_stats initial_stats;
static volatile uint32_t received;
static volatile unsigned int accepted;
static volatile bool active;
static volatile bool event_queued;
static volatile unsigned int ticks;
static volatile int completion_status;
static unsigned int phase;
static unsigned int current_case;
static const fwk_id_t device_id =
    FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_MHU3, SI0_CFGD_MOD_MHU3_EIDX_SI0_AP_NS);
static const fwk_id_t alarm_id = FWK_ID_SUB_ELEMENT_INIT(
    FWK_MODULE_IDX_TIMER,
    SI0_SI0_TIMER_ALARM_ELEMENT_IDX,
    SI0_CFGD_MHU_FCE_ALARM_IDX);

static void receive(uintptr_t param)
{
    uint32_t value = fwk_mmio_read_32(receive_address.local_view_address);

    /* Reading PAY acknowledges before returning from the hardware ISR. */
    if (!active) {
        return;
    }
    received = value;
    accepted++;
    fwk_mmio_write_32(transmit_address.local_view_address, value ^ UINT32_MAX);
    active = false;
}

static void watchdog(uintptr_t param)
{
    struct fwk_event event = {
        .id = MOD_INTEGRATION_TEST_EVENT_ID_STEP_CONTINUE,
        .source_id = FWK_ID_MODULE(FWK_MODULE_IDX_TEST_MHU_FCE),
        .target_id = FWK_ID_MODULE(FWK_MODULE_IDX_INTEGRATION_TEST),
        .params = { 0, 1 },
    };
    if (event_queued) {
        return;
    }
    if (active && ++ticks < 300) {
        return;
    }
    if (active) {
        active = false;
        completion_status = FWK_E_TIMEOUT;
        FWK_LOG_ERR(
            "MHU_FCE_FAIL case=%u phase=%u status=%d",
            current_case,
            phase,
            completion_status);
    }
    int status = fwk_put_event(&event);
    if (status == FWK_SUCCESS) {
        event_queued = true;
    } else if (completion_status == FWK_SUCCESS) {
        completion_status = status;
        FWK_LOG_ERR("MHU_FCE_FAIL queue status=%d", status);
    }
}

static int arm_receive(void)
{
    unsigned int flags = fwk_interrupt_global_disable();
    event_queued = false;
    ticks = 0;
    completion_status = FWK_SUCCESS;
    active = true;
    fwk_interrupt_global_enable(flags);
    int status = alarm_api->start(
        alarm_id, 100000, MOD_TIMER_ALARM_TYPE_PERIODIC, watchdog, 0);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    uint32_t payload = current_case == 0 ? 0x320AE001 : 0x320AE010 + phase;
    fwk_log_printf(
        "MHU_FCE_READY case=%u phase=%u channel=32 payload=0x%08x",
        current_case,
        phase,
        payload);
    return FWK_PENDING;
}

static int run(
    unsigned int case_idx,
    unsigned int step_idx,
    const struct fwk_event *event)
{
    struct mod_mhu3_stats stats;
    if (case_idx > 1 || step_idx > 1) {
        return FWK_E_PARAM;
    }
    if (step_idx == 0) {
        TEST_ASSERT_EQUAL_UINT(4, receive_address.length);
        TEST_ASSERT_EQUAL_UINT(4, transmit_address.length);
        TEST_ASSERT_EQUAL_PTR(
            SI0_AP2SI0_NS_MHUV3_RCV_BASE + 0x3080,
            receive_address.local_view_address);
        TEST_ASSERT_EQUAL_PTR(
            SI0_SI02AP_NS_MHUV3_SEND_BASE + 0x3080,
            transmit_address.local_view_address);
        TEST_ASSERT_EQUAL_HEX32(
            2, fwk_mmio_read_32(SI0_AP2SI0_NS_MHUV3_RCV_BASE + 0x144));
        (void)fwk_mmio_read_32(receive_address.local_view_address);
        TEST_ASSERT_EQUAL(
            FWK_SUCCESS, diagnostic_api->get_stats(device_id, &initial_stats));
        accepted = phase = 0;
        current_case = case_idx;
        return arm_receive();
    }
    (void)alarm_api->stop(alarm_id);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, completion_status);
    TEST_ASSERT_EQUAL_UINT(phase + 1, accepted);
    TEST_ASSERT_EQUAL_HEX32(
        case_idx == 0 ? 0x320AE001 : 0x320AE010 + phase, received);
    TEST_ASSERT_EQUAL_HEX32(
        0, fwk_mmio_read_32(SI0_AP2SI0_NS_MHUV3_RCV_BASE + 0x484));
    TEST_ASSERT_EQUAL(
        FWK_SUCCESS, diagnostic_api->get_stats(device_id, &stats));
    if (case_idx == 1) {
        TEST_ASSERT_EQUAL_UINT(
            1, stats.unregistered - initial_stats.unregistered);
        TEST_ASSERT_EQUAL_UINT(33, stats.last_unregistered_channel);
        TEST_ASSERT_EQUAL_HEX32(
            1UL << 31, fwk_mmio_read_32(SI0_AP2SI0_NS_MHUV3_RCV_BASE + 0x480));
        if (++phase < 3) {
            return arm_receive();
        }
        /* The masked group never caused an IRQ; the diagnostic owns cleanup. */
        (void)fwk_mmio_read_32(SI0_AP2SI0_NS_MHUV3_RCV_BASE + 0x307C);
    }
    TEST_ASSERT_EQUAL_UINT(
        case_idx == 0 ? 1 : 3, stats.callbacks - initial_stats.callbacks);
    fwk_log_printf(
        "MHU_FCE_PASS case=%u accepted=%u irq_entries=%u callbacks=%u "
        "unregistered=%u",
        case_idx,
        accepted,
        stats.irq_entries - initial_stats.irq_entries,
        stats.callbacks - initial_stats.callbacks,
        stats.unregistered - initial_stats.unregistered);
    return FWK_SUCCESS;
}

static void teardown(void)
{
    active = false;
    event_queued = true;
    (void)alarm_api->stop(alarm_id);
    (void)fwk_mmio_read_32(SI0_AP2SI0_NS_MHUV3_RCV_BASE + 0x307C);
}

static const char *test_name(unsigned int case_idx)
{
    static const char *names[] = {
        "hardware_fast_channel_irq_echo",
        "fast_channel_ownership_and_repeat",
    };
    return case_idx < 2 ? names[case_idx] : NULL;
}

static const struct mod_integration_test_api test_api = {
    .run = run,
    .test_name = test_name,
    .teardown = teardown,
};

static int bind(fwk_id_t id, unsigned int round)
{
    int status;
    fwk_id_t receive_id = FWK_ID_SUB_ELEMENT(
        FWK_MODULE_IDX_MHU3,
        SI0_CFGD_MOD_MHU3_EIDX_SI0_AP_NS,
        SI0_CFGD_MOD_MHU3_AP_NS_FCE_IN);
    fwk_id_t transmit_id = FWK_ID_SUB_ELEMENT(
        FWK_MODULE_IDX_MHU3,
        SI0_CFGD_MOD_MHU3_EIDX_SI0_AP_NS,
        SI0_CFGD_MOD_MHU3_AP_NS_FCE_OUT);
    if (round != 0) {
        return FWK_SUCCESS;
    }
    status = fwk_module_bind(
        alarm_id,
        FWK_ID_API(FWK_MODULE_IDX_TIMER, MOD_TIMER_API_IDX_ALARM),
        &alarm_api);
    if (status != FWK_SUCCESS)
        return status;
    status = fwk_module_bind(
        device_id,
        FWK_ID_API(FWK_MODULE_IDX_MHU3, MOD_MHU3_API_IDX_DIAGNOSTIC),
        &diagnostic_api);
    if (status != FWK_SUCCESS)
        return status;
    status = fwk_module_bind(
        receive_id,
        FWK_ID_API(FWK_MODULE_IDX_MHU3, MOD_MHU3_API_IDX_TRANSPORT_DRIVER),
        &mhu_api);
    if (status != FWK_SUCCESS)
        return status;
    status = mhu_api->get_fch_address(receive_id, &receive_address);
    if (status != FWK_SUCCESS)
        return status;
    status = mhu_api->get_fch_address(transmit_id, &transmit_address);
    if (status != FWK_SUCCESS)
        return status;
    return mhu_api->fch_register_callback(
        receive_id, (uintptr_t)&active, receive);
}

static int process_bind_request(
    fwk_id_t source_id,
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

static int init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *data)
{
    return FWK_SUCCESS;
}

struct fwk_module_config config_test_mhu_fce = { 0 };
const struct fwk_module module_test_mhu_fce = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .init = init,
    .bind = bind,
    .process_bind_request = process_bind_request,
    .api_count = MOD_INTEGRATION_TEST_API_COUNT,
};
