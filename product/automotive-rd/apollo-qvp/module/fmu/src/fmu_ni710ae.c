/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "internal/fmu_common.h"

#include <fwk_math.h>
#include <fwk_mmio.h>
#include <fwk_status.h>

#define NI710AE_MAX_RECORDS 384U
#define NI710AE_MECHANISMS 18U
#define FMU_ERR_CTLR_0 0x8
#define FMU_FIELD_ERRSTATUS(n) (0x10 + (n) * 64U)
#define FMU_ERR_MISC0(n) (0x20 + (n) * 64U)
#define FMU_FIELD_ERRGSR_L(n) (0xE000 + (n) * 8U)
#define FMU_FIELD_ERRGSR_H(n) (0xE004 + (n) * 8U)
#define FMU_FIELD_SYS_KEY 0xE200
#define FMU_ERR_SMEN 0xE204
#define FMU_ERR_INJECT 0xE208
#define FMU_ERR_SMINFO 0xE210
#define FMU_ERRDEVID 0xFFC8
#define V_BIT FWK_BIT(30)
#define UE_BIT FWK_BIT(29)
#define OF_BIT FWK_BIT(27)
#define MV_BIT FWK_BIT(26)
#define CE_BITS FWK_GEN_MASK(25, 24)
#define UET_BITS FWK_GEN_MASK(21, 20)
#define CI_BIT FWK_BIT(19)
#define STATUS_W1C (V_BIT | UE_BIT | OF_BIT | MV_BIT | CE_BITS | UET_BITS | CI_BIT)
#define CTRL_ED FWK_BIT(0)
#define CTRL_UI FWK_BIT(2)
#define CTRL_FI FWK_BIT(3)
#define CTRL_CFI FWK_BIT(8)
#define CTRL_CI FWK_BIT(13)
#define FMU_ERR_STATUS_IERR_SHIFT 8
#define FMU_ERR_STATUS_IERR_MASK FWK_GEN_MASK(15, 8)

static uint32_t read32(const struct mod_fmu_dev_config *config, uintptr_t offset)
{
    return fwk_mmio_read_32(config->base + offset);
}

static void write32(const struct mod_fmu_dev_config *config,
                    uintptr_t offset, uint32_t value)
{
    fwk_mmio_write_32(config->base + FMU_FIELD_SYS_KEY, 0xBE);
    fwk_mmio_write_32(config->base + offset, value);
}

static unsigned int record_count(const struct mod_fmu_dev_config *config)
{
    unsigned int count = read32(config, FMU_ERRDEVID) & 0xFFFF;
    return count <= NI710AE_MAX_RECORDS ? count : 0;
}

static unsigned int parent_sources(uint32_t status, uint32_t control)
{
    unsigned int sources = 0;
    if (!(status & V_BIT) || !(control & CTRL_ED))
        return 0;
    if ((status & UE_BIT) && (control & CTRL_UI))
        sources |= 1U << MOD_FMU_SOURCE_ERI;
    if (((status & UE_BIT) && (control & CTRL_FI)) ||
        ((status & CE_BITS) && (control & CTRL_CFI)))
        sources |= 1U << MOD_FMU_SOURCE_FHI;
    if ((status & CI_BIT) && (control & CTRL_CI))
        sources |= 1U << MOD_FMU_SOURCE_CRI;
    return sources;
}

static bool find_record(const struct mod_fmu_dev_config *config,
                        unsigned int source_mask, int critical,
                        unsigned int *node_idx)
{
    unsigned int count = record_count(config);
    uint32_t control = read32(config, FMU_ERR_CTLR_0);
    for (unsigned int group = 0; group * 64 < count; group++) {
        uint64_t pending = read32(config, FMU_FIELD_ERRGSR_L(group)) |
            ((uint64_t)read32(config, FMU_FIELD_ERRGSR_H(group)) << 32);
        while (pending != 0) {
            unsigned int node = group * 64 + fwk_math_log2(LSB_GET(pending));
            if (node >= count)
                break;
            uint32_t status = read32(config, FMU_FIELD_ERRSTATUS(node));
            if ((parent_sources(status, control) & source_mask) != 0 &&
                (critical < 0 || !!(status & CI_BIT) == (bool)critical)) {
                *node_idx = node;
                return true;
            }
            pending &= pending - 1;
        }
    }
    return false;
}

static bool fault_peek(const struct mod_fmu_dev_config *config,
                       bool critical, unsigned int *node_idx)
{
    return find_record(config, 7, critical, node_idx);
}

static bool fault_peek_source(const struct mod_fmu_dev_config *config,
                              unsigned int parent_node, unsigned int *node_idx)
{
    if (!config->has_parent_sources)
        return false;
    for (unsigned int source = 0; source < MOD_FMU_SOURCE_COUNT; source++) {
        if (config->parent_source[source] == parent_node)
            /* A System FMU may upgrade an ERI/FHI input to critical. */
            return find_record(config, 1U << source, -1, node_idx);
    }
    return false;
}

static int fault_ack(const struct mod_fmu_dev_config *config,
                     struct mod_fmu_fault *fault, unsigned int node_idx,
                     bool *fault_tracked, struct mod_fmu_ack_state *state)
{
    if (state->acknowledged)
        return FWK_SUCCESS;
    if (node_idx >= record_count(config))
        return FWK_E_PARAM;
    uint32_t status = read32(config, FMU_FIELD_ERRSTATUS(node_idx));
    if (!(status & V_BIT))
        return FWK_E_DATA;
    if (!*fault_tracked) {
        fault->node_idx = node_idx;
        fault->sm_idx = (status & FMU_ERR_STATUS_IERR_MASK) >>
            FMU_ERR_STATUS_IERR_SHIFT;
        fault->block_id = 0;
        fault->injected = false;
        *fault_tracked = true;
    }
    state->parent_clear_mask = parent_sources(status, read32(config, FMU_ERR_CTLR_0));
    /* A mixed record has one priority syndrome, not one identity per flag.
     * ACK the observed snapshot once; never replay a discarded syndrome. */
    write32(config, FMU_FIELD_ERRSTATUS(node_idx), status & STATUS_W1C);
    state->acknowledged = true;
    return FWK_SUCCESS;
}

static int select_node(const struct mod_fmu_dev_config *config,
                       const struct mod_fmu_fault *fault)
{
    if (fault->node_idx >= record_count(config) || fault->block_id != 0 ||
        (fault->sm_idx != MOD_FMU_SM_ALL && fault->sm_idx >= NI710AE_MECHANISMS))
        return FWK_E_PARAM;
    uint32_t low = read32(config, FMU_ERR_MISC0(fault->node_idx));
    uint32_t high = read32(config, FMU_ERR_MISC0(fault->node_idx) + 4);
    if ((low & 0xFFFF) == 0)
        return FWK_E_DATA;
    write32(config, FMU_ERR_SMINFO, low);
    write32(config, FMU_ERR_SMINFO + 4, high);
    return FWK_SUCCESS;
}

static int inject(const struct mod_fmu_dev_config *config,
                  const struct mod_fmu_fault *fault)
{
    if (fault->sm_idx >= NI710AE_MECHANISMS)
        return FWK_E_PARAM;
    int status = select_node(config, fault);
    if (status != FWK_SUCCESS)
        return status;
    write32(config, FMU_ERR_INJECT, fault->sm_idx);
    return FWK_SUCCESS;
}

static int get_enabled(const struct mod_fmu_dev_config *config,
                       const struct mod_fmu_fault *fault, bool *enabled)
{
    int status = select_node(config, fault);
    if (status != FWK_SUCCESS)
        return status;
    uint32_t mask = fault->sm_idx == MOD_FMU_SM_ALL ?
        (1U << NI710AE_MECHANISMS) - 1 : 1U << fault->sm_idx;
    *enabled = (read32(config, FMU_ERR_SMEN) & mask) == mask;
    return FWK_SUCCESS;
}

static int set_enabled(const struct mod_fmu_dev_config *config,
                       const struct mod_fmu_fault *fault, bool enabled)
{
    int status = select_node(config, fault);
    if (status != FWK_SUCCESS)
        return status;
    uint32_t mask = fault->sm_idx == MOD_FMU_SM_ALL ?
        (1U << NI710AE_MECHANISMS) - 1 : 1U << fault->sm_idx;
    uint32_t value = read32(config, FMU_ERR_SMEN);
    write32(config, FMU_ERR_SMEN, enabled ? value | mask : value & ~mask);
    if (enabled) {
        value = read32(config, FMU_ERR_CTLR_0);
        write32(config, FMU_ERR_CTLR_0,
                value | CTRL_ED | CTRL_UI | CTRL_FI | CTRL_CFI | CTRL_CI);
    }
    return FWK_SUCCESS;
}

static int set_critical(const struct mod_fmu_dev_config *config,
                        const struct mod_fmu_fault *fault, bool critical)
{
    /* criticalErrVector is fixed at hardware integration, not a SMEN field. */
    return FWK_E_SUPPORT;
}

struct mod_fmu_impl_api mod_ni710ae_fmu_api = {
    .fault_peek = fault_peek,
    .fault_peek_source = fault_peek_source,
    .fault_ack = fault_ack,
    .inject = inject,
    .get_enabled = get_enabled,
    .set_enabled = set_enabled,
    .set_critical = set_critical,
};
