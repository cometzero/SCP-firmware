/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_exceptions.h>

#include <stdint.h>

#if AARCH64_EXCEPTION_SYMTAB_MAX_SIZE > 0
const uint8_t aarch64_exception_symtab_blob[AARCH64_EXCEPTION_SYMTAB_MAX_SIZE]
    __attribute__((section(".exception_symtab"), used, aligned(8))) = { 0 };
#endif
