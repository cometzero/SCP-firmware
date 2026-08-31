/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "scp_unity.h"
#include "unity.h"

#include <Mockfwk_mm.h>
#include <Mockfwk_notification.h>

#include <fwk_status.h>

#include <stdint.h>
#include <string.h>

#include UNIT_TEST_SRC

#define TEST_ECAM_BASE  UINT64_C(0x10000000)
#define TEST_MMIOL_BASE UINT64_C(0x80000000)
#define TEST_MMIOH_BASE UINT64_C(0x100000000)
#define TEST_ECAM_SIZE  FWK_MIB
#define TEST_MMIOL_SIZE (2 * FWK_MIB)
#define TEST_MMIOH_SIZE (4 * FWK_MIB)

enum io_operation {
    IO_OPERATION_MAP,
    IO_OPERATION_UNMAP,
};

struct io_event {
    enum io_operation operation;
    enum interface_io_block_carveout_type type;
    uint64_t base;
    uint64_t size;
    uint8_t region;
};

static struct io_event io_events[8];
static unsigned int io_event_count;
static unsigned int io_map_count;
static unsigned int io_map_failure_call;
static unsigned int cmn_map_count;
static unsigned int sds_write_count;
static uint64_t interrupt_ids[] = { 49 };
static struct mod_pcie_setup_ep_config endpoint_config = {
    .valid = true,
    .rp_node_id = 7,
};
static struct mod_pcie_setup_config setup_config = {
    .reg_base = UINT64_C(0x20000000),
    .cmn_node_id = 3,
    .nci_source_node_id = 5,
    .block_id = 1,
    .sds_struct_id = 9,
    .smmu_base = UINT64_C(0x30000000),
    .ep_config = &endpoint_config,
};
static struct mod_system_info system_info = {
    .chip_id = 0,
};
static const struct mod_pcie_setup_resource_info resource_info = {
    .translation = 0,
    .mmap = {
        .ecam = { .start = TEST_ECAM_BASE, .size = 256 * FWK_MIB },
        .mmiol = { .start = TEST_MMIOL_BASE, .size = 256 * FWK_MIB },
        .mmioh = { .start = TEST_MMIOH_BASE, .size = 256 * FWK_MIB },
        .bus = { .start = 0, .size = 256 },
    },
    .ep_count = 1,
    .ep_interrupt_ids = interrupt_ids,
};
static uint8_t block_storage
    [sizeof(struct mod_pcie_setup_block_mmap) +
     sizeof(struct mod_pcie_setup_mmap)];

static int test_io_map(struct interface_io_block_setup_mmap *mmap)
{
    struct interface_io_block_carveout_info *carveout = mmap->carveout_info;
    struct io_event *event = &io_events[io_event_count++];

    event->operation = IO_OPERATION_MAP;
    event->type = carveout->carveout_type;
    event->base = carveout->base;
    event->size = carveout->size;
    event->region = io_map_count;
    carveout->region_id = io_map_count;
    io_map_count++;

    if (io_map_count == io_map_failure_call) {
        return FWK_E_DEVICE;
    }

    return FWK_SUCCESS;
}

static int test_io_unmap(struct interface_io_block_setup_mmap *mmap)
{
    struct interface_io_block_carveout_info *carveout = mmap->carveout_info;
    struct io_event *event = &io_events[io_event_count++];

    event->operation = IO_OPERATION_UNMAP;
    event->region = carveout->region_id;

    return FWK_SUCCESS;
}

static int test_discover(
    uint64_t ecam_base,
    struct pcie_mmap_size *mmap_size,
    uint8_t primary_bus,
    struct pcie_discovery_rw_api *rw_api)
{
    mmap_size->ecam = TEST_ECAM_SIZE;
    mmap_size->mmiol = TEST_MMIOL_SIZE;
    mmap_size->mmioh = TEST_MMIOH_SIZE;
    mmap_size->bus = 1;

    return FWK_SUCCESS;
}

static int test_cmn_map(uint64_t base, size_t size, uint32_t node_id)
{
    cmn_map_count++;
    return FWK_SUCCESS;
}

static int test_sds_write(
    uint32_t structure_id,
    unsigned int offset,
    const void *data,
    size_t size)
{
    sds_write_count++;
    return FWK_SUCCESS;
}

static uint8_t test_read8(uint64_t address)
{
    return 0;
}

static uint32_t test_read32(uint64_t address)
{
    return 0;
}

static void test_write8(uint64_t address, uint8_t value)
{
}

static void test_write32(uint64_t address, uint32_t value)
{
}

static struct interface_io_block_memmap_api io_api = {
    .map_region = test_io_map,
    .unmap_region = test_io_unmap,
};
static struct mod_pcie_discovery_api discovery_api = {
    .calculate_resource = test_discover,
};
static struct interface_cmn_memmap_rnsam_api cmn_api = {
    .map_io_region = test_cmn_map,
};
static struct mod_sds_api sds_api = {
    .struct_write = test_sds_write,
};
static struct interface_address_remapper_rw_api remapper_api = {
    .read8 = test_read8,
    .read32 = test_read32,
    .write8 = test_write8,
    .write32 = test_write32,
};

void setUp(void)
{
    memset(&pcie_setup_context, 0, sizeof(pcie_setup_context));
    memset(io_events, 0, sizeof(io_events));
    memset(block_storage, 0, sizeof(block_storage));
    io_event_count = 0;
    io_map_count = 0;
    io_map_failure_call = 0;
    cmn_map_count = 0;
    sds_write_count = 0;

    memcpy(
        &pcie_setup_context.resource_info,
        &resource_info,
        sizeof(pcie_setup_context.resource_info));
    pcie_setup_context.pcie_discovery_api = &discovery_api;
    pcie_setup_context.io_block_memmap_api = &io_api;
    pcie_setup_context.cmn_memmap_rnsam_api = &cmn_api;
    pcie_setup_context.sds_api = &sds_api;
    pcie_setup_context.remapper_rw_api = &remapper_api;
    pcie_setup_context.system_info = &system_info;
    pcie_setup_context.block_count = 1;
    pcie_setup_context.block_config_size = sizeof(block_storage);

    Mockfwk_mm_Init();
    Mockfwk_notification_Init();
}

void tearDown(void)
{
    Mockfwk_mm_Verify();
    Mockfwk_mm_Destroy();
    Mockfwk_notification_Verify();
    Mockfwk_notification_Destroy();
}

void test_maps_ecam_mmiol_mmioh_after_temporary_ecam_unmap(void)
{
    struct mod_pcie_setup_block_mmap *block_mmap =
        (struct mod_pcie_setup_block_mmap *)block_storage;
    int status;

    status = configure_pcie_endpoint(&setup_config, block_mmap);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
    TEST_ASSERT_EQUAL_UINT(5, io_event_count);
    TEST_ASSERT_EQUAL(IO_OPERATION_MAP, io_events[0].operation);
    TEST_ASSERT_EQUAL(INTERFACE_IO_BLOCK_CARVEOUT_TYPE_ECAM, io_events[0].type);
    TEST_ASSERT_EQUAL(IO_OPERATION_UNMAP, io_events[1].operation);
    TEST_ASSERT_EQUAL_UINT8(0, io_events[1].region);
    TEST_ASSERT_EQUAL(INTERFACE_IO_BLOCK_CARVEOUT_TYPE_ECAM, io_events[2].type);
    TEST_ASSERT_EQUAL_UINT64(TEST_ECAM_SIZE, io_events[2].size);
    TEST_ASSERT_EQUAL(
        INTERFACE_IO_BLOCK_CARVEOUT_TYPE_MMIOL, io_events[3].type);
    TEST_ASSERT_EQUAL_UINT64(TEST_MMIOL_SIZE, io_events[3].size);
    TEST_ASSERT_EQUAL(
        INTERFACE_IO_BLOCK_CARVEOUT_TYPE_MMIOH, io_events[4].type);
    TEST_ASSERT_EQUAL_UINT64(TEST_MMIOH_SIZE, io_events[4].size);
    TEST_ASSERT_EQUAL_UINT(4, cmn_map_count);
    TEST_ASSERT_EQUAL_UINT(3, sds_write_count);
}

void test_mapping_failure_propagates_without_notification(void)
{
    int status;

    io_map_failure_call = 2;
    fwk_mm_calloc_ExpectAndReturn(sizeof(block_storage), 1, block_storage);
    fwk_mm_free_Expect(block_storage);

    status = configure_pcie_ecam_mmio_space(&setup_config, 0);

    TEST_ASSERT_EQUAL(FWK_E_DEVICE, status);
    TEST_ASSERT_EQUAL_UINT(3, io_event_count);
    TEST_ASSERT_EQUAL_UINT(2, io_map_count);
}

void test_success_sends_initialised_notification(void)
{
    int status;

    fwk_mm_calloc_ExpectAndReturn(sizeof(block_storage), 1, block_storage);
    fwk_mm_free_Expect(block_storage);
    fwk_notification_notify_ExpectAnyArgsAndReturn(FWK_SUCCESS);

    status = configure_pcie_ecam_mmio_space(&setup_config, 0);

    TEST_ASSERT_EQUAL(FWK_SUCCESS, status);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_maps_ecam_mmiol_mmioh_after_temporary_ecam_unmap);
    RUN_TEST(test_mapping_failure_propagates_without_notification);
    RUN_TEST(test_success_sends_initialised_notification);

    return UNITY_END();
}
