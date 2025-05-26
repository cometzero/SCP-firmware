/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "internal/ras_defines.h"

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

static void cpu_ras_intr_handler()
{
    uint32_t intr;
    fwk_interrupt_get_current(&intr);

    fwk_interrupt_clear_pending(intr);
}

static int ras_set_interrupt_configuration(const struct mod_ras_isr_desc *desc){
    int status = FWK_SUCCESS;

    /* For an Invalid Param */
    if (desc == NULL) {
        return FWK_E_PARAM;
    }

    /* Set the interrupt trigger type */
    status = fwk_interrupt_configure(desc->interrupt_no,desc->interrupt_trigger_type);
    if (status != FWK_SUCCESS) {
        return  status;
    }

    switch (desc->ip_type)
    {
    case TYPE_CPU_IP:
        /* Set the interrupt ISR*/
        status = fwk_interrupt_set_isr(desc->interrupt_no, &cpu_ras_intr_handler);
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
    unsigned int  desc_idx;

    desc_idx = fwk_id_get_element_idx(element_idx);
    if (data==NULL) {
        return FWK_E_PARAM;
    }

    /* Assign the descriptor the correct entry */
    desc = data;
    ras_ctx.descriptors[desc_idx]=*desc;

    return FWK_SUCCESS;
}

const struct fwk_module module_ras_handlers = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .init = mod_ras_handler_init,
    .start = mod_ras_handler_start,
    .element_init = mod_ras_handler_elements_init,
};

