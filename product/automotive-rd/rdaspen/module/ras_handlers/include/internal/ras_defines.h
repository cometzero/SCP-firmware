/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef RAS_DEFINES_H
#define RAS_DEFINES_H

#include <fwk_macros.h>

#include <stdint.h>

#define MOD_NAME            "[RAS_FFH_SERVICE]"
#define CPU_HANDLE_MOD_NAME "[AP_RAS_CPU_INT]"

#define ERX_STATUS_V  (1UL << 30)
#define ERX_STATUS_UC (1UL << 29)
#define ERX_STATUS_CE (1UL << 25)
#define ERX_STATUS_DE (1UL << 23)

/* RAS Error status register IERR value */
#define ERX_STATUS_IERR(err_status) (((err_status) >> 8UL) & 0x1FUL)
/* RAS Error status register SERR value */
#define ERX_STATUS_SERR(err_status) ((err_status)&0x1FUL)

#define TFP_ERROR_SERR          (0x1AUL)
#define TFP_ERROR_STRING_OFFSET (0x4UL)
#define TFP_ERROR_SOURCES_COUNT (0xDUL)

// clang-format off
struct ext_cpu_ras_cluster_regs {
    FWK_R   uint64_t  ERRXFR;
    FWK_RW  uint64_t  ERRXCTLR;
    FWK_RW  uint64_t  ERRXSTATUS;
    FWK_RW  uint64_t  ERRXADDR;
    FWK_RW  uint64_t  ERRXMISC0;
    FWK_RW  uint64_t  ERRXMISC1;
    FWK_RW  uint64_t  ERRXMISC2;
    FWK_RW  uint64_t  ERRXMISC3;
    FWK_R   uint64_t  ERRXPFGF;
    FWK_RW  uint64_t  ERRXPFGCTL;
    FWK_RW  uint64_t  ERRXPFGCDN;
    FWK_R   uint64_t  ERRGSR;
    FWK_R   uint64_t  ERRIIDR;
    FWK_R   uint64_t  ERRDEVAFF;
    FWK_R   uint64_t  ERRDEVARCH;
    FWK_R   uint64_t  ERRDEVID;
    FWK_R   uint64_t  ERRPIDR4;
    FWK_R   uint64_t  ERRPIDR0;
    FWK_R   uint64_t  ERRPIDR1;
    FWK_R   uint64_t  ERRPIDR2;
    FWK_R   uint64_t  ERRPIDR3;
    FWK_R   uint64_t  ERRCIDR0;
    FWK_R   uint64_t  ERRCIDR1;
    FWK_R   uint64_t  ERRCIDR2;
    FWK_R   uint64_t  ERRCIDR3;
};
// clang-format on

#endif /* RAS_DEFINES_H */
