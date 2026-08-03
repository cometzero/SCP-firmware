/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef INTERNAL_GICX00_MULTIVIEW_REG_H
#define INTERNAL_GICX00_MULTIVIEW_REG_H

#define GICD_CTLR                0x0000u
#define GICX00_GICD_ISPENDR(N)   (0x0200u + (4u * (N)))
#define GICX00_GICD_ICPENDR(N)   (0x0280u + (4u * (N)))
#define GICX00_GICD_ISACTIVER(N) (0x0300u + (4u * (N)))
#define GICX00_GICD_ICACTIVER(N) (0x0380u + (4u * (N)))
#define GICD_CFGID               0xF000u
#define GICD_IVIEWR(N)           (0xF600u + (4 * (N)))

#define GICR_WAKER 0x0014u
#define GICR_PWRR  0x0024u
#define GICR_VIEWR 0x002Cu

#define GICR_VIEWR_MASK 0x3u

#define GICD_CFGID_VIEW FWK_BIT_64(53)

#define GICD_CTLR_ENABLE_GROUP_0   FWK_BIT(0)
#define GICD_CTLR_ENABLE_GROUP_1NS FWK_BIT(1)
#define GICD_CTLR_ENABLE_GROUP_1S  FWK_BIT(2)

#define GICR_WAKER_PROCESSOR_SLEEP FWK_BIT(1)
#define GICR_WAKER_CHILDREN_ASLEEP FWK_BIT(2)

#define GICR_PWRR_RDPD  FWK_BIT(0)
#define GICR_PWRR_RDGPD FWK_BIT(2)
#define GICR_PWRR_RDGPO FWK_BIT(3)

#define INTERRUPT_ID_SPI_MIN   32
#define INTERRUPT_ID_SPI_LIMIT 992

#endif /* INTERNAL_GICX00_MULTIVIEW_REG_H */
