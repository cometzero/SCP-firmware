/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'smcf'.
 */

#include "platform_core.h"
#include "si0_irq.h"
#include "si0_mmap.h"
#include "smcf_utils.h"

#include <si0_smcf.h>

#include <mod_smcf.h>

#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

#include <stdio.h>

/* Address of MGI registers in Cluster utility bus (AP Cluster level MGIs) */
#define CLUSTER_UTILITY_ADDRESS             SI0_ATW1_CLUSTER_UTILITY_BASE
#define SMCF_AP_CLUSTER_MGI_REGISTER_OFFSET UINT32_C(0x200000)
#define SMCF_AP_CLUSTER_MGI_REGISTER_BASE \
    (CLUSTER_UTILITY_ADDRESS + SMCF_AP_CLUSTER_MGI_REGISTER_OFFSET)
#define SMCF_AP_CLUSTER_MGI_REGISTER_STRIDE UINT32_C(0X04000000)
#define SMCF_AP_CLUSTER_0_MGI_REGISTER_ADDR \
    (SMCF_AP_CLUSTER_MGI_REGISTER_BASE + \
     (0 * SMCF_AP_CLUSTER_MGI_REGISTER_STRIDE))
#define SMCF_AP_CLUSTER_1_MGI_REGISTER_ADDR \
    (SMCF_AP_CLUSTER_MGI_REGISTER_BASE + \
     (1 * SMCF_AP_CLUSTER_MGI_REGISTER_STRIDE))
#define SMCF_AP_CLUSTER_2_MGI_REGISTER_ADDR \
    (SMCF_AP_CLUSTER_MGI_REGISTER_BASE + \
     (2 * SMCF_AP_CLUSTER_MGI_REGISTER_STRIDE))
#define SMCF_AP_CLUSTER_3_MGI_REGISTER_ADDR \
    (SMCF_AP_CLUSTER_MGI_REGISTER_BASE + \
     (3 * SMCF_AP_CLUSTER_MGI_REGISTER_STRIDE))

/* Address of MGI registers in SMD Expansion (SMD SMCF MGI) */
#define SMCF_SMD_MGI_REGISTER_ADDR SI0_ATW16_SMCF_SMD_MGI_BASE

/* Data location stride */
#define SMD_SHARED_SRAM_STRIDE UINT32_C(0x100)

/* Data location in shared SRAM - READ view (via SI ATU) */
#define SMD_SHARED_SRAM_READ_BASE SI0_ATW17_SMD_SRAM_BASE
#define SMD_SHARED_SRAM_AP_CLUSTER_0_MGI_READ_ADDR \
    (SMD_SHARED_SRAM_READ_BASE + (0 * SMD_SHARED_SRAM_STRIDE))
#define SMD_SHARED_SRAM_AP_CLUSTER_1_MGI_READ_ADDR \
    (SMD_SHARED_SRAM_READ_BASE + (1 * SMD_SHARED_SRAM_STRIDE))
#define SMD_SHARED_SRAM_AP_CLUSTER_2_MGI_READ_ADDR \
    (SMD_SHARED_SRAM_READ_BASE + (2 * SMD_SHARED_SRAM_STRIDE))
#define SMD_SHARED_SRAM_AP_CLUSTER_3_MGI_READ_ADDR \
    (SMD_SHARED_SRAM_READ_BASE + (3 * SMD_SHARED_SRAM_STRIDE))
#define SMD_SHARED_SRAM_SMD_MGI_READ_OFFS UINT32_C(0x2000)
#define SMD_SHARED_SRAM_SMD_MGI_READ_ADDR \
    (SMD_SHARED_SRAM_READ_BASE + SMD_SHARED_SRAM_SMD_MGI_READ_OFFS)

/* Data location in shared SRAM - WRITE view (via AP ATU)*/
#define SMD_SHARED_SRAM_AP_CLUSTER_MGI_WRITE_BASE SI0_AP_CLUSTER_MGI_WRITE_BASE
#define SMD_SHARED_SRAM_AP_CLUSTER_0_MGI_WRITE_ADDR \
    (SMD_SHARED_SRAM_AP_CLUSTER_MGI_WRITE_BASE + (0 * SMD_SHARED_SRAM_STRIDE))
#define SMD_SHARED_SRAM_AP_CLUSTER_1_MGI_WRITE_ADDR \
    (SMD_SHARED_SRAM_AP_CLUSTER_MGI_WRITE_BASE + (1 * SMD_SHARED_SRAM_STRIDE))
#define SMD_SHARED_SRAM_AP_CLUSTER_2_MGI_WRITE_ADDR \
    (SMD_SHARED_SRAM_AP_CLUSTER_MGI_WRITE_BASE + (2 * SMD_SHARED_SRAM_STRIDE))
#define SMD_SHARED_SRAM_AP_CLUSTER_3_MGI_WRITE_ADDR \
    (SMD_SHARED_SRAM_AP_CLUSTER_MGI_WRITE_BASE + (3 * SMD_SHARED_SRAM_STRIDE))

/* Data location in shared SRAM - WRITE view (via SMD Expansion ATU)*/
#define SMD_SHARED_SRAM_SMD_EXP_MGI_WRITE_ADDR SI0_ATW18_SMCF_SMDEXP_SRAM_BASE

/* MGI sampling period in cycles */
#define MGI_PERIOD_IN_CYCLES UINT32_C(8000000)

/*
 * SMCF Context module configuration
 */
static const struct fwk_element smcf_element_table[SI0_SMCF_MGI_IDX_COUNT+1] = {
    [SI0_SMCF_MGI_IDX_SMD_SMCF_EXPANSION_MGI] = {
        .name = "MGI_SMD_SMCF_MGI",
        .sub_element_count = SI0_SMCF_SMD_SMCF_EXPANSION_MGI_MLI_IDX_COUNT,
        .data = &((struct mod_smcf_element_config) {
            .reg_base = SMCF_SMD_MGI_REGISTER_ADDR,
            .irq = CL0_SMCF_SMD_MGI_IRQ_OUT,
            .sample_type = SMCF_SAMPLE_TYPE_PERIODIC,
            .sample_period = MGI_PERIOD_IN_CYCLES,
            .data_config.header_format = 0,
            .data_config.data_location = SMCF_DATA_LOCATION_RAM,
            .data_config.write_addr = SMD_SHARED_SRAM_SMD_EXP_MGI_WRITE_ADDR,
            .data_config.read_addr = (uint32_t*)SMD_SHARED_SRAM_SMD_MGI_READ_ADDR,
        }),
    },
    [SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_0] = {
        .name = "MGI_AP_CLUSTER_MGI_0",
        .sub_element_count = SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_COUNT,
        .data = &((struct mod_smcf_element_config) {
            .reg_base = SMCF_AP_CLUSTER_0_MGI_REGISTER_ADDR,
            .irq = CL0_SMCF_AP_MGI_CLUSTER_0_IRQ_OUT,
            .sample_type = SMCF_SAMPLE_TYPE_PERIODIC,
            .sample_period = MGI_PERIOD_IN_CYCLES,
            .data_config.header_format = 0,
            .data_config.data_location = SMCF_DATA_LOCATION_RAM,
            .data_config.write_addr = SMD_SHARED_SRAM_AP_CLUSTER_0_MGI_WRITE_ADDR,
            .data_config.read_addr = (uint32_t*)SMD_SHARED_SRAM_AP_CLUSTER_0_MGI_READ_ADDR,
        }),
    },
    [SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_1] = {
        .name = "MGI_AP_CLUSTER_MGI_1",
        .sub_element_count = SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_COUNT,
        .data = &((struct mod_smcf_element_config) {
            .reg_base = SMCF_AP_CLUSTER_1_MGI_REGISTER_ADDR,
            .irq = CL0_SMCF_AP_MGI_CLUSTER_1_IRQ_OUT,
            .sample_type = SMCF_SAMPLE_TYPE_PERIODIC,
            .sample_period = MGI_PERIOD_IN_CYCLES,
            .data_config.header_format = 0,
            .data_config.data_location = SMCF_DATA_LOCATION_RAM,
            .data_config.write_addr = SMD_SHARED_SRAM_AP_CLUSTER_1_MGI_WRITE_ADDR,
            .data_config.read_addr = (uint32_t*)SMD_SHARED_SRAM_AP_CLUSTER_1_MGI_READ_ADDR,
        }),
    },
    [SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_2] = {
        .name = "MGI_AP_CLUSTER_MGI_2",
        .sub_element_count = SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_COUNT,
        .data = &((struct mod_smcf_element_config) {
            .reg_base = SMCF_AP_CLUSTER_2_MGI_REGISTER_ADDR,
            .irq = CL0_SMCF_AP_MGI_CLUSTER_2_IRQ_OUT,
            .sample_type = SMCF_SAMPLE_TYPE_PERIODIC,
            .sample_period = MGI_PERIOD_IN_CYCLES,
            .data_config.header_format = 0,
            .data_config.data_location = SMCF_DATA_LOCATION_RAM,
            .data_config.write_addr = SMD_SHARED_SRAM_AP_CLUSTER_2_MGI_WRITE_ADDR,
            .data_config.read_addr = (uint32_t*)SMD_SHARED_SRAM_AP_CLUSTER_2_MGI_READ_ADDR,
        }),
    },
    [SI0_SMCF_MGI_IDX_AP_CLUSTER_MGI_3] = {
        .name = "MGI_AP_CLUSTER_MGI_3",
        .sub_element_count = SI0_SMCF_AP_CLUSTER_MGI_MLI_IDX_COUNT,
        .data = &((struct mod_smcf_element_config) {
            .reg_base = SMCF_AP_CLUSTER_3_MGI_REGISTER_ADDR,
            .irq = CL0_SMCF_AP_MGI_CLUSTER_3_IRQ_OUT,
            .sample_type = SMCF_SAMPLE_TYPE_PERIODIC,
            .sample_period = MGI_PERIOD_IN_CYCLES,
            .data_config.header_format = 0,
            .data_config.data_location = SMCF_DATA_LOCATION_RAM,
            .data_config.write_addr = SMD_SHARED_SRAM_AP_CLUSTER_3_MGI_WRITE_ADDR,
            .data_config.read_addr = (uint32_t*)SMD_SHARED_SRAM_AP_CLUSTER_3_MGI_READ_ADDR,
        }),
    },
    [SI0_SMCF_MGI_IDX_COUNT] = { 0 },
};

const struct fwk_module_config config_smcf = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(smcf_element_table),
};
