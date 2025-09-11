/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Safety Island (SI) System Control Space information used to check
 *     SI-related configuration details.
 */

#ifndef SI_SCR_INFO_H
#define SI_SCR_INFO_H

#include <fwk_mmio.h>

#include <stdbool.h>
#include <stdint.h>

#define SI_SCR_BASE                    0x2A6B0000
#define SI_SCR_SIZE                    0x10000
#define SI_SCR_SYSTEM_CFG_OFFSET       0x70
#define SI_SYSTEM_CFG_CL1_PRESENT_BIT  0U
#define SI_SYSTEM_CFG_CL1_PRESENT_MASK (1U << SI_SYSTEM_CFG_CL1_PRESENT_BIT)

static inline bool si_cl1_present(void)
{
    const uintptr_t addr = (uintptr_t)(SI_SCR_BASE + SI_SCR_SYSTEM_CFG_OFFSET);
    const uint32_t v = fwk_mmio_read_32(addr);

    return (v & SI_SYSTEM_CFG_CL1_PRESENT_MASK) != 0u;
}

#endif /* SI_SCR_INFO_H */
