/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Safety Island (SI) System Control Space information used to check
 *     SI-related configuration details.
 */

#ifndef SI_SCR_INFO_H
#define SI_SCR_INFO_H

#include <fwk_macros.h>
#include <fwk_mmio.h>

#include <stdbool.h>
#include <stdint.h>

#define SI_SCR_BASE                    0x2A6B0000
#define SI_SCR_SIZE                    0x10000
#define SI_SCR_SYSTEM_CFG_OFFSET       0x70
#define SI_SYSTEM_CFG_CL1_PRESENT_BIT  0U
#define SI_SYSTEM_CFG_CL1_PRESENT_MASK (1U << SI_SYSTEM_CFG_CL1_PRESENT_BIT)

typedef union system_cfg_type {
    struct {
        uint32_t cl1_present : 1;
        uint32_t reserved0 : 3;
        uint32_t lm0_size : 4;
        uint32_t lm1_size : 4;
        uint32_t reserved1 : 20;
    } bits;
    uint32_t byte;
} system_cfg_t;

typedef union cpuhalt_type {
    struct {
        uint32_t cl0_c0_cpuhalt : 1;
        uint32_t reserved0 : 7;
        uint32_t cl1_c0_cpuhalt : 1;
        uint32_t cl1_c1_cpuhalt : 1;
        uint32_t cl1_c2_cpuhalt : 1;
        uint32_t cl1_c3_cpuhalt : 1;
        uint32_t reserved1 : 20;
    } bits;
    uint32_t byte;
} cpuhalt_t;

typedef struct scr_type {
    FWK_RW uint32_t cl0_config_0; /* 0x0 */
    FWK_RW uint32_t cl0_config_1; /* 0x4 */
    FWK_RW uint32_t cl0_config_2; /* 0x8 */
    const uint8_t reserved0[4]; /* reserved 4 bytes */
    FWK_RW uint32_t cl0_c0_config_0; /* 0x10 */
    FWK_RW uint32_t cl0_c0_config_1; /* 0x14 */
    FWK_RW uint32_t cl0_c0_config_2; /* 0x18 */
    FWK_RW uint32_t cl0_c0_config_3; /* 0x1C */
    const uint8_t reserved1[32]; /* reserved 32 bytes */
    FWK_R uint32_t sid_system_id; /* 0x40 */
    const uint8_t reserved2[12]; /* reserved 12 bytes */
    FWK_R uint32_t sid_soc_id; /* 0x50 */
    const uint8_t reserved3[12]; /* reserved 12 bytes */
    FWK_R uint32_t sid_chip_id; /* 0x60 */
    const uint8_t reserved4[12]; /* reserved 12 bytes */
    FWK_R system_cfg_t sid_system_cfg; /* 0x70 */
    const uint8_t reserved5[652]; /* reserved 652 bytes */
    FWK_RW cpuhalt_t cpuhalt; /* 0x300 */
    const uint8_t reserved6[764]; /* reserved 764 bytes */
    FWK_RW uint32_t safectlr; /* 0x600 */
    const uint8_t reserved7[2556]; /* reserved 2556 bytes */
} scr_t;

/*!
 * \brief SCR configuration.
 */
typedef struct scr_config {
    /*! Base address of the system config register. */
    uintptr_t scr_base;

    /*! Expected values of the SCR config to verify during boot */
    const scr_t scr_expected;
} scr_config_t;

static inline bool si_cl1_present(void)
{
    const uintptr_t addr = (uintptr_t)(SI_SCR_BASE + SI_SCR_SYSTEM_CFG_OFFSET);
    const uint32_t v = fwk_mmio_read_32(addr);

    return (v & SI_SYSTEM_CFG_CL1_PRESENT_MASK) != 0u;
}

#endif /* SI_SCR_INFO_H */
