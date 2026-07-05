/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Cluster control registers
 */

#ifndef CLUSTER_CONTROL_REG_H
#define CLUSTER_CONTROL_REG_H

#include <fwk_macros.h>

#include <stdint.h>

// clang-format off
struct cluster_control_reg {
   FWK_RW  uint32_t  CFGEND;
   FWK_RW  uint32_t  BROADCAST;
           uint8_t   RESERVED[0x034-0x008];
   FWK_RW  uint32_t  ASTART0;
   FWK_RW  uint32_t  AEND0;
   FWK_RW  uint32_t  ASTART1;
   FWK_RW  uint32_t  AEND1;
   FWK_RW  uint32_t  ASTART2;
   FWK_RW  uint32_t  AEND2;
   FWK_RW  uint32_t  ASTART3;
   FWK_RW  uint32_t  AEND3;
           uint8_t   RESERVED2[0x100-0x054];
   FWK_RW  uint32_t  PE0_RVBARADDR_LW;
   FWK_RW  uint32_t  PE0_RVBARADDR_UP;
   FWK_RW  uint32_t  PE1_RVBARADDR_LW;
   FWK_RW  uint32_t  PE1_RVBARADDR_UP;
   FWK_RW  uint32_t  PE2_RVBARADDR_LW;
   FWK_RW  uint32_t  PE2_RVBARADDR_UP;
   FWK_RW  uint32_t  PE3_RVBARADDR_LW;
   FWK_RW  uint32_t  PE3_RVBARADDR_UP;
           uint8_t   RESERVED3[0xfd0-0x120];
   FWK_R   uint32_t  PID4;
   FWK_R   uint32_t  PID5;
   FWK_R   uint32_t  PID6;
   FWK_R   uint32_t  PID7;
   FWK_R   uint32_t  PID0;
   FWK_R   uint32_t  PID1;
   FWK_R   uint32_t  PID2;
   FWK_R   uint32_t  PID3;
   FWK_R   uint32_t  ID0;
   FWK_R   uint32_t  ID1;
   FWK_R   uint32_t  ID2;
   FWK_R   uint32_t  ID3;
};
// clang-format on

#endif /* CLUSTER_CONTROL_REG_H */
