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
#include "si_scr_info.h"

#include <mod_power_domain.h>
#include <mod_si0_platform.h>

#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

#include <arch_reg.h>

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

/*!
 * \brief This struct contains scr base address and expected values which is
 * verified during boot.
 */
static const scr_config_t config_scr = {
    .scr_base = SI_SCR_BASE,
    .scr_expected = {
        .cl0_config_0 = 0x01201717U,
        .cl0_config_1 = 0x01100002U,
        .cl0_config_2 = 0x00000034U,
        .cl0_c0_config_0 = 0x01000001U,
        .cl0_c0_config_1 = 0x01001000U,
        .cl0_c0_config_2 = 0x01200000U,
        .cl0_c0_config_3 = 0x00000000U,
    },
};

/* Structure for ATU region */
typedef struct atu_region_type {
    /* Region start address */
    const uint32_t *region_start_addr;
    /* Size of the ATU region */
    uint32_t size;
} atu_region_t;

/* Indices for SI ATU regions */
enum SI_ATU_REGIONS {
    SI_ATU_REGION_IDX_CMN,
    SI_ATU_REGION_IDX_CLUSTER_UTILITY,
#if (PLATFORM_VARIANT == APOLLO_FVP_VARIANT_FVP)
    SI_ATU_REGION_IDX_SMD_EXPANSION,
#endif
    SI_ATU_REGION_IDX_SYSTOP_PIK,
    SI_ATU_REGION_IDX_SYSTEM_ID,
    SI_ATU_REGION_IDX_CSS_COUNTERS_TIMERS,
    SI_ATU_REGION_IDX_NI710AE_CLUSTER0_FMU,
    SI_ATU_REGION_IDX_NI710AE_CLUSTER1_FMU,
    SI_ATU_REGION_IDX_NI710AE_CLUSTER2_FMU,
    SI_ATU_REGION_IDX_NI710AE_CLUSTER3_FMU,
    SI_ATU_REGION_IDX_NI710AE_SYS_CTRL,
    SI_ATU_REGION_IDX_NI710AE_SMD,
    SI_ATU_REGION_IDX_AP_GIC,
    SI_ATU_REGION_IDX_SHARED_SRAM,
    SI_ATU_REGION_IDX_SHARED_SRAM_NS,
#if (PLATFORM_VARIANT == APOLLO_FVP_VARIANT_FVP)
    SI_ATU_REGION_IDX_SMD_SMCF_MGI,
#endif
    SI_ATU_REGION_IDX_SMD_SRAM,
    SI_ATU_REGION_COUNT,
};

/* These sizes are the accessible ranges checked by the ATU self-check. */
static const atu_region_t si_atu_regions[SI_ATU_REGION_COUNT] = {
    [SI_ATU_REGION_IDX_CMN] = {
        .region_start_addr = (const uint32_t*)0x80000000UL,
        .size = 0x10000UL,
    },
    [SI_ATU_REGION_IDX_CLUSTER_UTILITY] = {
        .region_start_addr = (const uint32_t*)0xC1000000UL,
        .size = 0x800000UL,
    },
#if (PLATFORM_VARIANT == APOLLO_FVP_VARIANT_FVP)
    [SI_ATU_REGION_IDX_SMD_EXPANSION] = {
        .region_start_addr = (const uint32_t*)0xD0000000UL,
        .size = 0x20000UL,
    },
#endif
    [SI_ATU_REGION_IDX_SYSTOP_PIK] = {
        .region_start_addr = (const uint32_t*)0xD0020000UL,
        .size = 0x2000UL,
    },
    [SI_ATU_REGION_IDX_SYSTEM_ID] = {
        .region_start_addr = (const uint32_t*)0xD0030000UL,
        .size = 0x10000UL,
    },
    [SI_ATU_REGION_IDX_CSS_COUNTERS_TIMERS] = {
        .region_start_addr = (const uint32_t*)0xD0040000UL,
        .size = 0x30000UL,
    },
    [SI_ATU_REGION_IDX_NI710AE_CLUSTER0_FMU] = {
        .region_start_addr = (const uint32_t*)0xD0070000UL,
        .size = 0x10000UL,
    },
    [SI_ATU_REGION_IDX_NI710AE_CLUSTER1_FMU] = {
        .region_start_addr = (const uint32_t*)0xD0170000UL,
        .size = 0x10000UL,
    },
    [SI_ATU_REGION_IDX_NI710AE_CLUSTER2_FMU] = {
        .region_start_addr = (const uint32_t*)0xD0270000UL,
        .size = 0x10000UL,
    },
    [SI_ATU_REGION_IDX_NI710AE_CLUSTER3_FMU] = {
        .region_start_addr = (const uint32_t*)0xD0370000UL,
        .size = 0x10000UL,
    },
    [SI_ATU_REGION_IDX_NI710AE_SYS_CTRL] = {
        .region_start_addr = (const uint32_t*)0xD0470000UL,
        .size = 0x10000UL,
    },
    [SI_ATU_REGION_IDX_NI710AE_SMD] = {
        .region_start_addr = (const uint32_t*)0xD0670000UL,
        .size = 0x10000UL,
    },
    [SI_ATU_REGION_IDX_AP_GIC] = {
        .region_start_addr = (const uint32_t*)0xD0770000UL,
        .size = 0x80000UL,
    },
    [SI_ATU_REGION_IDX_SHARED_SRAM] = {
        .region_start_addr = (const uint32_t*)0xE0030000UL,
        .size = 0x100000UL,
    },
    [SI_ATU_REGION_IDX_SHARED_SRAM_NS] = {
        .region_start_addr = (const uint32_t*)0xE0130000UL,
        .size = 0x100000UL,
    },
#if (PLATFORM_VARIANT == APOLLO_FVP_VARIANT_FVP)
    [SI_ATU_REGION_IDX_SMD_SMCF_MGI] = {
        .region_start_addr = (const uint32_t*)0xE0230000UL,
        .size = 0x10000UL,
    },
#endif
    [SI_ATU_REGION_IDX_SMD_SRAM] = {
        .region_start_addr = (const uint32_t*)0xE0240000UL,
        .size = 0x100000UL,
    },
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

static int verify_scr_cfg_integrity(void)
{
    volatile const scr_t *const scr =
        (volatile const scr_t *const)(config_scr.scr_base);

    if ((scr->cl0_config_0 != config_scr.scr_expected.cl0_config_0) ||
        (scr->cl0_config_1 != config_scr.scr_expected.cl0_config_1) ||
        (scr->cl0_config_2 != config_scr.scr_expected.cl0_config_2) ||
        (scr->cl0_c0_config_0 != config_scr.scr_expected.cl0_c0_config_0) ||
        (scr->cl0_c0_config_1 != config_scr.scr_expected.cl0_c0_config_1) ||
        (scr->cl0_c0_config_2 != config_scr.scr_expected.cl0_c0_config_2) ||
        (scr->cl0_c0_config_3 != config_scr.scr_expected.cl0_c0_config_3)) {
        return FWK_E_PANIC;
    }
    return FWK_SUCCESS;
}

static void verify_atu_cfg(void)
{
    uint32_t value;

    /*
     * Read the start and end address of each regions.
     * Expect an abort if the region configuration is not as expected.
     */
    for (uint32_t i = 0; i < SI_ATU_REGION_COUNT; ++i) {
        const volatile uint32_t *region_start_address =
            (volatile uint32_t *)si_atu_regions[i].region_start_addr;
        const volatile uint32_t *region_end_address =
            (volatile uint32_t
                 *)(si_atu_regions[i].region_start_addr + ((si_atu_regions[i].size - 4) / 4U));

        value = *(region_start_address);
        value = *(region_end_address);
        (void)value;
    }
}

int pd_transition_ap_platform_hook(unsigned int pd_state)
{
    static bool is_atu_check_done = false;

    switch (pd_state) {
    case (unsigned int)MOD_PD_STATE_OFF:
    case (unsigned int)MOD_PD_STATE_OFF_0:
    case (unsigned int)MOD_PD_STATE_OFF_1:
    case (unsigned int)MOD_PD_STATE_OFF_2:
    case (unsigned int)MOD_PD_STATE_SLEEP:
        /* do nothing */
        break;
    case (unsigned int)MOD_PD_STATE_ON:
        /* Perform ATU cfg check once during boot */
        if (is_atu_check_done == false) {
            FWK_LOG_INFO(
                "[SI0-PLATFORM] AP domain has been turned on, performing ATU "
                "cfg check");
            is_atu_check_done = true;
            /* Verify ATU config integrity. Any mismatch will result in abort */
            verify_atu_cfg();
        }
        break;
    default:
        /* Unsupported Power Domain state, do nothing */
        break;
    }

    return FWK_SUCCESS;
}

int platform_init_hook(void *params)
{
    int status = FWK_SUCCESS;

#ifdef BUILD_HAS_IMAGE_INTEGRITY_CHECK
    /* Verify the integrity of image. */
    if (FWK_SUCCESS != verify_image_crc()) {
        status = FWK_E_PANIC;
    }
#endif

    /* Disable MPU for SCR access*/
    WRITE_SYSREG(sctlr_el2, ((READ_SYSREG(sctlr_el2)) & ~SCTLR_EL2_M));
    BARRIER_DSYNC_FENCE_FULL();
    BARRIER_ISYNC_FENCE_FULL();

    /* Verify SCR config integrity. */
    if (FWK_SUCCESS != verify_scr_cfg_integrity()) {
        status = FWK_E_PANIC;
    }

    /* Enable MPU */
    WRITE_SYSREG(sctlr_el2, ((READ_SYSREG(sctlr_el2)) | SCTLR_EL2_M));
    BARRIER_DSYNC_FENCE_FULL();
    BARRIER_ISYNC_FENCE_FULL();

    return status;
}

struct fwk_module_config config_si0_platform = {
    .data = &system_config,
};
