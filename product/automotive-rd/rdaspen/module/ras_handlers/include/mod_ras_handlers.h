/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MOD_RAS_HANDLER_H
#define MOD_RAS_HANDLER_H

#include <fwk_id.h>
#include <fwk_module_idx.h>

#include <stddef.h>
#include <stdint.h>
#include <fwk_interrupt.h>

#include <mod_ssu.h>

/*!
 * \addtogroup GroupPLATFORMModule PLATFORM Product Modules
 * \{
 */

/*!
 * \defgroup GroupRASHandlerService RD-Aspen RAS Handler Service
 *
 * \details A service to initialize the RD-Aspen FFH handling.
 *
 * \{
 */

/*!
 * \brief Platform RAS IPs types.
 */
enum ras_ip_type {
    TYPE_CPU_IP = 1,
};

/*!
 * \brief Platform interupts descriptions acts as a dictionary to classify RAS
 *  interrupts to various IPs, and assign interrupt handlers.
 */
struct mod_ras_isr_desc {
    /*! Interrupt number*/
    unsigned int interrupt_no;
    /*! Interrupt Trigger Type */
    unsigned int interrupt_trigger_type;
    /*! RAS IP type */
    enum ras_ip_type ip_type;
    /*! PE Numbers for CPU based RAS, Default should be NULL */
    const unsigned int *pe_ids;
    /*! Number of PEs */
    unsigned int pe_count;
    /*! Error Record Base Address */
    const uintptr_t *err_records_base;
    /*! Number of Error Records */
    unsigned int err_record_count;
    /*! MHU in and out base */
    uintptr_t mhu_in_base;
    uintptr_t mhu_out_base;
    /*! MHU channel, flag and retries */
    unsigned int mhu_channel;
    unsigned int mhu_flag;
    unsigned int mhu_poll_retries;
};

/*!
 * \brief RAS context configuration.
 */
struct ras_context {
    struct mod_ras_isr_desc *descriptors;
    unsigned int desc_count;
    struct mod_ssu_sys_register_api *ssu_sys_reg_api_ctx;
    fwk_id_t ssu_sys_reg_id;
    fwk_id_t element_id_ssu;
};

/*!
 * \}
 */

/*!
 * \}
 */

#endif /* MOD_RAS_HANDLER_H */
