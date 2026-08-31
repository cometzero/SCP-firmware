/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "scp_unity.h"
#include "unity.h"

#include <fwk_macros.h>
#include <fwk_status.h>

#include <stdint.h>
#include <string.h>

#include UNIT_TEST_SRC

#define TEST_ECAM_BASE UINT64_C(0x10000000)

static uint32_t bar_values[DEVICE_BAR_COUNT];
static unsigned int bar_write_count;
static bool endpoint_present;

static uint8_t test_read8(uint64_t address)
{
    if (address == (TEST_ECAM_BASE + PCIE_HEADER_TYPE_OFFSET)) {
        return PCIE_HEADER_TYPE_ENDPOINT;
    }

    return 0;
}

static void test_write8(uint64_t address, uint8_t value)
{
}

static uint32_t test_read32(uint64_t address)
{
    uint64_t offset = address - TEST_ECAM_BASE;

    if (offset == 0) {
        return endpoint_present ? UINT32_C(0x12348086) : UINT32_C(0xFFFFFFFF);
    }

    if ((offset >= PCIE_BAR_OFFSET) &&
        (offset < (PCIE_BAR_OFFSET + sizeof(bar_values)))) {
        return bar_values[(offset - PCIE_BAR_OFFSET) / PCIE_BAR_SIZE];
    }

    return UINT32_C(0xFFFFFFFF);
}

static void test_write32(uint64_t address, uint32_t value)
{
    uint64_t offset = address - TEST_ECAM_BASE;

    if ((offset >= PCIE_BAR_OFFSET) &&
        (offset < (PCIE_BAR_OFFSET + sizeof(bar_values)))) {
        TEST_ASSERT_EQUAL_HEX32(UINT32_C(0xFFFFFFFF), value);
        bar_write_count++;
    }
}

static struct pcie_discovery_rw_api test_rw_api = {
    .read8 = test_read8,
    .write8 = test_write8,
    .read32 = test_read32,
    .write32 = test_write32,
};

void setUp(void)
{
    memset(bar_values, 0, sizeof(bar_values));
    bar_write_count = 0;
    endpoint_present = true;
}

void tearDown(void)
{
}

void test_valid_vid_discovers_bus_and_sizes_32_bit_bar(void)
{
    struct pcie_mmap_size mmap_size = { 0 };
    int status;

    bar_values[0] = UINT32_C(0xFFFFF000);

    status = calculate_resource(TEST_ECAM_BASE, &mmap_size, 0, &test_rw_api);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL_UINT64(FWK_MIB, mmap_size.ecam);
    TEST_ASSERT_EQUAL_UINT64(FWK_MIB, mmap_size.mmiol);
    TEST_ASSERT_EQUAL_UINT64(0, mmap_size.mmioh);
    TEST_ASSERT_EQUAL_UINT64(1, mmap_size.bus);
    TEST_ASSERT_EQUAL_UINT(DEVICE_BAR_COUNT, bar_write_count);
}

void test_invalid_vid_skips_bar_probe(void)
{
    struct pcie_mmap_size mmap_size = { 0 };
    int status;

    endpoint_present = false;
    status = calculate_resource(TEST_ECAM_BASE, &mmap_size, 7, &test_rw_api);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL_UINT64(FWK_MIB, mmap_size.ecam);
    TEST_ASSERT_EQUAL_UINT64(0, mmap_size.mmiol);
    TEST_ASSERT_EQUAL_UINT64(0, mmap_size.mmioh);
    TEST_ASSERT_EQUAL_UINT64(1, mmap_size.bus);
    TEST_ASSERT_EQUAL_UINT(0, bar_write_count);
}

void test_bar_sizing_accumulates_32_and_64_bit_resources(void)
{
    struct pcie_mmap_size mmap_size = { 0 };

    bar_values[0] = UINT32_C(0xFFFFF000);
    bar_values[1] = UINT32_C(0xFFE00004);
    bar_values[2] = UINT32_C(0xFFFFFFFF);

    get_mmio_memory_size(
        TEST_ECAM_BASE, &mmap_size, &test_rw_api, DEVICE_BAR_COUNT);

    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0x1000), mmap_size.mmiol);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0x200000), mmap_size.mmioh);
    TEST_ASSERT_EQUAL_UINT(DEVICE_BAR_COUNT, bar_write_count);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_valid_vid_discovers_bus_and_sizes_32_bit_bar);
    RUN_TEST(test_invalid_vid_skips_bar_probe);
    RUN_TEST(test_bar_sizing_accumulates_32_and_64_bit_resources);

    return UNITY_END();
}
