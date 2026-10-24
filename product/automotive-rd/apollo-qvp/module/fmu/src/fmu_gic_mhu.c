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

#define FMU_FIELD_ERRCTLR(n)   (0x008 + (n * 64))
#define FMU_FIELD_ERRSTATUS(n) (0x010 + (n * 64))
#define FMU_FIELD_ERRGSR       0xE00
#define FMU_FIELD_SMEN         0xF00
#define FMU_FIELD_SMERR        0xF04
#define FMU_FIELD_SMCR         0xF08
#define FMU_FIELD_STATUS       0xF1C
#define FMU_FIELD_KEY          0xF20
#define FMU_FIELD_ERRUPDATE    0xF28
#define FMU_FIELD_ERRDEVID     0xFC8

#define FMU_ERRCTLR_ED_MASK              FWK_BIT(0)
#define FMU_ERRCTLR_UI_MASK              FWK_BIT(2)
#define FMU_ERRCTLR_CI_MASK              FWK_BIT(13)
#define FMU_ERRSTATUS_V_MASK            FWK_BIT(30)
#define FMU_ERRSTATUS_UE_MASK           FWK_BIT(29)
#define FMU_ERRSTATUS_OF_MASK           FWK_BIT(27)
#define FMU_ERRSTATUS_BLKID_MASK        FWK_GEN_MASK_64(43, 32)
#define FMU_ERRSTATUS_BLKID_SHIFT       32
#define FMU_ERRSTATUS_SMID_MASK         FWK_GEN_MASK(15, 8)
#define FMU_ERRSTATUS_SMID_SHIFT        8
#define FMU_ERRSTATUS_INJ_MASK          FWK_BIT_64(44)
#define FMU_STATUS_OFX_MASK             FWK_BIT_64(47)
#define FMU_ERRUPDATE_RECORD_PAIR_MASK  FWK_GEN_MASK(30, 28)
#define FMU_ERRUPDATE_RECORD_PAIR_SHIFT 28
#define FMU_ERRUPDATE_CRITICAL_MASK     FWK_BIT(2)
#define FMU_ERRUPDATE_NON_CRITICAL_MASK FWK_BIT(1)
#define FMU_STATUS_BUSY_MASK            FWK_BIT(0)
#define FMU_STATUS_TIMEOUT_MASK         FWK_BIT(1)
#define FMU_STATUS_BLKID_PWROFF_MASK    FWK_BIT(2)
#define FMU_STATUS_BLKID_ERR_MASK       FWK_BIT(3)
#define FMU_STATUS_PROTID_ERR_MASK      FWK_BIT(4)
#define FMU_STATUS_UNEXP_RESP_MASK      FWK_BIT(5)
#define FMU_SMEN_BLKTYPE_MASK           FWK_GEN_MASK(30, 28)
#define FMU_SMEN_BLKTYPE_SHIFT          28
#define FMU_SMEN_BLKID_MASK             FWK_GEN_MASK(27, 16)
#define FMU_SMEN_BLKID_SHIFT            16
#define FMU_SMEN_SMID_MASK              FWK_GEN_MASK(15, 8)
#define FMU_SMEN_SMID_SHIFT             8

#define FMU_SMEN_ENABLED   0x1
#define FMU_SMCR_CRITICAL  0x1
#define FMU_SYS_KEY_UNLOCK 0xBE

static fwk_id_t timer_id;
static struct mod_timer_api *timer_api;

static inline void fmu_write_32(
    uintptr_t base,
    uintptr_t offset,
    uint32_t value)
{
    fwk_mmio_write_32(base + FMU_FIELD_KEY, FMU_SYS_KEY_UNLOCK);
    fwk_mmio_write_32(base + offset, value);
}

static inline void fmu_write_64(
    uintptr_t base,
    uintptr_t offset,
    uint64_t value)
{
    fwk_mmio_write_32(base + FMU_FIELD_KEY, FMU_SYS_KEY_UNLOCK);
    fwk_mmio_write_64(base + offset, value);
}

static bool fmu_busy(void *data)
{
    const struct mod_fmu_dev_config *config = data;

    fwk_assert(config != NULL);

    return (fwk_mmio_read_32(config->base + FMU_FIELD_STATUS) &
            FMU_STATUS_BUSY_MASK) == 0;
}

static int fmu_device_wait_busy(const struct mod_fmu_dev_config *config)
{
    int wait_status;
    uint32_t status;

    fwk_assert(config != NULL);

    /* Poll while busy */
    wait_status =
        timer_api->wait(timer_id, FWK_MS(100), fmu_busy, (void *)config);
    if (wait_status != FWK_SUCCESS) {
        return wait_status;
    }

    status = fwk_mmio_read_32(config->base + FMU_FIELD_STATUS);

    /* Inspect the resultant status */
    if (status & FMU_STATUS_TIMEOUT_MASK) {
        FWK_LOG_ERR(MOD_NAME "FMU transaction timed out\n");
        return FWK_E_TIMEOUT;
    } else if (status & FMU_STATUS_BLKID_PWROFF_MASK) {
        FWK_LOG_ERR(MOD_NAME "FMU block powered off\n");
        return FWK_E_DEVICE;
    } else if (status & FMU_STATUS_BLKID_ERR_MASK) {
        FWK_LOG_ERR(MOD_NAME "Invalid FMU block ID\n");
        return FWK_E_PARAM;
    } else if (status & FMU_STATUS_PROTID_ERR_MASK) {
        FWK_LOG_ERR(MOD_NAME "Invalid FMU protection ID\n");
        return FWK_E_PARAM;
    } else if (status & FMU_STATUS_UNEXP_RESP_MASK) {
        return FWK_E_DEVICE;
    }

    return FWK_SUCCESS;
}

static int fmu_device_update(
    const struct mod_fmu_dev_config *config,
    uint32_t record_id)
{
    fwk_assert(config != NULL);

    uint32_t errupdate = ((record_id / 2) << FMU_ERRUPDATE_RECORD_PAIR_SHIFT) |
        ((record_id % 2 == 0) ? FMU_ERRUPDATE_CRITICAL_MASK :
                                FMU_ERRUPDATE_NON_CRITICAL_MASK);

    fmu_write_32(config->base, FMU_FIELD_ERRUPDATE, errupdate);
    return fmu_device_wait_busy(config);
}

static bool fault_peek(
    const struct mod_fmu_dev_config *config,
    bool critical,
    unsigned int *node_idx)
{
    uint64_t errgsr = fwk_mmio_read_64(config->base + FMU_FIELD_ERRGSR);

    /* Even records are critical; odd records are non-critical. Keep the exact
     * record token until ACK so a concurrent fault cannot select another one. */
    errgsr &= critical ? UINT64_C(0x555) : UINT64_C(0xAAA);
    while (errgsr != 0) {
        unsigned int record = fwk_math_log2(LSB_GET(errgsr));
        if (fwk_mmio_read_64(config->base + FMU_FIELD_ERRSTATUS(record)) &
            FMU_ERRSTATUS_V_MASK) {
            *node_idx = record;
            return true;
        }
        errgsr &= errgsr - 1;
    }
    return false;
}

static bool fault_peek_source(
    const struct mod_fmu_dev_config *config,
    unsigned int parent_node,
    unsigned int *node_idx)
{
    /* A System parent can upgrade this FMU's non-critical output. */
    if (parent_node == config->parent_cr_index)
        return fault_peek(config, true, node_idx);
    if (parent_node == config->parent_ncr_index)
        return fault_peek(config, false, node_idx);
    return false;
}

static int fault_ack(
    const struct mod_fmu_dev_config *config,
    struct mod_fmu_fault *fault,
    unsigned int record_id,
    bool *fault_tracked,
    struct mod_fmu_ack_state *state)
{
    uint64_t status;
    int result;

    fwk_assert(config != NULL && fault != NULL && fault_tracked != NULL);
    fwk_assert(state != NULL);
    if (record_id >= 12) {
        return FWK_E_PARAM;
    }
    if (!state->acknowledged) {
        status = fwk_mmio_read_64(config->base + FMU_FIELD_ERRSTATUS(record_id));
        if (!(status & FMU_ERRSTATUS_V_MASK)) {
            return FWK_E_DATA;
        }
        fmu_write_64(config->base, FMU_FIELD_ERRSTATUS(record_id),
            FMU_ERRSTATUS_V_MASK | FMU_ERRSTATUS_UE_MASK |
                (status & FMU_ERRSTATUS_OF_MASK));
        result = fmu_device_wait_busy(config);
        if (result != FWK_SUCCESS) {
            return result;
        }
        state->acknowledged = true;
        state->refresh_pending = (status & FMU_STATUS_OFX_MASK) != 0;
        if (!*fault_tracked) {
            fault->node_idx = record_id / 2;
            fault->sm_idx = (status & FMU_ERRSTATUS_SMID_MASK) >>
                FMU_ERRSTATUS_SMID_SHIFT;
            fault->block_id = (status & FMU_ERRSTATUS_BLKID_MASK) >>
                FMU_ERRSTATUS_BLKID_SHIFT;
            fault->injected = (status & FMU_ERRSTATUS_INJ_MASK) != 0;
            *fault_tracked = true;
        }
    }
    if (state->refresh_pending) {
        result = fmu_device_update(config, record_id);
        if (result != FWK_SUCCESS) {
            return result;
        }
        state->refresh_pending = false;
    }
    return FWK_SUCCESS;
}

static int configure(const struct mod_fmu_config *config)
{
    fwk_assert(config != NULL);

    timer_id = config->timer_id;

    return FWK_SUCCESS;
}

static int bind(fwk_id_t id)
{
    int status;
    const struct mod_fmu_dev_config *config;
    uint32_t record, count;
    uint64_t control;

    status = fwk_module_bind(
        timer_id,
        FWK_ID_API(FWK_MODULE_IDX_TIMER, MOD_TIMER_API_IDX_TIMER),
        &timer_api);
    if (status != FWK_SUCCESS) {
        fwk_unexpected();
        return status;
    }

    config = fwk_module_get_data(id);
    count = fwk_mmio_read_32(config->base + FMU_FIELD_ERRDEVID) & 0xFFFF;
    if (count > 12) {
        return FWK_E_DEVICE;
    }
    for (record = 0; record < count; record++) {
        control = fwk_mmio_read_64(config->base + FMU_FIELD_ERRCTLR(record));
        control |= FMU_ERRCTLR_ED_MASK |
            ((record % 2 == 0) ? FMU_ERRCTLR_CI_MASK : FMU_ERRCTLR_UI_MASK);
        fmu_write_64(config->base, FMU_FIELD_ERRCTLR(record), control);
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
    uint32_t prot_id;

    fwk_assert(config != NULL);
    fwk_assert(fault != NULL);

    if (fault->node_idx > 7 || fault->block_id > 0xFFF) {
        return FWK_E_PARAM;
    }
    prot_id = (fault->node_idx << FMU_SMEN_BLKTYPE_SHIFT) |
        (fault->block_id << FMU_SMEN_BLKID_SHIFT) |
        (fault->sm_idx << FMU_SMEN_SMID_SHIFT);

    fmu_write_32(config->base, FMU_FIELD_SMERR, prot_id);

    return fmu_device_wait_busy(config);
}

static int set_enabled(
    const struct mod_fmu_dev_config *config,
    const struct mod_fmu_fault *fault,
    bool enabled)
{
    uint32_t prot_id;

    fwk_assert(config != NULL);
    fwk_assert(fault != NULL);

    if (fault->node_idx > 7 || fault->block_id > 0xFFF) {
        return FWK_E_PARAM;
    }
    prot_id = (fault->node_idx << FMU_SMEN_BLKTYPE_SHIFT) |
        (fault->block_id << FMU_SMEN_BLKID_SHIFT) |
        (fault->sm_idx << FMU_SMEN_SMID_SHIFT);

    if (enabled) {
        prot_id |= FMU_SMEN_ENABLED;
    } else {
        prot_id &= ~FMU_SMEN_ENABLED;
    }

    fmu_write_32(config->base, FMU_FIELD_SMEN, prot_id);

    return fmu_device_wait_busy(config);
}

static int set_critical(
    const struct mod_fmu_dev_config *config,
    const struct mod_fmu_fault *fault,
    bool critical)
{
    uint32_t prot_id;

    fwk_assert(config != NULL);
    fwk_assert(fault != NULL);

    if (fault->node_idx > 7 || fault->block_id > 0xFFF) {
        return FWK_E_PARAM;
    }
    prot_id = (fault->node_idx << FMU_SMEN_BLKTYPE_SHIFT) |
        (fault->block_id << FMU_SMEN_BLKID_SHIFT) |
        (fault->sm_idx << FMU_SMEN_SMID_SHIFT);

    if (critical) {
        prot_id |= FMU_SMCR_CRITICAL;
    } else {
        prot_id &= ~FMU_SMCR_CRITICAL;
    }

    fmu_write_32(config->base, FMU_FIELD_SMCR, prot_id);

    return fmu_device_wait_busy(config);
}

struct mod_fmu_impl_api mod_fmu_gic_mhu_api = {
    .configure = configure,
    .bind = bind,
    .fault_peek = fault_peek,
    .fault_peek_source = fault_peek_source,
    .fault_ack = fault_ack,
    .inject = inject,
    .set_enabled = set_enabled,
    .set_critical = set_critical,
};
