/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'si0_platform'.
 */

#include "si0_cfgd_transport.h"
#include "si0_mmap.h"

#include <mod_si0_platform.h>

#include <fwk_id.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

#define RSE_SYNC_WAIT_TIMEOUT_US (800 * 1000)

struct mod_si0_platform_config system_config = {
    .primary_cpu_mpid = 0,
    .isolated_cpu_info = { .isolated_cpu_mpid_list = NULL,
                           .isolated_cpu_count = 0 },
    .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
    .rse_sync_wait_us = RSE_SYNC_WAIT_TIMEOUT_US,
    .transport_id = FWK_ID_ELEMENT_INIT(
        FWK_MODULE_IDX_TRANSPORT,
        SI0_CFGD_MOD_TRANSPORT_EIDX_RSE_WARM_SYNC),
};

#ifdef BUILD_HAS_IMAGE_INTEGRITY_CHECK

FWK_IMPORT_SYM(unsigned long, __imageCRC_start__, IMAGE_CRC_START);

/*!
 * \brief This is a placeholder for appending the image CRC.
 * This is used to verify the integrity failures of image after loading.
 */
static volatile uint32_t image_crc_placeholder FWK_SECTION(".image_crc") =
    0xFFFFFFFF;

/*!
 * \brief Function to calculate CRC32 using the lookup table
 * polynomial 0xedb88320 and Initial crc value 0xFFFFFFFF.
 */
static uint32_t calculate_crc32(const uint8_t *data, uint32_t length)
{
    /* Initial CRC value */
    uint32_t crc = 0xFFFFFFFFUL;

    /* crc table generated from polynomial 0xedb88320 */
    static const uint32_t table[16] = {
        0x00000000U, 0x1db71064U, 0x3b6e20c8U, 0x26d930acU,
        0x76dc4190U, 0x6b6b51f4U, 0x4db26158U, 0x5005713cU,
        0xedb88320U, 0xf00f9344U, 0xd6d6a3e8U, 0xcb61b38cU,
        0x9b64c2b0U, 0x86d3d2d4U, 0xa00ae278U, 0xbdbdf21cU,
    };

    for (uint32_t i = 0; i < length; ++i) {
        uint8_t byte = data[i];

        crc = (crc >> 4) ^ table[(crc ^ byte) & 0x0f];
        crc = (crc >> 4) ^ table[(crc ^ ((uint32_t)byte >> 4)) & 0x0f];
    }
    /* Final XOR with 0xFFFFFFFF */
    return (~crc);
}

static int verify_image_crc(void)
{
    /*
     * Start address and length of the image can be obtained from linker symbol
     */
    if ((calculate_crc32(
            (uint8_t *)SI0_SRAM_BASE, (IMAGE_CRC_START - SI0_SRAM_BASE))) !=
        image_crc_placeholder) {
        return FWK_E_PANIC;
    }
    return FWK_SUCCESS;
}
#endif

int platform_init_hook(void *params)
{
    int status = FWK_SUCCESS;

#ifdef BUILD_HAS_IMAGE_INTEGRITY_CHECK
    /* Verify the integrity of image. */
    if (FWK_SUCCESS != verify_image_crc()) {
        status = FWK_E_PANIC;
    }
#endif

    return status;
}

struct fwk_module_config config_si0_platform = {
    .data = &system_config,
};
