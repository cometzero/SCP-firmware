/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "internal/fmu_common.h"

#include <mod_timer.h>

#include <fwk_assert.h>
#include <fwk_log.h>
#include <fwk_math.h>
#include <fwk_mmio.h>
#include <fwk_module.h>
#include <fwk_status.h>
#include <fwk_time.h>

#include <stddef.h>
#include <stdint.h>

#define NI710AE_MAX_DISCOVERY_ENTRIES 64

struct ni710ae_fmu_node_info {
    /*! Nodetype of block reporting error */
    uint16_t node_type;
    /*! Node ID of block reporting error */
    uint16_t node_id;
    /*! Clock domain ID of block reporting error */
    uint16_t clock_domain_id;
    /*! Power domain ID of block reporting error */
    uint16_t power_domain_id;
    /*! Voltage domain ID of block reporting error */
    uint16_t voltage_domain_id;
};

static struct ni710ae_fmu_node_info
    ni710ae_node_info_table[NI710AE_MAX_DISCOVERY_ENTRIES];

#define FMU_ERR_FR_0           0x0
#define FMU_ERR_CTLR_0         0x8
#define FMU_FIELD_ERRSTATUS(n) (0x010 + (n * 64U))
#define FMU_ERR_MISCO(n)       (0x020 + (n * 64U))
#define FMU_ERR_FR(n)          (0x040 + (n * 64U))
#define FMU_ERR_CTRL(n)        (0x048 + (n * 64U))
#define FMU_FIELD_ERRGSR_L(n)  (0xE000 + (8 * (n)))
#define FMU_FIELD_ERRGSR_H(n)  (0xE004 + (8 * (n)))
#define FMU_FIELD_SYS_KEY      0xE200
#define FMU_ERR_SMEN           0xE204
#define FMU_ERR_INJECT         0xE208
#define FMU_ERR_SMINFO         0xE210
#define FMU_ERRDEVID           0xFFC8

#define NI710AE_FMU_NUM_MECHANISMS 18U
#define FMU_SYS_KEY_UNLOCK         0xBE

#define FMU_ERR_MISCO_NODE_TYPE_MASK  FWK_GEN_MASK(15, 0)
#define FMU_ERR_MISCO_NODE_TYPE_SHIFT 0

#define FMU_ERR_MISCO_NODE_ID_MASK  FWK_GEN_MASK(31, 16)
#define FMU_ERR_MISCO_NODE_ID_SHIFT 16

#define FMU_ERR_MISCO_NODE_CD_ID_MASK  FWK_GEN_MASK_64(41, 32)
#define FMU_ERR_MISCO_NODE_CD_ID_SHIFT 32

#define FMU_ERR_MISCO_NODE_PD_ID_MASK  FWK_GEN_MASK_64(51, 42)
#define FMU_ERR_MISCO_NODE_PD_ID_SHIFT 42

#define FMU_ERR_MISCO_NODE_VD_ID_MASK  FWK_GEN_MASK_64(61, 52)
#define FMU_ERR_MISCO_NODE_VD_ID_SHIFT 52

#define FMU_ERR_STATUS_IERR_MASK  FWK_GEN_MASK(12, 8)
#define FMU_ERR_STATUS_IERR_SHIFT 8

#define FMU_ERRGSR_MAX      5U
#define FMU_ERRGSR_NUM_BITS 32U
#define FMU_ENABLE_CRITICAL 0x3BFFF
#define NODE_TYPE_FMU       0x61
#define NI710AE_FMU_DISABLE 0

#define V_BIT    FWK_BIT(30)
#define UE_BIT   FWK_BIT(29)
#define OF_BIT   FWK_BIT(27)
#define MV_BIT   FWK_BIT(26)
#define CE_BITS  (FWK_BIT(25) | FWK_BIT(24))
#define UET_BITS (FWK_BIT(21) | FWK_BIT(20))

static inline void fmu_write_32(
    uintptr_t base,
    uintptr_t offset,
    uint32_t value)
{
    fwk_mmio_write_32(base + FMU_FIELD_SYS_KEY, FMU_SYS_KEY_UNLOCK);
    fwk_mmio_write_32(base + offset, value);
}

static inline uint32_t fmu_read_32(uintptr_t base, uintptr_t offset)
{
    return fwk_mmio_read_32(base + offset);
}

static unsigned int find_active_node(const struct mod_fmu_dev_config *config)
{
    uint64_t errgsr;
    unsigned int errgsr_idx;

    fwk_assert(config != NULL);

    /* Determine fault record idx */
    for (errgsr_idx = 0; errgsr_idx <= FMU_ERRGSR_MAX; errgsr_idx++) {
        errgsr = fmu_read_32(config->base, FMU_FIELD_ERRGSR_L(errgsr_idx)) |
            ((uint64_t)fmu_read_32(config->base, FMU_FIELD_ERRGSR_H(errgsr_idx))
             << FMU_ERRGSR_NUM_BITS);

        if (errgsr != 0) {
            return (errgsr_idx * FMU_ERRGSR_NUM_BITS * 2) +
                fwk_math_log2(LSB_GET(errgsr));
        }
    }

    return MOD_FMU_PARENT_NONE;
}

static bool fault_peek(
    const struct mod_fmu_dev_config *config,
    unsigned int *node_idx)
{
    unsigned int next_node_idx;

    fwk_assert(config != NULL);
    fwk_assert(node_idx != NULL);

    /* If current FMU has an active fault record */
    next_node_idx = find_active_node(config);
    if (next_node_idx == MOD_FMU_PARENT_NONE) {
        return false;
    }
    *node_idx = next_node_idx;

    return true;
}

static void ack_error_status(
    const struct mod_fmu_dev_config *config,
    unsigned int node_idx)
{
    uint32_t val;
    uint32_t writeback = 0;

    fwk_assert(config != NULL);

    val = fmu_read_32(config->base, FMU_FIELD_ERRSTATUS(node_idx));

    if (val & V_BIT)
        writeback |= (val & V_BIT);
    if (val & UE_BIT)
        writeback |= (val & UE_BIT);
    if (val & OF_BIT)
        writeback |= (val & OF_BIT);
    if (val & MV_BIT)
        writeback |= (val & MV_BIT);

    if (val & CE_BITS)
        writeback |= (val & CE_BITS);
    if (val & UET_BITS)
        writeback |= (val & UET_BITS);

    if (writeback)
        fmu_write_32(config->base, FMU_FIELD_ERRSTATUS(node_idx), val);
}

static void fault_ack(
    const struct mod_fmu_dev_config *config,
    struct mod_fmu_fault *fault,
    unsigned int node_idx,
    bool *fault_tracked)
{
    fwk_assert(config != NULL);
    fwk_assert(fault != NULL);
    fwk_assert(fault_tracked != NULL);

    /* Acknowledge the fault */
    ack_error_status(config, node_idx);

    if (!(*fault_tracked)) {
        fault->node_idx = node_idx;
        fault->sm_idx =
            (fmu_read_32(config->base, FMU_FIELD_ERRSTATUS(node_idx)) &
             FMU_ERR_STATUS_IERR_MASK) >>
            FMU_ERR_STATUS_IERR_SHIFT;
        *fault_tracked = true;
    }
}

static int prepare_error_records(const struct mod_fmu_dev_config *config)
{
    uint32_t num_records;
    uint32_t idx;
    long long unsigned int rd_data;

    num_records = fwk_mmio_read_32(config->base + FMU_ERRDEVID);

    /* Check if there is a record */
    if (num_records == 0) {
        FWK_LOG_ERR(MOD_NAME "error record found:%d", num_records);
        return FWK_E_DATA;
    }

    const uint32_t capped = (num_records > NI710AE_MAX_DISCOVERY_ENTRIES) ?
        NI710AE_MAX_DISCOVERY_ENTRIES :
        num_records;

    for (idx = 0; idx < capped; idx++) {
        uint32_t low;
        uint32_t high;
        low = fwk_mmio_read_32(config->base + FMU_ERR_MISCO(idx));
        high = fwk_mmio_read_32(config->base + FMU_ERR_MISCO(idx) + 4);

        rd_data = ((uint64_t)high << 32) | low;
        ni710ae_node_info_table[idx].node_type =
            ((rd_data & FMU_ERR_MISCO_NODE_TYPE_MASK) >>
             FMU_ERR_MISCO_NODE_TYPE_SHIFT);
        ni710ae_node_info_table[idx].node_id =
            ((rd_data & FMU_ERR_MISCO_NODE_ID_MASK) >>
             FMU_ERR_MISCO_NODE_ID_SHIFT);
        ni710ae_node_info_table[idx].clock_domain_id =
            ((rd_data & FMU_ERR_MISCO_NODE_CD_ID_MASK) >>
             FMU_ERR_MISCO_NODE_CD_ID_SHIFT);
        ni710ae_node_info_table[idx].power_domain_id =
            ((rd_data & FMU_ERR_MISCO_NODE_PD_ID_MASK) >>
             FMU_ERR_MISCO_NODE_PD_ID_SHIFT);
        ni710ae_node_info_table[idx].voltage_domain_id =
            ((rd_data & FMU_ERR_MISCO_NODE_VD_ID_MASK) >>
             FMU_ERR_MISCO_NODE_VD_ID_SHIFT);
    }

    return FWK_SUCCESS;
}

/*
 * API Handlers
 */
static int inject(
    const struct mod_fmu_dev_config *config,
    const struct mod_fmu_fault *fault)
{
    uint32_t idx;
    uint32_t num_records;

    fwk_assert(config != NULL);
    fwk_assert(fault != NULL);

    num_records = fwk_mmio_read_32(config->base + FMU_ERRDEVID);

    /* Check if there is a record */
    if (num_records == 0) {
        FWK_LOG_ERR(MOD_NAME "error record found:%d", num_records);
        return FWK_E_DATA;
    }
    const uint32_t capped = (num_records > NI710AE_MAX_DISCOVERY_ENTRIES) ?
        NI710AE_MAX_DISCOVERY_ENTRIES :
        num_records;

    for (idx = 0; idx < capped; idx++) {
        uint32_t fmu_sminfo_low;
        uint32_t fmu_sminfo_high;
        fmu_sminfo_low = ni710ae_node_info_table[idx].node_type;
        fmu_sminfo_low =
            (fmu_sminfo_low | (ni710ae_node_info_table[idx].node_id << 16));
        fmu_sminfo_high = ni710ae_node_info_table[idx].clock_domain_id;
        fmu_sminfo_high =
            (fmu_sminfo_high |
             (ni710ae_node_info_table[idx].power_domain_id << 10) |
             (ni710ae_node_info_table[idx].voltage_domain_id << 20));
        if (fmu_sminfo_low == NODE_TYPE_FMU) {
            fmu_write_32(config->base, FMU_ERR_SMINFO, fmu_sminfo_low);
            fmu_write_32(config->base, FMU_ERR_SMINFO + 4, fmu_sminfo_high);
            fmu_write_32(config->base, FMU_ERR_INJECT, fault->sm_idx);
            break;
        }
    }
    return FWK_SUCCESS;
}

static int set_enabled(
    const struct mod_fmu_dev_config *config,
    const struct mod_fmu_fault *fault,
    bool enabled)
{
    fwk_assert(config != NULL);

    int st = prepare_error_records(config);
    if (st != FWK_SUCCESS)
        return st;

    if (enabled) {
        fmu_write_32(
            config->base, FMU_ERR_SMEN, (1 << NI710AE_FMU_NUM_MECHANISMS) - 1);
    } else {
        fmu_write_32(config->base, FMU_ERR_SMEN, NI710AE_FMU_DISABLE);
    }

    return FWK_SUCCESS;
}

static int set_critical(
    const struct mod_fmu_dev_config *config,
    const struct mod_fmu_fault *fault,
    bool critical)
{
    fwk_assert(config != NULL);
    fwk_assert(fault != NULL);

    if (critical) {
        fmu_write_32(config->base, FMU_ERR_SMEN, FMU_ENABLE_CRITICAL);
    } else {
        fmu_write_32(
            config->base, FMU_ERR_SMEN, (1 << NI710AE_FMU_NUM_MECHANISMS) - 1);
    }
    return FWK_SUCCESS;
}

struct mod_fmu_impl_api mod_ni710ae_fmu_api = {
    .fault_peek = fault_peek,
    .fault_ack = fault_ack,
    .inject = inject,
    .set_enabled = set_enabled,
    .set_critical = set_critical,
};
