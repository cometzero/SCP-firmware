/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MOD_RAS_HANDLER_H
#define MOD_RAS_HANDLER_H

#include <mod_ssu.h>

#include <fwk_id.h>
#include <fwk_interrupt.h>
#include <fwk_module_idx.h>

#include <stddef.h>
#include <stdint.h>

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
    /*! Interrupt Priority */
    unsigned int interrupt_priority;
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
};

/*!
 * \brief SCP platform configuration data.
 */
struct mod_ras_config {
    /*! SSU system API ID to be used */
    fwk_id_t ssu_sys_elem_id;
};

/*!
 * \brief RAS context configuration.
 */
struct ras_context {
    /*! ISR based RAS descriptors for RAS IPs */
    struct mod_ras_isr_desc *descriptors;
    /*! Number of ISR Descriptors */
    unsigned int desc_count;
    /*! RAS module configuration */
    const struct mod_ras_config *ras_config;
    /*! SSU API context */
    struct mod_ssu_sys_register_api *ssu_sys_reg_api_ctx;
};

/*!
 * \}
 */

/*!
 * \}
 */

#endif /* MOD_RAS_HANDLER_H */
