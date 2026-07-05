/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mod_clear_memory.h>

#include <fwk_assert.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#define MOD_NAME "[Clear_Memory] "

/* Memory context */
struct clear_memory_element_ctx {
    /* Memory configuration data */
    const struct mod_clear_memory_config *mem_cfg;
};

/* Module context */
struct clear_memory_mod_ctx {
    /* Memory context data */
    struct clear_memory_element_ctx *mem_ctx_table;
    /* Number of memories that need to be cleared */
    uint32_t mem_count;
};

static struct clear_memory_mod_ctx ctx;

static void zero64_inbounds_stp(const struct mod_clear_memory_config *mem_cfg)
{
    unsigned long base_addr, aligned_lo, aligned_hi, bytes;
    size_t size_in_bytes;

    base_addr = (unsigned long)mem_cfg->base_address;
    size_in_bytes = mem_cfg->size;

    /* Compute 8-byte aligned boundaries */
    aligned_lo = (base_addr + 7UL) & ~7UL;
    aligned_hi = (base_addr + (unsigned long)size_in_bytes) & ~7UL;
    if (aligned_hi <= aligned_lo)
        return;

    bytes = aligned_hi - aligned_lo;

    __asm__ volatile(
        "mov   x2, %[start]        \n" // pointer
        "mov   x3, %[bytes]        \n" // total bytes to clear
        "lsr   x4, x3, #4          \n" // q = bytes / 16
        "and   x5, x3, #15         \n" // r = bytes % 16
        "cbz   x4, 2f              \n"
        "1:                        \n"
        "stp   xzr, xzr, [x2], #16 \n" // store 16B zeros
        "subs  x4, x4, #1          \n"
        "b.ne  1b                  \n"
        "2:                        \n"
        "tst   x5, #8              \n" // if remainder >= 8, one last 8B zero
        "beq   3f                  \n"
        "str   xzr, [x2], #8       \n"
        "3:                        \n"
        :
        : [start] "r"(aligned_lo), [bytes] "r"(bytes)
        : "x2", "x3", "x4", "x5", "memory", "cc");
}

static int clear_memory_init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *data)
{
    if (element_count == 0) {
        return FWK_E_PARAM;
    }

    ctx.mem_ctx_table =
        fwk_mm_calloc(element_count, sizeof(struct clear_memory_element_ctx));

    ctx.mem_count = element_count;

    return FWK_SUCCESS;
}

static int clear_memory_element_init(
    fwk_id_t id,
    unsigned int sub_element_count,
    const void *data)
{
    struct clear_memory_element_ctx *mem_ctx;
    const struct mod_clear_memory_config *mem_cfg;
    unsigned int element_idx = fwk_id_get_element_idx(id);

    if (element_idx >= ctx.mem_count) {
        return FWK_E_PARAM;
    }

    mem_cfg = (struct mod_clear_memory_config *)data;

    if (mem_cfg == NULL) {
        return FWK_E_DATA;
    }

    mem_ctx = &ctx.mem_ctx_table[element_idx];
    mem_ctx->mem_cfg = mem_cfg;

    return FWK_SUCCESS;
}

static int clear_memory_start(fwk_id_t id)
{
    unsigned int element_idx;
    struct clear_memory_element_ctx *mem_ctx;
    const struct mod_clear_memory_config *mem_cfg;

    if (fwk_id_is_type(id, FWK_ID_TYPE_MODULE)) {
        return FWK_SUCCESS;
    }

    element_idx = fwk_id_get_element_idx(id);

    mem_ctx = &ctx.mem_ctx_table[element_idx];
    mem_cfg = mem_ctx->mem_cfg;

    zero64_inbounds_stp(mem_cfg);

    FWK_LOG_INFO(
        MOD_NAME "Cleared %s memory @0x%" PRIxPTR ", size=0x%zx",
        fwk_module_get_element_name(id),
        (uintptr_t)mem_cfg->base_address,
        (size_t)mem_cfg->size);

    return FWK_SUCCESS;
}

const struct fwk_module module_clear_memory = {
    .init = clear_memory_init,
    .element_init = clear_memory_element_init,
    .start = clear_memory_start,
};
