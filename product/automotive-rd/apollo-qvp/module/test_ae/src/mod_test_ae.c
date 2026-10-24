/*
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "si0_cfgd_timer.h"
#include "si0_irq.h"
#include "si0_mmap.h"

#include <unity.h>

#include <mod_fmu.h>
#include <mod_integration_test.h>
#include <mod_timer.h>

#include <fwk_core.h>
#include <fwk_interrupt.h>
#include <fwk_log.h>
#include <fwk_mmio.h>
#include <fwk_module.h>
#include <fwk_notification.h>

#define MHU_BASE       SI0_PC0_CL0_MHU_FMU_BASE
#define GIC_BASE       SI0_GIC_FMU_BASE
#define KEY            0xF20
#define TIMEOUT        0xF24
#define GSR            0xE00
#define EXPECTED_COUNT 5

static const struct mod_fmu_api *fmu_api;
static const struct mod_timer_alarm_api *alarm_api;
static const fwk_id_t alarm_id = FWK_ID_SUB_ELEMENT_INIT(
    FWK_MODULE_IDX_TIMER,
    SI0_SI0_TIMER_ALARM_ELEMENT_IDX,
    SI0_CFGD_AE_ALARM_IDX);
static const struct mod_fmu_fault_notification_params expected[] = {
    { .critical = true,
      .fault = { .device_idx = 8, .node_idx = 0, .sm_idx = 3 } },
    { .critical = true,
      .fault = { .device_idx = 8, .node_idx = 0, .sm_idx = 4 } },
    { .critical = false,
      .fault = { .device_idx = 8, .node_idx = 0, .sm_idx = 17 } },
    { .critical = true,
      .fault = { .device_idx = 5, .node_idx = 3, .sm_idx = 3, .block_id = 1 } },
    { .critical = true,
      .fault = { .device_idx = 5, .node_idx = 3, .sm_idx = 3 } },
};
static struct mod_fmu_fault_notification_params notifications[16];
static volatile unsigned int notification_count;
static volatile bool active;
static volatile bool event_queued;
static volatile unsigned int ticks;
static volatile int completion_status;
static unsigned int current_case;
static unsigned int stage;
static uint32_t saved_timeout;
static struct mod_fmu_drain_status ram_initial_drain[2];

static void write_timeout(uint32_t value)
{
    fwk_mmio_write_32(MHU_BASE + KEY, 0xBE);
    fwk_mmio_write_32(MHU_BASE + TIMEOUT, value);
}

static bool drained(void)
{
    struct mod_fmu_drain_status critical, noncritical;
    fmu_api->get_drain_status(true, &critical);
    fmu_api->get_drain_status(false, &noncritical);
    return !critical.masked && !noncritical.masked &&
        fwk_mmio_read_32(MHU_BASE + GSR) == 0 &&
        fwk_mmio_read_32(GIC_BASE + GSR) == 0;
}

static bool hierarchy_drained(void)
{
    if (!drained())
        return false;
    for (unsigned int bank = 0; bank < 5; bank++) {
        for (unsigned int word = 0; word < 12; word++) {
            if (fwk_mmio_read_32(SI0_FMU0_BASE + bank * 0x10000 +
                                 0xE000 + word * 4) != 0)
                return false;
        }
    }
    return fwk_mmio_read_32(SI0_ATW6_NI710AE_CLUSTER0_BASE + 0xE000) == 0 &&
        fwk_mmio_read_32(SI0_ATW7_NI710AE_CLUSTER1_BASE + 0xE000) == 0;
}

static void watchdog(uintptr_t param)
{
    bool ready;
    struct mod_fmu_drain_status state;
    struct fwk_event event = {
        .id = MOD_INTEGRATION_TEST_EVENT_ID_STEP_CONTINUE,
        .source_id = FWK_ID_MODULE(FWK_MODULE_IDX_TEST_AE),
        .target_id = FWK_ID_MODULE(FWK_MODULE_IDX_INTEGRATION_TEST),
        .params = { 0, stage },
    };
    if (!active || event_queued)
        return;
    fmu_api->get_drain_status(true, &state);
    if (current_case == 4 || current_case == 5) {
        ready = notification_count >= (current_case == 4 ? 4U : 2U) &&
            hierarchy_drained();
    } else if (current_case == 0) {
        ready = notification_count >= EXPECTED_COUNT && drained();
    } else if (current_case == 3) {
        ready = notification_count != 0 && drained();
    } else if (current_case == 2 && stage == 1) {
        ready = state.masked && state.last_error == FWK_E_TIMEOUT;
    } else {
        ready = drained();
    }
    if (!ready && ++ticks < (current_case == 3 ? 3000U : 500U))
        return;
    if (!ready) {
        completion_status = FWK_E_TIMEOUT;
        FWK_LOG_ERR(
            "AE_FAIL case=%u stage=%u deadline notifications=%u",
            current_case,
            stage,
            notification_count);
    }
    int status = fwk_put_event(&event);
    if (status == FWK_SUCCESS) {
        event_queued = true;
    } else if (completion_status == FWK_SUCCESS) {
        completion_status = status;
        FWK_LOG_ERR("AE_FAIL queue status=%d", status);
    }
}

static int wait_for(unsigned int next_stage)
{
    stage = next_stage;
    ticks = 0;
    event_queued = false;
    completion_status = FWK_SUCCESS;
    TEST_ASSERT_EQUAL(
        FWK_SUCCESS,
        alarm_api->start(
            alarm_id, 10000, MOD_TIMER_ALARM_TYPE_PERIODIC, watchdog, 0));
    return FWK_PENDING;
}

static void setup(void)
{
    active = true;
    event_queued = true;
    notification_count = 0;
    saved_timeout = fwk_mmio_read_32(MHU_BASE + TIMEOUT);
}

static void teardown(void)
{
    struct mod_fmu_drain_status state;
    active = false;
    event_queued = true;
    (void)alarm_api->stop(alarm_id);
    write_timeout(saved_timeout);
    for (unsigned int critical = 0; critical < 2; critical++) {
        fmu_api->get_drain_status(critical, &state);
        if (state.masked)
            (void)fmu_api->recover(critical);
        else
            (void)fwk_interrupt_enable(
                critical ? CL0_FMU_CRITICAL : CL0_FMU_NON_CRITICAL);
    }
}

static int run(
    unsigned int case_idx,
    unsigned int step_idx,
    const struct fwk_event *event)
{
    struct mod_fmu_fault fault = expected[0].fault;
    struct mod_fmu_drain_status state;
    current_case = case_idx;
    if (step_idx != 0) {
        (void)alarm_api->stop(alarm_id);
        TEST_ASSERT_EQUAL(FWK_SUCCESS, completion_status);
    }
    if (case_idx == 0) {
        if (step_idx == 0) {
            TEST_ASSERT_EQUAL(
                FWK_SUCCESS, fwk_interrupt_disable(CL0_FMU_CRITICAL));
            TEST_ASSERT_EQUAL(
                FWK_SUCCESS, fwk_interrupt_disable(CL0_FMU_NON_CRITICAL));
            for (unsigned int i = 0; i < EXPECTED_COUNT; i++) {
                TEST_ASSERT_EQUAL(
                    FWK_SUCCESS,
                    fmu_api->set_enabled(&expected[i].fault, true));
                TEST_ASSERT_EQUAL(
                    FWK_SUCCESS,
                    fmu_api->set_critical(
                        &expected[i].fault, expected[i].critical));
                TEST_ASSERT_EQUAL(
                    FWK_SUCCESS, fmu_api->inject(&expected[i].fault));
            }
            TEST_ASSERT_EQUAL(
                FWK_SUCCESS, fwk_interrupt_enable(CL0_FMU_CRITICAL));
            TEST_ASSERT_EQUAL(
                FWK_SUCCESS, fwk_interrupt_enable(CL0_FMU_NON_CRITICAL));
            return wait_for(1);
        }
        TEST_ASSERT_EQUAL_UINT(EXPECTED_COUNT, notification_count);
        for (unsigned int i = 0; i < EXPECTED_COUNT; i++) {
            unsigned int matches = 0;
            for (unsigned int j = 0; j < notification_count; j++) {
                const struct mod_fmu_fault_notification_params *p =
                    &notifications[j];
                matches += p->critical == expected[i].critical &&
                    p->fault.device_idx == expected[i].fault.device_idx &&
                    p->fault.node_idx == expected[i].fault.node_idx &&
                    p->fault.sm_idx == expected[i].fault.sm_idx &&
                    p->fault.block_id == expected[i].fault.block_id;
            }
            TEST_ASSERT_EQUAL_UINT(1, matches);
        }
        fwk_log_printf(
            "AE_MIXED_PASS notifications=5 mhu_smids=3,4,17 gic_blkids=1,0");
    } else if (case_idx == 1) {
        if (step_idx == 0) {
            fault = expected[3].fault;
            fault.block_id = 2; /* The active GIC has two GCI blocks. */
            TEST_ASSERT_EQUAL(FWK_E_PARAM, fmu_api->inject(&fault));
            fault = expected[0].fault;
            fault.sm_idx = 22; /* MHU PBX supports SMID 1..21. */
            TEST_ASSERT_EQUAL(FWK_E_PARAM, fmu_api->inject(&fault));
            /* Start a valid operation before draining resulting FMU
             * self-errors. */
            fault = expected[0].fault;
            TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->set_enabled(&fault, true));
            return wait_for(1);
        }
        fwk_log_printf("AE_INVALID_PASS block=2 smid=22 status=FWK_E_PARAM");
    } else if (case_idx == 2) {
        if (step_idx == 0) {
            TEST_ASSERT_EQUAL(
                FWK_SUCCESS, fwk_interrupt_disable(CL0_FMU_CRITICAL));
            TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->set_enabled(&fault, true));
            TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->inject(&fault));
            write_timeout(0);
            TEST_ASSERT_EQUAL(
                FWK_SUCCESS, fwk_interrupt_enable(CL0_FMU_CRITICAL));
            return wait_for(1);
        }
        if (step_idx == 1) {
            TEST_ASSERT_EQUAL(
                FWK_SUCCESS, fmu_api->get_drain_status(true, &state));
            TEST_ASSERT_TRUE(state.masked);
            TEST_ASSERT_EQUAL(FWK_E_TIMEOUT, state.last_error);
            fwk_log_printf(
                "AE_TIMEOUT_OBSERVED masked=1 status=%d", state.last_error);
            /* Repair the controller before retrying the failed leaf ACK. */
            write_timeout(saved_timeout);
            TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->recover(true));
            return wait_for(2);
        }
        TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->get_drain_status(true, &state));
        TEST_ASSERT_FALSE(state.masked);
        TEST_ASSERT_EQUAL(FWK_SUCCESS, state.last_error);
        fwk_log_printf(
            "AE_RECOVERY_PASS masked=0 gsr=0 notifications=%u",
            notification_count);
    } else
        return FWK_E_PARAM;
    return FWK_SUCCESS;
}

static int process_notification(
    const struct fwk_event *event,
    struct fwk_event *response)
{
    if (active && fwk_id_is_equal(event->id, mod_fmu_notification_id_fault)) {
        const struct mod_fmu_fault_notification_params *p =
            (const void *)event->params;
        if (notification_count < 16)
            notifications[notification_count] = *p;
        notification_count++;
        if (current_case == 4 || current_case == 5) {
            fwk_log_printf(
                "FMU_HIERARCHY_FAULT critical=%u device=%u node=%u smid=%u",
                p->critical, p->fault.device_idx, p->fault.node_idx,
                p->fault.sm_idx);
            return FWK_SUCCESS;
        }
        fwk_log_printf(
            "AE_FAULT critical=%u device=%u type=%u block=%u smid=%u "
            "injected=%u",
            p->critical,
            p->fault.device_idx,
            p->fault.node_idx,
            p->fault.block_id,
            p->fault.sm_idx,
            p->fault.injected);
    }
    return FWK_SUCCESS;
}

static const char *test_name(unsigned int case_idx)
{
    static const char *names[] = {
        "mixed_severity_smid_and_block_identity",
        "invalid_block_and_smid",
        "ack_timeout_mask_and_explicit_recovery",
    };
    return case_idx < 3 ? names[case_idx] : NULL;
}
static const struct mod_integration_test_api test_api = {
    .run = run,
    .test_name = test_name,
    .setup = setup,
    .teardown = teardown,
};
static bool matches_ram_fault(
    const struct mod_fmu_fault_notification_params *fault,
    unsigned int kind)
{
    return kind < 3 && fault->critical == (kind == 2) &&
        fault->fault.device_idx == 8 && fault->fault.node_idx == 1 &&
        fault->fault.block_id == 0 && fault->fault.sm_idx == 20 + kind &&
        !fault->fault.injected;
}

static uintptr_t ram_status_address(unsigned int record)
{
    return SI0_AP2SI0_NS_MHUV3_RCV_BASE + 0x10000 + record * 64 + 0x10;
}

static void clear_ram_status(void)
{
    for (unsigned int record = 4; record <= 5; record++) {
        uintptr_t address = ram_status_address(record);
        fwk_mmio_write_32(address, fwk_mmio_read_32(address) & 0xFF000000U);
    }
}

static int run_ram(
    unsigned int case_idx,
    unsigned int step_idx,
    const struct fwk_event *event)
{
    struct mod_fmu_drain_status observed;
    struct mod_fmu_fault fault = {
        .device_idx = 8,
        .node_idx = 1,
        .block_id = 0,
        .sm_idx = 20 + case_idx,
    };
    if (case_idx >= 3 || step_idx > 1)
        return FWK_E_PARAM;
    current_case = 3;
    if (step_idx == 0) {
        (void)fwk_mmio_read_32(SI0_AP2SI0_NS_MHUV3_RCV_BASE + 0x3080);
        clear_ram_status();
        TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->set_enabled(&fault, true));
        TEST_ASSERT_EQUAL(
            FWK_SUCCESS, fmu_api->set_critical(&fault, case_idx == 2));
        for (unsigned int critical = 0; critical < 2; critical++) {
            TEST_ASSERT_EQUAL(
                FWK_SUCCESS,
                fmu_api->get_drain_status(
                    critical, &ram_initial_drain[critical]));
            TEST_ASSERT_FALSE(ram_initial_drain[critical].masked);
        }
        /* Only external RAM corruption plus an incoming payload may trigger
         * this case. It deliberately never calls the FMU inject API. */
        fwk_log_printf(
            "AE_RAM_READY kind=%u channel=32 payload=0x%08x smid=%u "
            "critical=%u",
            case_idx,
            0xAE320100U + case_idx,
            20 + case_idx,
            case_idx == 2);
        return wait_for(1);
    }
    (void)alarm_api->stop(alarm_id);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, completion_status);
    TEST_ASSERT_EQUAL_UINT(1, notification_count);
    TEST_ASSERT_TRUE(matches_ram_fault(&notifications[0], case_idx));
    uintptr_t ras = ram_status_address(case_idx == 2 ? 5 : 4);
    uint32_t status = fwk_mmio_read_32(ras);
    TEST_ASSERT_BITS_HIGH(1UL << 30, status);
    TEST_ASSERT_EQUAL_UINT(case_idx == 2, (status >> 29) & 1);
    TEST_ASSERT_EQUAL_UINT(32, fwk_mmio_read_32(ras + 8));
    uint32_t transfer_lost = (fwk_mmio_read_32(ras + 16) >> 16) & 1;
    uint32_t payload = fwk_mmio_read_32(SI0_AP2SI0_NS_MHUV3_RCV_BASE + 0x3080);
    TEST_ASSERT_EQUAL_UINT(case_idx != 0, transfer_lost);
    TEST_ASSERT_EQUAL_HEX32(case_idx == 0 ? 0xAE320100U : 0, payload);
    for (unsigned int critical = 0; critical < 2; critical++) {
        TEST_ASSERT_EQUAL(
            FWK_SUCCESS, fmu_api->get_drain_status(critical, &observed));
        TEST_ASSERT_FALSE(observed.masked);
        TEST_ASSERT_EQUAL(FWK_SUCCESS, observed.last_error);
        TEST_ASSERT_EQUAL_UINT(
            critical == (case_idx == 2) ? 1 : 0,
            observed.drained - ram_initial_drain[critical].drained);
    }
    TEST_ASSERT_EQUAL_HEX32(0, fwk_mmio_read_32(MHU_BASE + GSR));
    fwk_log_printf(
        "AE_RAM_PASS kind=%u device=8 type=1 block=0 smid=%u critical=%u "
        "injected=0 gsr=0 tl=%u payload=0x%08x",
        case_idx,
        20 + case_idx,
        case_idx == 2,
        transfer_lost,
        payload);
    return FWK_SUCCESS;
}

static const char *ram_test_name(unsigned int case_idx)
{
    static const char *names[] = {
        "external_fast_ram_sec_data",
        "external_fast_ram_sec_address",
        "external_fast_ram_ded",
    };
    return case_idx < 3 ? names[case_idx] : NULL;
}

static void ram_setup(void)
{
    setup();
    current_case = 3;
}

static void ram_teardown(void)
{
    /* A one-shot armed latent fault is also consumed by a final payload read,
     * so an aborted diagnostic cannot leak it into the next test. */
    (void)fwk_mmio_read_32(SI0_AP2SI0_NS_MHUV3_RCV_BASE + 0x3080);
    clear_ram_status();
    teardown();
}

static const struct mod_integration_test_api ram_test_api = {
    .run = run_ram,
    .test_name = ram_test_name,
    .setup = ram_setup,
    .teardown = ram_teardown,
};

/* System input records and NI route geometry belong to the active QVP.
 * FHI is the explicitly configured middle record of each ERI/FHI/CRI triplet. */
static const struct mod_fmu_fault_notification_params hierarchy_expected[][4] = {
    {
        { .critical = true, .fault = { .device_idx = 0, .node_idx = 0, .sm_idx = 16 } },
        { .fault = { .device_idx = 0, .node_idx = 10, .sm_idx = 16 } },
        { .critical = true, .fault = { .device_idx = 1, .node_idx = 70, .sm_idx = 16 } },
        { .fault = { .device_idx = 2, .node_idx = 65, .sm_idx = 8 } },
    },
    {
        { .critical = true, .fault = { .device_idx = 16, .node_idx = 11, .sm_idx = 13 } },
        { .fault = { .device_idx = 17, .node_idx = 11, .sm_idx = 14 } },
    },
};
static struct {
    uintptr_t base;
    unsigned int node;
    uint32_t control, impdef;
} saved_system[5];
static unsigned int saved_system_count;
static struct {
    uintptr_t base;
    uint32_t control, smen, selector_low, selector_high;
} saved_ni[2];
static unsigned int saved_ni_count;

static void hierarchy_write(uintptr_t base, unsigned int key,
                            unsigned int offset, uint32_t value)
{
    fwk_mmio_write_32(base + key, 0xBE);
    fwk_mmio_write_32(base + offset, value);
}

static void configure_system_source(unsigned int device, unsigned int node,
                                    bool upgrade)
{
    uintptr_t base = SI0_FMU0_BASE + device * 0x10000;
    unsigned int index = saved_system_count++;
    saved_system[index].base = base;
    saved_system[index].node = node;
    saved_system[index].control = fwk_mmio_read_32(base + node * 64 + 8);
    saved_system[index].impdef = fwk_mmio_read_32(base + 0x8000 + node * 8);
    /* Enable the input ERR_IN and parity detectors for this scoped test. */
    hierarchy_write(base, 0x8BFC, 0x8000 + node * 8,
                    saved_system[index].impdef | (3U << 7));
    struct mod_fmu_fault fault = {
        .device_idx = device, .node_idx = node, .sm_idx = MOD_FMU_SM_ALL,
    };
    fwk_id_t id = FWK_ID_ELEMENT(FWK_MODULE_IDX_FMU, device);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->set_enabled(&fault, true));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->set_count(id, node, 0));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->set_threshold(id, node, 0));
    TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->set_upgrade_enabled(id, node, upgrade));
}

static void save_ni_config(uintptr_t base)
{
    unsigned int index = saved_ni_count++;
    saved_ni[index].base = base;
    saved_ni[index].control = fwk_mmio_read_32(base + 8);
    saved_ni[index].selector_low = fwk_mmio_read_32(base + 0xE210);
    saved_ni[index].selector_high = fwk_mmio_read_32(base + 0xE214);
    hierarchy_write(base, 0xE200, 0xE210,
                    fwk_mmio_read_32(base + 11 * 64 + 0x20));
    hierarchy_write(base, 0xE200, 0xE214,
                    fwk_mmio_read_32(base + 11 * 64 + 0x24));
    saved_ni[index].smen = fwk_mmio_read_32(base + 0xE204);
}

static bool hierarchy_identity_matches(unsigned int case_idx)
{
    unsigned int count = case_idx == 0 ? 4 : 2;
    if (case_idx > 1 || notification_count != count)
        return false;
    for (unsigned int i = 0; i < count; i++) {
        const struct mod_fmu_fault_notification_params *want =
            &hierarchy_expected[case_idx][i];
        unsigned int matches = 0;
        for (unsigned int j = 0; j < count; j++) {
            const struct mod_fmu_fault_notification_params *got = &notifications[j];
            matches += got->critical == want->critical &&
                got->fault.device_idx == want->fault.device_idx &&
                got->fault.node_idx == want->fault.node_idx &&
                got->fault.sm_idx == want->fault.sm_idx &&
                got->fault.block_id == 0;
        }
        if (matches != 1)
            return false;
    }
    return true;
}

static int run_hierarchy(unsigned int case_idx, unsigned int step_idx,
                         const struct fwk_event *event)
{
    if (case_idx > 1 || step_idx > 1)
        return FWK_E_PARAM;
    current_case = 4 + case_idx;
    if (step_idx == 0) {
        TEST_ASSERT_TRUE(hierarchy_drained());
        for (unsigned int critical = 0; critical < 2; critical++) {
            TEST_ASSERT_EQUAL(FWK_SUCCESS,
                fmu_api->get_drain_status(critical, &ram_initial_drain[critical]));
            TEST_ASSERT_EQUAL(FWK_SUCCESS, fwk_interrupt_disable(
                critical ? CL0_FMU_CRITICAL : CL0_FMU_NON_CRITICAL));
        }
        if (case_idx == 0) {
            /* Root upgrades System1's NC summary; preserve its leaf identity. */
            configure_system_source(0, 1, true);
            for (unsigned int i = 0; i < 4; i++) {
                const struct mod_fmu_fault *fault = &hierarchy_expected[0][i].fault;
                configure_system_source(fault->device_idx, fault->node_idx, false);
                TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->inject(fault));
            }
        } else {
            /* vector0 UE is upgraded at the System ERI input; it has no NI CI. */
            configure_system_source(1, 1, true);
            configure_system_source(1, 2, false);
            configure_system_source(1, 17, false);
            save_ni_config(SI0_ATW6_NI710AE_CLUSTER0_BASE);
            save_ni_config(SI0_ATW7_NI710AE_CLUSTER1_BASE);
            struct mod_fmu_fault fault = { .device_idx = 16, .node_idx = 11, .sm_idx = 14 };
            TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->set_enabled(&fault, true));
            TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->inject(&fault));
            fault.sm_idx = 13;
            TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->set_enabled(&fault, true));
            TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->inject(&fault));
            fault.device_idx = 17;
            fault.sm_idx = 14;
            TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->set_enabled(&fault, true));
            TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->inject(&fault));
            uint32_t status = fwk_mmio_read_32(
                SI0_ATW6_NI710AE_CLUSTER0_BASE + 11 * 64 + 0x10);
            TEST_ASSERT_BITS_HIGH((1U << 30) | (1U << 29) | (1U << 27) | (2U << 24), status);
            TEST_ASSERT_BITS_LOW(1U << 19, status);
            TEST_ASSERT_EQUAL_UINT(13, (status >> 8) & 0xFF);
            fwk_log_printf("FMU_HIERARCHY_PENDING case=1 ni_status=0x%08x syndrome=13", status);
        }
        TEST_ASSERT_EQUAL(FWK_SUCCESS, fwk_interrupt_enable(CL0_FMU_CRITICAL));
        TEST_ASSERT_EQUAL(FWK_SUCCESS, fwk_interrupt_enable(CL0_FMU_NON_CRITICAL));
        return wait_for(1);
    }
    (void)alarm_api->stop(alarm_id);
    TEST_ASSERT_EQUAL(FWK_SUCCESS, completion_status);
    TEST_ASSERT_TRUE(hierarchy_identity_matches(case_idx));
    TEST_ASSERT_TRUE(hierarchy_drained());
    for (unsigned int critical = 0; critical < 2; critical++) {
        struct mod_fmu_drain_status state;
        TEST_ASSERT_EQUAL(FWK_SUCCESS, fmu_api->get_drain_status(critical, &state));
        TEST_ASSERT_EQUAL(FWK_SUCCESS, state.last_error);
        TEST_ASSERT_EQUAL_UINT(case_idx == 0 ? 2 : 1,
            state.drained - ram_initial_drain[critical].drained);
    }
    fwk_log_printf("FMU_HIERARCHY_PASS case=%u notifications=%u critical=%u noncritical=%u gsr=0",
        case_idx, notification_count, case_idx == 0 ? 2 : 1, case_idx == 0 ? 2 : 1);
    return FWK_SUCCESS;
}

static void hierarchy_setup(void)
{
    setup();
    saved_system_count = saved_ni_count = 0;
    current_case = 4;
}
static void hierarchy_teardown(void)
{
    for (unsigned int i = 0; i < saved_ni_count; i++) {
        uintptr_t base = saved_ni[i].base;
        hierarchy_write(base, 0xE200, 0xE210,
                        fwk_mmio_read_32(base + 11 * 64 + 0x20));
        hierarchy_write(base, 0xE200, 0xE214,
                        fwk_mmio_read_32(base + 11 * 64 + 0x24));
        hierarchy_write(base, 0xE200, 0xE204, saved_ni[i].smen);
        hierarchy_write(base, 0xE200, 8, saved_ni[i].control);
        hierarchy_write(base, 0xE200, 0xE210, saved_ni[i].selector_low);
        hierarchy_write(base, 0xE200, 0xE214, saved_ni[i].selector_high);
    }
    for (unsigned int i = 0; i < saved_system_count; i++) {
        uintptr_t base = saved_system[i].base;
        unsigned int node = saved_system[i].node;
        hierarchy_write(base, 0x8BFC, 0x8000 + node * 8, saved_system[i].impdef);
        hierarchy_write(base, 0x8BFC, node * 64 + 8, saved_system[i].control);
    }
    teardown();
}
static const char *hierarchy_name(unsigned int case_idx)
{
    static const char *names[] = {
        "system_root_and_two_bank_aggregation", "ni_mixed_snapshot_and_three_parent_routes",
    };
    return case_idx < 2 ? names[case_idx] : NULL;
}
static const struct mod_integration_test_api hierarchy_test_api = {
    .run = run_hierarchy, .test_name = hierarchy_name,
    .setup = hierarchy_setup, .teardown = hierarchy_teardown,
};

static int bind(fwk_id_t id, unsigned int round)
{
    int status;
    if (round != 0 || !fwk_id_is_type(id, FWK_ID_TYPE_MODULE))
        return FWK_SUCCESS;
    status = fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_FMU),
        FWK_ID_API(FWK_MODULE_IDX_FMU, MOD_FMU_DEVICE_API_IDX),
        &fmu_api);
    if (status != FWK_SUCCESS)
        return status;
    return fwk_module_bind(
        alarm_id,
        FWK_ID_API(FWK_MODULE_IDX_TIMER, MOD_TIMER_API_IDX_ALARM),
        &alarm_api);
}
static int start(fwk_id_t id)
{
    if (!fwk_id_is_type(id, FWK_ID_TYPE_MODULE))
        return FWK_SUCCESS;
    return fwk_notification_subscribe(
        mod_fmu_notification_id_fault, FWK_ID_MODULE(FWK_MODULE_IDX_FMU), id);
}
static int process_bind_request(
    fwk_id_t source_id,
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void **api)
{
    if (fwk_id_get_api_idx(api_id) != MOD_INTEGRATION_TEST_API_IDX_TEST)
        return FWK_E_PARAM;
    if (fwk_id_is_type(target_id, FWK_ID_TYPE_ELEMENT)) {
        unsigned int index = fwk_id_get_element_idx(target_id);
        if (index > 1)
            return FWK_E_PARAM;
        *api = index == 0 ? &ram_test_api : &hierarchy_test_api;
    } else {
        *api = &test_api;
    }
    return FWK_SUCCESS;
}
static int init(fwk_id_t id, unsigned int count, const void *data)
{
    return FWK_SUCCESS;
}
static int element_init(
    fwk_id_t id,
    unsigned int sub_element_count,
    const void *data)
{
    unsigned int index = fwk_id_get_element_idx(id);
    if (index > 1 || sub_element_count != 0 ||
        data != (index == 0 ? &ram_test_api : &hierarchy_test_api)) {
        return FWK_E_PARAM;
    }
    return FWK_SUCCESS;
}

static const struct fwk_element ae_elements[] = {
    { .name = "ram", .data = &ram_test_api },
    { .name = "hierarchy", .data = &hierarchy_test_api },
    { 0 },
};
struct fwk_module_config config_test_ae = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(ae_elements),
};
const struct fwk_module module_test_ae = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .init = init,
    .element_init = element_init,
    .bind = bind,
    .start = start,
    .process_bind_request = process_bind_request,
    .process_notification = process_notification,
    .api_count = MOD_INTEGRATION_TEST_API_COUNT,
};
