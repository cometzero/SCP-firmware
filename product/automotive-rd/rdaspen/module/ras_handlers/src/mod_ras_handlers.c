/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
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

/* Utility function to check if AP doorbell is rung */
static bool is_ap_doorbell_rung(void *unused)
{
    return ras_ctx.ap_door_bell_recieved;
}

/*
 * Module 'transport' signal interface implementation.
 */
static int signal_error(fwk_id_t unused)
{
    return FWK_SUCCESS;
}

static int signal_message(fwk_id_t unused)
{
    ras_ctx.ap_door_bell_recieved = true;
    return FWK_SUCCESS;
}

const struct mod_transport_firmware_signal_api platform_transport_signal_api = {
    .signal_error = signal_error,
    .signal_message = signal_message,
};

/*
 * Helper function to retrieve the 'transport' module signal API.
 */
const void *get_platform_transport_signal_api(void)
{
    return &platform_transport_signal_api;
}

static unsigned int find_descriptor_idx(unsigned int intr)
{
    unsigned int idx = 0;
    for (; idx < ras_ctx.desc_count; idx++) {
        if (ras_ctx.descriptors->interrupt_no == intr) {
            return idx;
        }
    }
    return -1;
}

static void cpu_ras_intr_handler()
{
    uint32_t intr;
    int status;
    unsigned int cpu_idx = 0;
    uint64_t erx_status;
    struct ext_cpu_ras_cluster_regs *reg = NULL;
    uint32_t desc_idx;
    /* make sure this flag is set to false in the begining of the interrupt */
    ras_ctx.ap_door_bell_recieved = false;

    /* Retrieve  Interrupt Number */
    fwk_interrupt_get_current(&intr);
    /* Find the descriptor from the Interrupt number */
    desc_idx = find_descriptor_idx(intr);

    for (; cpu_idx < ras_ctx.descriptors[desc_idx].err_record_count;
         cpu_idx++) {
        reg = (struct ext_cpu_ras_cluster_regs *)ras_ctx.descriptors[desc_idx]
                  .err_records_base[cpu_idx];

        /* The Faulty CPU has been found in the Cluster */
        if (reg->ERRXSTATUS & ERX_STATUS_V) {
            break;
        }
    }

    /* Spurious Interrupt - Ignore */
    if (cpu_idx >= ras_ctx.descriptors[desc_idx].err_record_count) {
        FWK_LOG_INFO(
            "%s spurious fwk_int number = %d", CPU_HANDLE_MOD_NAME, intr);
        fwk_interrupt_clear_pending(intr);
        return;
    }
    /* Record the Erx status */
    erx_status = reg->ERRXSTATUS;
    status = ras_ctx.transport_api->trigger_interrupt(
        ras_ctx.ras_config->transport_elem_id);

    FWK_LOG_INFO("%s ERXSTATUS = 0x%lx", CPU_HANDLE_MOD_NAME, reg->ERRXSTATUS);
    FWK_LOG_INFO("%s ERXMISC0 = 0x%lx", CPU_HANDLE_MOD_NAME, reg->ERRXMISC0);

    if (status == FWK_E_STATE) {
        FWK_LOG_WARN("%s Door bell to AP failed ", CPU_HANDLE_MOD_NAME);
        fwk_interrupt_clear_pending(intr);
        return;
    }

    if (erx_status & ERX_STATUS_CE) {
        FWK_LOG_INFO("%s Fault Type = Correctable Error", CPU_HANDLE_MOD_NAME);
    }

    else if (erx_status & ERX_STATUS_DE) {
        FWK_LOG_INFO("%s Fault Type = Deferred Error", CPU_HANDLE_MOD_NAME);
        ras_ctx.ssu_sys_reg_api_ctx->set_sys_ctrl(
            ras_ctx.ras_config->ssu_sys_elem_id, MOD_SSU_FSM_NCE_STATE);
    }

    else if (erx_status & ERX_STATUS_UC) {
        FWK_LOG_INFO(
            "%s Fault Type = Uncontainable Error", CPU_HANDLE_MOD_NAME);
        ras_ctx.ssu_sys_reg_api_ctx->set_sys_ctrl(
            ras_ctx.ras_config->ssu_sys_elem_id, MOD_SSU_FSM_CE_STATE);
    }

    status = ras_ctx.timer_api->wait(
        ras_ctx.ras_config->timer_elem_id,
        ras_ctx.ras_config->ras_sync_wait_us,
        &is_ap_doorbell_rung,
        NULL);

    if (status != FWK_SUCCESS) {
        /* Clear the RAS Error record since AP didn't reply */
        erx_status = reg->ERRXSTATUS;
        reg->ERRXSTATUS = erx_status;
        reg->ERRXMISC0 = 0x0;

        /* Clear these injection flags aswell for security purposes*/
        reg->ERRXPFGCDN = 0x0;
        reg->ERRXPFGCTL = 0x0;

        FWK_LOG_WARN("%s SI Clears Error record", CPU_HANDLE_MOD_NAME);
        FWK_LOG_WARN(
            "%s ERXSTATUS = 0x%lx", CPU_HANDLE_MOD_NAME, reg->ERRXSTATUS);
        FWK_LOG_WARN(
            "%s ERXMISC0 = 0x%lx", CPU_HANDLE_MOD_NAME, reg->ERRXMISC0);
    }
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

static int ras_handler_bind_request(
    fwk_id_t requester_id,
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void **api)
{
    int status;
    enum mod_ras_handler_api api_id_type;

    api_id_type = (enum mod_ras_handler_api)fwk_id_get_api_idx(api_id);

    switch (api_id_type) {
    case MOD_RAS_API_IDX_SIGNALS:
        *api = get_platform_transport_signal_api();
        status = FWK_SUCCESS;
        break;
    default:
        status = FWK_E_PARAM;
    }

    return status;
}

static int ras_handler_bind(fwk_id_t id, unsigned int round)
{
    int status;

    if (round > 0) {
        return FWK_SUCCESS;
    }

    fwk_id_t transport_api_id = FWK_ID_API_INIT(
        FWK_MODULE_IDX_TRANSPORT, MOD_TRANSPORT_API_IDX_FIRMWARE);
    fwk_id_t ssu_api_id =
        FWK_ID_API_INIT(FWK_MODULE_IDX_SSU, MOD_SSU_SYS_API_IDX);
    fwk_id_t timer_api_id =
        FWK_ID_API_INIT(FWK_MODULE_IDX_TIMER, MOD_TIMER_API_IDX_TIMER);

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

    status = fwk_module_bind(
        ras_ctx.ras_config->timer_elem_id, timer_api_id, &ras_ctx.timer_api);
    if (status != FWK_SUCCESS) {
        return status;
    }

    status = fwk_module_bind(
        ras_ctx.ras_config->transport_elem_id,
        transport_api_id,
        &ras_ctx.transport_api);

    return status;
}

const struct fwk_module module_ras_handlers = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .init = mod_ras_handler_init,
    .start = mod_ras_handler_start,
    .element_init = mod_ras_handler_elements_init,
    .bind = ras_handler_bind,
    .api_count = MOD_RAS_API_COUNT,
    .process_bind_request = ras_handler_bind_request,
};
