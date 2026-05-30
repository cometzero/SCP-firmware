/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Clear Memory
 */

#ifndef MOD_CLEAR_MEMORY_H
#define MOD_CLEAR_MEMORY_H

#include <fwk_id.h>

#include <stddef.h>
#include <stdint.h>

/*!
 * \brief Clear memory module configuration data.
 */
struct mod_clear_memory_config {
    void *base_address;
    size_t size;
};

#endif /* MOD_CLEAR_MEMORY_H */
