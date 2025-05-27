/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "internal/ras_defines.h"

#include "si0_mmap.h"
#include "platform_core.h"

#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_status.h>
#include <fwk_mm.h>
#include <fwk_arch.h>
#include <mod_ras_handlers.h>
#include <fwk_mmio.h>

static struct ras_context ras_ctx;

static void ring_ras_sync_door_bell_si2ap(
    uintptr_t mhu_send_base,
    uint32_t channel,
    uint32_t flag)
{
    uint32_t mhu_doorbell;
    uintptr_t PDBCWn_SET = ((0x20*channel) + 0x0C + 0x1000);
    do{
        mhu_doorbell = fwk_mmio_read_32(mhu_send_base + PDBCWn_SET);
        mhu_doorbell = mhu_doorbell & (~flag);
        mhu_doorbell = mhu_doorbell | flag;
        fwk_mmio_write_32(mhu_send_base + PDBCWn_SET , mhu_doorbell);
    } while (0);
}

static uint32_t poll_ras_sync_doorbell_ap2si(
    uintptr_t mhu_rcv_base,
    uint32_t channel)
{
    uintptr_t MDBCWn_ST = ((0x20*channel) + 0x1000 + 0x0);
    return fwk_mmio_read_32(mhu_rcv_base + MDBCWn_ST);
}

static void clear_door_bell(uintptr_t mhu_base,
    uint32_t   channel,
    uint32_t   flag_mask)
{
    uintptr_t MDBCWn_CLR = mhu_base + 0x1000 + (0x20*channel) + 0x08;
    fwk_mmio_write_32(MDBCWn_CLR, flag_mask);
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
    uint32_t intr, mhu_reply;
    uint32_t retries = 0;
    unsigned int cpu_idx = 0;
    uint64_t erx_status;
    struct ext_cpu_ras_cluster_regs *reg;
    uint32_t desc_idx;

    /* Retrieve  Interrupt Number */
    fwk_interrupt_get_current(&intr);
    /* Find the descriptor from the Interrupt number */
    desc_idx = find_descriptor_idx(intr);

    for (;cpu_idx < ras_ctx.descriptors[desc_idx].err_record_count; cpu_idx++) {
        reg = (struct ext_cpu_ras_cluster_regs *)
                      ras_ctx.descriptors[desc_idx].err_records_base[cpu_idx];

        /* The Faulty CPU has been found in the Cluster */
        if (reg->ERRXSTATUS != 0x0) {
            break;
        }
    }

    /* Spurious Interrupt - Ignore */
    if (cpu_idx >= ras_ctx.descriptors[desc_idx].err_record_count) {
        fwk_interrupt_clear_pending(intr);
        return;
    }

    ring_ras_sync_door_bell_si2ap(
        ras_ctx.descriptors[desc_idx].mhu_out_base,
        ras_ctx.descriptors[desc_idx].mhu_channel,
        ras_ctx.descriptors[desc_idx].mhu_flag
    );

    do {

        /* Read if Reply has been recieved */
        mhu_reply = poll_ras_sync_doorbell_ap2si(
            ras_ctx.descriptors[desc_idx].mhu_in_base,
            ras_ctx.descriptors[desc_idx].mhu_channel
        );

        /* AP Has cleared the record*/
        if ((mhu_reply & ras_ctx.descriptors[desc_idx].mhu_flag) ==
            ras_ctx.descriptors[desc_idx].mhu_flag) {

            FWK_LOG_INFO("%s AP Door Bell, Error Record cleared by AP",
                            CPU_HANDLE_MOD_NAME);
            clear_door_bell(
                ras_ctx.descriptors[desc_idx].mhu_in_base,
                ras_ctx.descriptors[desc_idx].mhu_channel,
                ras_ctx.descriptors[desc_idx].mhu_flag
            );

            break;
        }
        retries++;

    } while (((mhu_reply & ras_ctx.descriptors[desc_idx].mhu_flag) !=
               ras_ctx.descriptors[desc_idx].mhu_flag) && (retries <
               ras_ctx.descriptors[desc_idx].mhu_poll_retries));

    FWK_LOG_INFO("%s fwk_int number = %d",CPU_HANDLE_MOD_NAME ,intr);
    FWK_LOG_INFO("%s ERXSTATUS = 0x%lx",CPU_HANDLE_MOD_NAME, reg->ERRXSTATUS);
    FWK_LOG_INFO("%s ERXMISC0 = 0x%lx", CPU_HANDLE_MOD_NAME,reg->ERRXMISC0);

    /* All Retries fail clear the respective error record */
    if (retries==ras_ctx.descriptors[desc_idx].mhu_poll_retries) {
        /* Clear the RAS Error record since AP didn't reply */
        erx_status = reg->ERRXSTATUS;
        reg->ERRXSTATUS = erx_status;
        reg->ERRXMISC0 = 0x0;

        /* Clear these injection flags aswell for security purposes*/
        reg->ERRXPFGCDN = 0x0;
        reg->ERRXPFGCTL = 0x0;

        FWK_LOG_INFO("%s SI Clears Error record",CPU_HANDLE_MOD_NAME);
        FWK_LOG_INFO("%s ERXSTATUS = 0x%lx",CPU_HANDLE_MOD_NAME, reg->ERRXSTATUS);
        FWK_LOG_INFO("%s ERXMISC0 = 0x%lx", CPU_HANDLE_MOD_NAME,reg->ERRXMISC0);
    }

    fwk_interrupt_clear_pending(intr);
}

static int ras_set_interrupt_configuration(const struct mod_ras_isr_desc *desc){
    int status = FWK_SUCCESS;

    /* For an Invalid Param */
    if (desc == NULL) {
        return FWK_E_PARAM;
    }

    /* Set the interrupt trigger type */
    status = fwk_interrupt_configure(
        desc->interrupt_no, desc->interrupt_trigger_type);
    if (status != FWK_SUCCESS) {
        return  status;
    }

    switch (desc->ip_type)
    {
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
    ras_ctx.descriptors = fwk_mm_calloc(element_count,
                                        sizeof(struct mod_ras_isr_desc));
    ras_ctx.desc_count = element_count;

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
    for (;idx<ras_ctx.desc_count;idx++) {
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

const struct fwk_module module_ras_handlers = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .init = mod_ras_handler_init,
    .start = mod_ras_handler_start,
    .element_init = mod_ras_handler_elements_init,
};
