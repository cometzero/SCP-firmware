/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fwk_module_idx.h"
#include "internal/gicx00_multiview_reg.h"

#include <Mockfwk_module.h>
#include <internal/Mockfwk_core_internal.h>
#include <unity.h>

#include <mod_gicx00_multiview.h>
#include <mod_integration_test.h>
#include <mod_power_domain.h>

#include <fwk_macros.h>
#include <fwk_status.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_REDISTRIBUTOR_COUNT 4U
#define TEST_GICR_SIZE           0x100U
#define TEST_GICD_SIZE           0x400U

static uint32_t gicr_registers[TEST_REDISTRIBUTOR_COUNT]
                              [TEST_GICR_SIZE / sizeof(uint32_t)];
static uint32_t gicd_registers[TEST_GICD_SIZE / sizeof(uint32_t)];
static unsigned int invalid_order_count;
static char log_buffer[65536];
static size_t log_length;

static bool decode_gicr_address(
    uintptr_t address,
    unsigned int *redistributor_idx,
    uintptr_t *offset)
{
    unsigned int idx;

    for (idx = 0U; idx < TEST_REDISTRIBUTOR_COUNT; idx++) {
        const uintptr_t base = (uintptr_t)gicr_registers[idx];

        if ((address >= base) && (address < (base + TEST_GICR_SIZE))) {
            *redistributor_idx = idx;
            *offset = address - base;
            return true;
        }
    }

    return false;
}

static uint32_t test_mmio_read_32(uintptr_t address)
{
    return *(uint32_t *)address;
}

static uint64_t test_mmio_read_64(uintptr_t address)
{
    return *(uint64_t *)address;
}

static void test_mmio_write_32(uintptr_t address, uint32_t value)
{
    unsigned int idx;
    uintptr_t offset;
    const uintptr_t gicd_base = (uintptr_t)gicd_registers;

    if (decode_gicr_address(address, &idx, &offset)) {
        if (offset == GICR_WAKER) {
            if ((value & GICR_WAKER_PROCESSOR_SLEEP) != 0U) {
                value |= GICR_WAKER_CHILDREN_ASLEEP;
            } else {
                value &= ~GICR_WAKER_CHILDREN_ASLEEP;
            }
        } else if (
            (offset == GICR_PWRR) && ((value & GICR_PWRR_RDPD) != 0U) &&
            ((gicr_registers[idx][GICR_WAKER / sizeof(uint32_t)] &
              GICR_WAKER_CHILDREN_ASLEEP) == 0U)) {
            invalid_order_count++;
            return;
        }

        *(uint32_t *)address = value;
        return;
    }

    if ((address >= gicd_base) && (address < (gicd_base + TEST_GICD_SIZE))) {
        offset = address - gicd_base;
        if ((offset >= 0x0200U) && (offset < 0x0280U)) {
            gicd_registers[offset / sizeof(uint32_t)] |= value;
            return;
        }
        if ((offset >= 0x0280U) && (offset < 0x0300U)) {
            gicd_registers[(offset - 0x0080U) / sizeof(uint32_t)] &= ~value;
            return;
        }
        if ((offset >= 0x0300U) && (offset < 0x0380U)) {
            gicd_registers[offset / sizeof(uint32_t)] |= value;
            return;
        }
        if ((offset >= 0x0380U) && (offset < 0x0400U)) {
            gicd_registers[(offset - 0x0080U) / sizeof(uint32_t)] &= ~value;
            return;
        }
    }

    *(uint32_t *)address = value;
}

#define FWK_MMIO_H
#define fwk_mmio_read_32  test_mmio_read_32
#define fwk_mmio_read_64  test_mmio_read_64
#define fwk_mmio_write_32 test_mmio_write_32
#include DRIVER_UNIT_TEST_SRC
#undef fwk_mmio_read_32
#undef fwk_mmio_read_64
#undef fwk_mmio_write_32

static void test_log_printf(const char *format, ...)
{
    int length;
    va_list args;

    va_start(args, format);
    length = vsnprintf(
        log_buffer + log_length, sizeof(log_buffer) - log_length, format, args);
    va_end(args);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, length);
    TEST_ASSERT_LESS_THAN(sizeof(log_buffer) - log_length, (size_t)length);
    log_length += (size_t)length;
    log_buffer[log_length++] = '\n';
    log_buffer[log_length] = '\0';
    fputs(log_buffer + log_length - (size_t)length - 1U, stdout);
}

#define fwk_log_printf test_log_printf
#include UNIT_TEST_SRC
#undef fwk_log_printf

static const struct mod_gicx00_multiview_redistributor_map
    redistributor_map[TEST_REDISTRIBUTOR_COUNT] = {
        { .gicr_base = (uintptr_t)gicr_registers[0] },
        { .gicr_base = (uintptr_t)gicr_registers[1] },
        { .gicr_base = (uintptr_t)gicr_registers[2] },
        { .gicr_base = (uintptr_t)gicr_registers[3] },
    };

static const struct mod_gicx00_multiview_config gic_config = {
    .gicd_base = (uintptr_t)gicd_registers,
    .redistributor_map = redistributor_map,
    .redistributor_map_count = FWK_ARRAY_SIZE(redistributor_map),
};

static int fake_system_shutdown(enum mod_pd_system_shutdown shutdown)
{
    TEST_ASSERT_EQUAL_INT(MOD_PD_SYSTEM_WARM_RESET, shutdown);
    memset(gicd_registers, 0, sizeof(gicd_registers));
    return FWK_PENDING;
}

static const struct mod_pd_restricted_api fake_pd_api = {
    .system_shutdown = fake_system_shutdown,
};

static bool valid_element_callback(fwk_id_t id, int call_count)
{
    return fwk_id_get_module_idx(id) == FWK_MODULE_IDX_GICX00_MULTIVIEW;
}

static const void *get_data_callback(fwk_id_t id, int call_count)
{
    if (fwk_id_get_module_idx(id) == FWK_MODULE_IDX_GICX00_MULTIVIEW) {
        return &gic_config;
    }

    return NULL;
}

static int bind_callback(
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void *api,
    int call_count)
{
    const void **api_out = (const void **)api;

    if (fwk_id_get_module_idx(target_id) == FWK_MODULE_IDX_GICX00_MULTIVIEW) {
        return gicx00_multiview_process_bind_request(
            FWK_ID_MODULE(FWK_MODULE_IDX_TEST_GIC_POWER),
            target_id,
            api_id,
            api_out);
    }
    if (fwk_id_get_module_idx(target_id) == FWK_MODULE_IDX_POWER_DOMAIN) {
        *api_out = &fake_pd_api;
        return FWK_SUCCESS;
    }

    return FWK_E_PARAM;
}

static int put_event_callback(struct fwk_event *event, int call_count)
{
    return FWK_SUCCESS;
}

void setUp(void)
{
    memset(gicr_registers, 0, sizeof(gicr_registers));
    memset(gicd_registers, 0, sizeof(gicd_registers));
    invalid_order_count = 0U;
    log_length = 0U;
    log_buffer[0] = '\0';

    fwk_module_is_valid_element_id_StubWithCallback(valid_element_callback);
    fwk_module_get_data_StubWithCallback(get_data_callback);
    fwk_module_bind_StubWithCallback(bind_callback);
    __fwk_put_event_StubWithCallback(put_event_callback);
}

void tearDown(void)
{
}

void test_bound_driver_executes_power_lifecycle_and_timeout_negative(void)
{
    int status;
    unsigned int case_idx;
    unsigned int pe;
    unsigned int phase_idx;
    unsigned int scenario_idx;
    char expected[160];
    const char *const phases[] = { "START", "READY", "DONE" };
    const char *const scenarios[] = { "pwrr", "waker", "pending-warm-reset" };
    const struct mod_integration_test_api *api;

    status = module_test_gic_power.bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_TEST_GIC_POWER), 0U);
    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&power_api, gic_power_api);

    status = module_test_gic_power.process_bind_request(
        FWK_ID_MODULE(FWK_MODULE_IDX_INTEGRATION_TEST),
        FWK_ID_MODULE(FWK_MODULE_IDX_TEST_GIC_POWER),
        FWK_ID_API(
            FWK_MODULE_IDX_TEST_GIC_POWER, MOD_INTEGRATION_TEST_API_IDX_TEST),
        (const void **)&api);
    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);

    TEST_ASSERT_EQUAL_INT(
        FWK_E_PARAM, api->run(POWER_TEST_CASE_COUNT, 0U, NULL));
    TEST_ASSERT_EQUAL_INT(FWK_E_PARAM, api->run(0U, 1U, NULL));
    TEST_ASSERT_NULL(api->test_name(POWER_TEST_CASE_COUNT));

    for (case_idx = 0U; case_idx < (POWER_TEST_CASE_COUNT - 1U); case_idx++) {
        status = api->run(case_idx, 0U, NULL);
        TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);
    }

    status = api->run(POWER_TEST_CASE_COUNT - 1U, 0U, NULL);
    TEST_ASSERT_EQUAL_INT(FWK_PENDING, status);
    status = api->run(POWER_TEST_CASE_COUNT - 1U, 1U, NULL);
    TEST_ASSERT_EQUAL_INT(FWK_SUCCESS, status);

    TEST_ASSERT_EQUAL_UINT(1U, invalid_order_count);
    for (scenario_idx = 0U; scenario_idx < FWK_ARRAY_SIZE(scenarios);
         scenario_idx++) {
        for (pe = 1U; pe <= TEST_REDISTRIBUTOR_COUNT; pe++) {
            for (phase_idx = 0U; phase_idx < FWK_ARRAY_SIZE(phases);
                 phase_idx++) {
                snprintf(
                    expected,
                    sizeof(expected),
                    "GIC720AE_POWER_%s scenario=%s pe=%u pass=1",
                    phases[phase_idx],
                    scenarios[scenario_idx],
                    pe);
                TEST_ASSERT_NOT_NULL(strstr(log_buffer, expected));
            }
        }
    }
    for (phase_idx = 0U; phase_idx < FWK_ARRAY_SIZE(phases); phase_idx++) {
        snprintf(
            expected,
            sizeof(expected),
            "GIC720AE_POWER_%s scenario=pwrr-timeout-negative pe=1 pass=1",
            phases[phase_idx]);
        TEST_ASSERT_NOT_NULL(strstr(log_buffer, expected));
    }
    TEST_ASSERT_NOT_NULL(strstr(
        log_buffer,
        "GIC720AE_POWER_NEGATIVE scenario=pwrr-timeout-negative "
        "verdict=PASS expected=FWK_E_TIMEOUT"));
}

int template_test_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bound_driver_executes_power_lifecycle_and_timeout_negative);
    return UNITY_END();
}

#if !defined(TEST_ON_TARGET)
int main(void)
{
    return template_test_main();
}
#endif
