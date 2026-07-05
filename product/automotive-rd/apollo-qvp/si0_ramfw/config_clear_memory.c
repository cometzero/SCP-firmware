/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'clear_memory'.
 */
#include "si0_mmap.h"

#include <mod_clear_memory.h>

#include <fwk_module.h>

static const struct fwk_element clear_memory_table[] = {
    {
        .name = "Secure Peripheral SRAM",
        .data =
            &(struct mod_clear_memory_config){
                .base_address =
                    (void *)(uintptr_t)SI0_ATW6_AP_PERIPHERAL_SRAM_BASE,
                .size = SI0_ATW6_AP_PERIPHERAL_SRAM_SIZE,
            },
    },
    {
        .name = "Non Secure Peripheral SRAM",
        .data =
            &(struct mod_clear_memory_config){
                .base_address =
                    (void *)(uintptr_t)SI0_ATW7_AP_PERIPHERAL_NS_SRAM_BASE,
                .size = SI0_ATW7_AP_PERIPHERAL_NS_SRAM_SIZE,
            },
    },
    {
        .name = "SMD SRAM",
        .data =
            &(struct mod_clear_memory_config){
                .base_address = (void *)(uintptr_t)SI0_ATW17_SMD_SRAM_BASE,
                .size = SI0_ATW17_SMD_SRAM_SIZE,
            },
    },
    { 0 },
};

const struct fwk_module_config config_clear_memory = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(clear_memory_table),
};
