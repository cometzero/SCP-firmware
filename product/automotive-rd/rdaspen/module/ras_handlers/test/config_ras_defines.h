/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     RAS Handler Service Test Defines.
 */

#ifndef CONFIG_RAS_DEFINES_H
#define CONFIG_RAS_DEFINES_H

#include <mod_ras_handlers.h>
#include <fwk_macros.h>

#define CLUSTER_X_BASE                 0xDCDCDCD0
#define CLUSTER_ERR_REG_MOCK(core_idx) (CLUSTER_X_BASE + (0x8 * core_idx))

#define GIC_LEVEL 0x0
#define GIC_EDGE  0x2

/*!
 * \brief RAS IP indexes.
 */
enum ras_ip_list_idx {
    RAS_CLUSTERX_IP_IDX,
    IPCOUNT,
};

/*!
 * \brief RAS IP Interrupts.
 */
enum ras_ip_intr_idx {
    TEST_RAS_CLUSTERX_INTR_IDX = 340,
};

static const uintptr_t error_records_clx[] = {
    CLUSTER_ERR_REG_MOCK(0),
    CLUSTER_ERR_REG_MOCK(1),
    CLUSTER_ERR_REG_MOCK(2),
    CLUSTER_ERR_REG_MOCK(3),
};

static const uintptr_t error_records_ip[] = {
    0x0,
    0xCDCDCDCD,
    0xCDCDCDC5,
    0xCDCDCDBD,
};

static const unsigned int cpu_clx_pe_ids[] = { 0, 1, 2, 3 };

static const struct mod_ras_isr_desc valid_intr_desc[] = {
    {
        .interrupt_no = TEST_RAS_CLUSTERX_INTR_IDX,
        .ip_type = TYPE_CPU_IP,
        .interrupt_trigger_type = GIC_LEVEL,
        .pe_ids = cpu_clx_pe_ids,
        .pe_count = FWK_ARRAY_SIZE(cpu_clx_pe_ids),
        .err_records_base = error_records_clx,
        .err_record_count = FWK_ARRAY_SIZE(error_records_clx),
        .mhu_in_base = 0xFDFDFDFEULL,
        .mhu_out_base = 0xFDFDFDFDULL,
        .mhu_channel = 0,
        .mhu_flag = 0x2,
        .mhu_poll_retries = 1000,
    },
};


#endif /* CONFIG_RAS_DEFINES_H */
