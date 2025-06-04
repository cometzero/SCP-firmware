/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #include "si0_mmap.h"
 #include "platform_core.h"

#include <mod_ras_handlers.h>

#include <fwk_element.h>
#include <fwk_id.h>
#include <fwk_module.h>

/*
 * These are Core Fault SPIs on SI0 that trigger
 * with Core Fault PPI from AP
 */
#define CLUSTER0_FAULT_INT 324
#define CLUSTER1_FAULT_INT 326
#define CLUSTER2_FAULT_INT 328
#define CLUSTER3_FAULT_INT 330

/* RAS MHU Sync based constants */
#define RAS_SYNC_FLAG                  0x2u
#define RAS_SYNC_CHANNEL               0x0u
#define RAS_MAX_RETRIES                100000

#define CPU_RAS_ERR_RECORD_REG_ADDR(core_idx) \
    (SI0_ATW1_CLUSTER_UTILITY_BASE + (core_idx * SI0_CORE_REG_UTILITY_SIZE) + \
     SI0_CLUSTER_UTILITY_CORE0_RAS_REG_OFFSET)

/*
 * RAS Components
 */
enum ras_ip_idx {
    CPU_CL0 = 0,
    CPU_CL1,
    CPU_CL2,
    CPU_CL3,
    COMPONENT_END,
};

//Make this a formal struct with core number and the respective Err Record
/* Cluster to Core Mapings for Interrupt configuration */
static const unsigned int cpu_cl0_pe_ids[] = { 0, 1, 2, 3 };
static const unsigned int cpu_cl1_pe_ids[] = { 4, 5, 6, 7 };
static const unsigned int cpu_cl2_pe_ids[] = { 8, 9, 10, 11 };
static const unsigned int cpu_cl3_pe_ids[] = { 12, 13, 14, 15 };

static const uintptr_t error_records_cl0[] = {
    CPU_RAS_ERR_RECORD_REG_ADDR(0),
    CPU_RAS_ERR_RECORD_REG_ADDR(1),
    CPU_RAS_ERR_RECORD_REG_ADDR(2),
    CPU_RAS_ERR_RECORD_REG_ADDR(3),
};

static const uintptr_t error_records_cl1[] = {
    CPU_RAS_ERR_RECORD_REG_ADDR(4),
    CPU_RAS_ERR_RECORD_REG_ADDR(5),
    CPU_RAS_ERR_RECORD_REG_ADDR(6),
    CPU_RAS_ERR_RECORD_REG_ADDR(7),
};

static const uintptr_t error_records_cl2[] = {
    CPU_RAS_ERR_RECORD_REG_ADDR(8),
    CPU_RAS_ERR_RECORD_REG_ADDR(9),
    CPU_RAS_ERR_RECORD_REG_ADDR(10),
    CPU_RAS_ERR_RECORD_REG_ADDR(11),
};

static const uintptr_t error_records_cl3[] = {
    CPU_RAS_ERR_RECORD_REG_ADDR(12),
    CPU_RAS_ERR_RECORD_REG_ADDR(13),
    CPU_RAS_ERR_RECORD_REG_ADDR(14),
    CPU_RAS_ERR_RECORD_REG_ADDR(15),
};

static const struct fwk_element ras_config_table[] = {
    [CPU_CL0] = {
        .name = "Cluster0 Fault IRQ",
        .data = &((struct mod_ras_isr_desc) {
            .interrupt_no = CLUSTER0_FAULT_INT,
            .ip_type  = TYPE_CPU_IP,
            .interrupt_trigger_type = GIC_LEVEL_TRIGGER_INTR,
            .pe_ids = cpu_cl0_pe_ids,
            .pe_count = FWK_ARRAY_SIZE(cpu_cl3_pe_ids),
            .err_records_base = error_records_cl0,
            .err_record_count = FWK_ARRAY_SIZE(error_records_cl0),
            .mhu_in_base = SI0_AP2SI0_S_MHUV3_RCV_BASE,
            .mhu_out_base = SI0_SI02AP_S_MHUV3_SEND_BASE,
            .mhu_channel = RAS_SYNC_CHANNEL,
            .mhu_flag = RAS_SYNC_FLAG,
            .mhu_poll_retries = RAS_MAX_RETRIES,
        }),
    },
    [CPU_CL1] = {
        .name = "Cluster1 Fault IRQ",
        .data = &((struct mod_ras_isr_desc) {
            .interrupt_no = CLUSTER1_FAULT_INT,
            .ip_type  = TYPE_CPU_IP,
            .interrupt_trigger_type = GIC_LEVEL_TRIGGER_INTR,
            .pe_ids = cpu_cl1_pe_ids,
            .pe_count = FWK_ARRAY_SIZE(cpu_cl3_pe_ids),
            .err_records_base = error_records_cl1,
            .err_record_count = FWK_ARRAY_SIZE(error_records_cl1),
            .mhu_in_base = SI0_AP2SI0_S_MHUV3_RCV_BASE,
            .mhu_out_base = SI0_SI02AP_S_MHUV3_SEND_BASE,
            .mhu_channel = RAS_SYNC_CHANNEL,
            .mhu_flag = RAS_SYNC_FLAG,
            .mhu_poll_retries = RAS_MAX_RETRIES,
        }),
    },
    [CPU_CL2] = {
        .name = "Cluster2 Fault IRQ",
        .data = &((struct mod_ras_isr_desc) {
            .interrupt_no = CLUSTER2_FAULT_INT,
            .ip_type  = TYPE_CPU_IP,
            .interrupt_trigger_type = GIC_LEVEL_TRIGGER_INTR,
            .pe_ids = cpu_cl2_pe_ids,
            .pe_count = FWK_ARRAY_SIZE(cpu_cl3_pe_ids),
            .err_records_base = error_records_cl2,
            .err_record_count = FWK_ARRAY_SIZE(error_records_cl2),
            .mhu_in_base = SI0_AP2SI0_S_MHUV3_RCV_BASE,
            .mhu_out_base = SI0_SI02AP_S_MHUV3_SEND_BASE,
            .mhu_channel = RAS_SYNC_CHANNEL,
            .mhu_flag = RAS_SYNC_FLAG,
            .mhu_poll_retries = RAS_MAX_RETRIES,
        }),
    },
    [CPU_CL3] = {
        .name = "Cluster3 Fault IRQ",
        .data = &((struct mod_ras_isr_desc) {
            .interrupt_no = CLUSTER3_FAULT_INT,
            .ip_type  = TYPE_CPU_IP,
            .interrupt_trigger_type = GIC_LEVEL_TRIGGER_INTR,
            .pe_ids = cpu_cl3_pe_ids,
            .pe_count = FWK_ARRAY_SIZE(cpu_cl3_pe_ids),
            .err_records_base = error_records_cl3,
            .err_record_count = FWK_ARRAY_SIZE(error_records_cl3),
            .mhu_in_base = SI0_AP2SI0_S_MHUV3_RCV_BASE,
            .mhu_out_base = SI0_SI02AP_S_MHUV3_SEND_BASE,
            .mhu_channel = RAS_SYNC_CHANNEL,
            .mhu_flag = RAS_SYNC_FLAG,
            .mhu_poll_retries = RAS_MAX_RETRIES,
        }),
    },
    [COMPONENT_END]={0},
};

const struct fwk_module_config config_ras_handlers = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(ras_config_table),
};
