/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "internal/ras_defines.h"

#include <mod_ras_handlers.h>

#include <fwk_arch.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_mmio.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_status.h>

static struct ras_context ras_ctx;

/* Flop parity Error strings */
static const char *tfp_error_strings[TFP_ERROR_SOURCES_COUNT] = {
    "DSIDE",     "VECTOR_UNIT", "MMU",        "LEVEL_2", "GIC_CPU_INTERFACE",
    "DBG_TRACE", "ISIDE",       "DECODE",     "RENAME",  "COMMIT",
    "ISSUE",     "IEXECUTE",    "AXIS_BRIDGE"
};

/* Check if the TFP error source value is within valid range */
static inline bool is_valid_tfp_ierr(uint64_t tfp_source)
{
    return (
        (tfp_source >= TFP_ERROR_STRING_OFFSET) &&
        (tfp_source < TFP_ERROR_SOURCES_COUNT + TFP_ERROR_STRING_OFFSET));
}

static void check_tfp_error(uint64_t err_status)
{
    /* Check if the error status indicates a Transient Fault error */
    if ((err_status & ERX_STATUS_V) &&
        (ERX_STATUS_SERR(err_status) == TFP_ERROR_SERR)) {
        /* Check if source of TFP is valid */
        if (!is_valid_tfp_ierr(ERX_STATUS_IERR(err_status))) {
            FWK_LOG_WARN("AP detected TFP Error : Unknown");
        } else {
            /* Prints the TFP error source */
            FWK_LOG_WARN(
                "AP detected TFP Error : %s",
                tfp_error_strings
                    [ERX_STATUS_IERR(err_status) - TFP_ERROR_STRING_OFFSET]);
        }
    }
}

/* Do a linear search to see if the RAS desc matches the arrived interrupt */
static int find_descriptor_idx(unsigned int intr)
{
    unsigned int idx = 0;
    for (; idx < ras_ctx.desc_count; idx++) {
        if (ras_ctx.descriptors[idx].interrupt_no == intr) {
            return idx;
        }
    }
    return FWK_E_PARAM;
}

/* CPU RAS interrupt handler only deals with Outband Errors, Which only cover UE
 */
static void cpu_ras_intr_handler()
{
    uint32_t intr;
    unsigned int cpu_idx = 0;
    uint64_t erx_status;
    struct ext_cpu_ras_cluster_regs *reg = NULL;
    const struct mod_ras_isr_desc *desc;
    int desc_idx;

    /* Retrieve  Interrupt Number */
    fwk_interrupt_get_current(&intr);
    /* Find the descriptor from the Interrupt number */
    desc_idx = find_descriptor_idx(intr);

    if (desc_idx < 0) {
        FWK_LOG_WARN(
            "%s interrupt %u has no descriptor", CPU_HANDLE_MOD_NAME, intr);
        fwk_interrupt_clear_pending(intr);
        return;
    }

    desc = &ras_ctx.descriptors[desc_idx];

    if ((desc->err_records_base == NULL) || (desc->err_record_count == 0U)) {
        FWK_LOG_WARN(
            "%s interrupt %u missing error records", CPU_HANDLE_MOD_NAME, intr);
        fwk_interrupt_clear_pending(intr);
        return;
    }

    for (; cpu_idx < desc->err_record_count; cpu_idx++) {
        reg =
            (struct ext_cpu_ras_cluster_regs *)desc->err_records_base[cpu_idx];

        if (reg == NULL) {
            FWK_LOG_WARN(
                "%s Malformed Error record for CPU 0x%x",
                CPU_HANDLE_MOD_NAME,
                cpu_idx);
            continue;
        }

        /* The Faulty CPU has been found in the Cluster */
        if (reg->ERRXSTATUS & ERX_STATUS_V) {
            break;
        }
    }

    /* Spurious Interrupt - Ignore */
    if ((cpu_idx >= desc->err_record_count) || (reg == NULL)) {
        FWK_LOG_INFO(
            "%s spurious fwk_int number = %d", CPU_HANDLE_MOD_NAME, intr);
        fwk_interrupt_clear_pending(intr);
        return;
    }

    FWK_LOG_INFO("%s Cluster: 0x%x", CPU_HANDLE_MOD_NAME, desc_idx);
    FWK_LOG_INFO(
        "%s Faulty CPU Identified: 0x%x", CPU_HANDLE_MOD_NAME, cpu_idx);
    /* Record the Erx status */
    erx_status = reg->ERRXSTATUS;

    FWK_LOG_INFO("%s ERXSTATUS = 0x%lx", CPU_HANDLE_MOD_NAME, reg->ERRXSTATUS);
    FWK_LOG_INFO("%s ERXMISC0 = 0x%lx", CPU_HANDLE_MOD_NAME, reg->ERRXMISC0);

    /* Only Outband Errors SSU triggered */
    FWK_LOG_INFO("%s Fault Type = Uncontainable Error", CPU_HANDLE_MOD_NAME);
    ras_ctx.ssu_sys_reg_api_ctx->set_sys_ctrl(
        ras_ctx.ras_config->ssu_sys_elem_id, MOD_SSU_FSM_CE_STATE);

    /* Check if Transient fault */
    check_tfp_error(erx_status);

    erx_status = reg->ERRXSTATUS;
    reg->ERRXSTATUS = erx_status;
    reg->ERRXMISC0 = 0x0;

    FWK_LOG_WARN("%s SI Clears Error record", CPU_HANDLE_MOD_NAME);
    FWK_LOG_WARN("%s ERXSTATUS = 0x%lx", CPU_HANDLE_MOD_NAME, reg->ERRXSTATUS);
    FWK_LOG_WARN("%s ERXMISC0 = 0x%lx", CPU_HANDLE_MOD_NAME, reg->ERRXMISC0);

    fwk_interrupt_clear_pending(intr);
}

static int ras_set_interrupt_configuration(const struct mod_ras_isr_desc *desc)
{
    int status = FWK_SUCCESS;

    /* For an Invalid Param */
    if (desc == NULL) {
        return FWK_E_PARAM;
    }

    /* Set the interrupt trigger type */
    status = fwk_interrupt_configure(
        desc->interrupt_no, desc->interrupt_trigger_type);
    if (status != FWK_SUCCESS) {
        return status;
    }
    status = fwk_interrupt_set_intr_priority(
        desc->interrupt_no, desc->interrupt_priority);
    if (status != FWK_SUCCESS) {
        return status;
    }

    switch (desc->ip_type) {
    case TYPE_CPU_IP:
        /* Set the interrupt ISR*/
        status =
            fwk_interrupt_set_isr(desc->interrupt_no, &cpu_ras_intr_handler);
        if (status != FWK_SUCCESS) {
            return status;
        }
        break;
    default:
        status = FWK_E_PARAM;
        return status;
    }

    status = fwk_interrupt_enable(desc->interrupt_no);

    return status;
}

static int mod_ras_handler_init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *data)
{
    FWK_LOG_INFO("%s Initializing", MOD_NAME);

    if (element_count == 0) {
        return FWK_E_PARAM;
    }

    /* Initialise space for descriptors */
    ras_ctx.descriptors =
        fwk_mm_calloc(element_count, sizeof(struct mod_ras_isr_desc));
    ras_ctx.desc_count = element_count;

    /*  Set RAS Configs Configs, this contains API configs */
    ras_ctx.ras_config = data;

    return FWK_SUCCESS;
}

static int mod_ras_handler_start(fwk_id_t id)
{
    int status = FWK_SUCCESS;
    const struct mod_ras_isr_desc *desc;
    unsigned idx = 0;

    /* Invalid Handler */
    if (ras_ctx.descriptors == NULL)
        return FWK_E_HANDLER;

    /* Iterate all IP interrupt descriptions and set Intr Configuration */
    for (; idx < ras_ctx.desc_count; idx++) {
        desc = &ras_ctx.descriptors[idx];
        status = ras_set_interrupt_configuration(desc);
    }

    return status;
}

static int mod_ras_handler_elements_init(
    fwk_id_t element_idx,
    unsigned int element_count,
    const void *data)
{
    const struct mod_ras_isr_desc *desc;
    unsigned int desc_idx;

    desc_idx = fwk_id_get_element_idx(element_idx);
    if (data == NULL) {
        return FWK_E_PARAM;
    }

    /* Assign the descriptor the correct entry */
    desc = data;
    ras_ctx.descriptors[desc_idx] = *desc;

    return FWK_SUCCESS;
}

static int ras_handler_bind(fwk_id_t id, unsigned int round)
{
    int status;

    if (round > 0) {
        return FWK_SUCCESS;
    }

    fwk_id_t ssu_api_id =
        FWK_ID_API_INIT(FWK_MODULE_IDX_SSU, MOD_SSU_SYS_API_IDX);

    if (ras_ctx.ras_config == NULL) {
        return FWK_E_HANDLER;
    }

    status = fwk_module_bind(
        ras_ctx.ras_config->ssu_sys_elem_id,
        ssu_api_id,
        &ras_ctx.ssu_sys_reg_api_ctx);

    if (status != FWK_SUCCESS) {
        return status;
    }

    return status;
}

const struct fwk_module module_ras_handlers = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .init = mod_ras_handler_init,
    .start = mod_ras_handler_start,
    .element_init = mod_ras_handler_elements_init,
    .bind = ras_handler_bind,
};
