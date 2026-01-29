/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <fwk_log.h>
#include <fwk_macros.h>

#include <arch_exceptions.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern char stack_bottom;
extern char __stack;

const struct aarch64_symtab aarch64_symtab __attribute__((weak)) = { 0 };

#if AARCH64_EXCEPTION_SYMTAB_MAX_SIZE > 0
extern const uint8_t
    aarch64_exception_symtab_blob[AARCH64_EXCEPTION_SYMTAB_MAX_SIZE];
#endif

static_assert(
    AARCH64_EXCEPTION_CONTEXT_OFFSET_X0 ==
        offsetof(struct aarch64_exception_context, x[0]),
    "AArch64 exception context layout mismatch");
static_assert(
    AARCH64_EXCEPTION_CONTEXT_OFFSET_X30 ==
        offsetof(struct aarch64_exception_context, x[30]),
    "AArch64 exception context layout mismatch");
static_assert(
    AARCH64_EXCEPTION_CONTEXT_OFFSET_SP ==
        offsetof(struct aarch64_exception_context, sp_elx),
    "AArch64 exception context layout mismatch");
static_assert(
    AARCH64_EXCEPTION_CONTEXT_OFFSET_VECTOR ==
        offsetof(struct aarch64_exception_context, vector),
    "AArch64 exception context layout mismatch");
static_assert(
    AARCH64_EXCEPTION_CONTEXT_OFFSET_MPIDR_EL1 ==
        offsetof(struct aarch64_exception_context, mpidr_el1),
    "AArch64 exception context layout mismatch");
static_assert(
    AARCH64_EXCEPTION_CONTEXT_OFFSET_CURRENT_EL ==
        offsetof(struct aarch64_exception_context, current_el),
    "AArch64 exception context layout mismatch");
static_assert(
    AARCH64_EXCEPTION_CONTEXT_SIZE == sizeof(struct aarch64_exception_context),
    "AArch64 exception context size mismatch");

#if (FWK_LOG_LEVEL <= FWK_LOG_LEVEL_ERROR) && !defined(FWK_LOG_BUFFERED)
static const char
    *const vector_descriptions[AARCH64_EXCEPTION_VECTOR_IDX_COUNT] = {
        [AARCH64_EXCEPTION_VECTOR_IDX_SYNC_CURRENT_EL_SP0] =
            "Synchronous exception - current EL using SP_EL0",
        [AARCH64_EXCEPTION_VECTOR_IDX_IRQ_CURRENT_EL_SP0] =
            "IRQ - current EL using SP_EL0",
        [AARCH64_EXCEPTION_VECTOR_IDX_FIQ_CURRENT_EL_SP0] =
            "FIQ - current EL using SP_EL0",
        [AARCH64_EXCEPTION_VECTOR_IDX_SERROR_CURRENT_EL_SP0] =
            "SError - current EL using SP_EL0",
        [AARCH64_EXCEPTION_VECTOR_IDX_SYNC_CURRENT_EL_SPX] =
            "Synchronous exception - current EL using SP_ELx",
        [AARCH64_EXCEPTION_VECTOR_IDX_IRQ_CURRENT_EL_SPX] =
            "IRQ - current EL using SP_ELx",
        [AARCH64_EXCEPTION_VECTOR_IDX_FIQ_CURRENT_EL_SPX] =
            "FIQ - current EL using SP_ELx",
        [AARCH64_EXCEPTION_VECTOR_IDX_SERROR_CURRENT_EL_SPX] =
            "SError - current EL using SP_ELx",
        [AARCH64_EXCEPTION_VECTOR_IDX_SYNC_LOWER_EL_AARCH64] =
            "Synchronous exception - lower EL using AArch64",
        [AARCH64_EXCEPTION_VECTOR_IDX_IRQ_LOWER_EL_AARCH64] =
            "IRQ - lower EL using AArch64",
        [AARCH64_EXCEPTION_VECTOR_IDX_FIQ_LOWER_EL_AARCH64] =
            "FIQ - lower EL using AArch64",
        [AARCH64_EXCEPTION_VECTOR_IDX_SERROR_LOWER_EL_AARCH64] =
            "SError - lower EL using AArch64",
        [AARCH64_EXCEPTION_VECTOR_IDX_SYNC_LOWER_EL_AARCH32] =
            "Synchronous exception - lower EL using AArch32",
        [AARCH64_EXCEPTION_VECTOR_IDX_IRQ_LOWER_EL_AARCH32] =
            "IRQ - lower EL using AArch32",
        [AARCH64_EXCEPTION_VECTOR_IDX_FIQ_LOWER_EL_AARCH32] =
            "FIQ - lower EL using AArch32",
        [AARCH64_EXCEPTION_VECTOR_IDX_SERROR_LOWER_EL_AARCH32] =
            "SError - lower EL using AArch32",
    };
static_assert(
    FWK_ARRAY_SIZE(vector_descriptions) == AARCH64_EXCEPTION_VECTOR_IDX_COUNT,
    "Vector description count mismatch");

static const struct aarch64_symtab *get_symtab(void)
{
#    if AARCH64_EXCEPTION_SYMTAB_MAX_SIZE > 0
    static struct aarch64_symtab cached_symtab;
    static bool cached;
    const struct aarch64_symtab_blob_header *hdr;
    const uint8_t *blob = aarch64_exception_symtab_blob;
    uint32_t entries_size;
    uint32_t total_size;

    if (!cached) {
        cached = true;
        if (AARCH64_EXCEPTION_SYMTAB_MAX_SIZE <
            sizeof(struct aarch64_symtab_blob_header)) {
            goto fallback;
        }

        hdr = (const struct aarch64_symtab_blob_header *)blob;
        if (hdr->magic != 0x5343594d) { /* "SCYM" */
            goto fallback;
        }

        entries_size = hdr->count * sizeof(struct aarch64_symtab_entry);
        total_size = hdr->strtab_off + hdr->strtab_size;
        if ((hdr->strtab_off <
             sizeof(struct aarch64_symtab_blob_header) + entries_size) ||
            (total_size > AARCH64_EXCEPTION_SYMTAB_MAX_SIZE)) {
            goto fallback;
        }

        cached_symtab.count = hdr->count;
        cached_symtab.entries =
            (const struct aarch64_symtab_entry
                 *)(blob + sizeof(struct aarch64_symtab_blob_header));
        cached_symtab.strtab = (const char *)(blob + hdr->strtab_off);

        return &cached_symtab;
    }

    if (cached_symtab.count != 0) {
        return &cached_symtab;
    }

fallback:
#    endif
    if ((aarch64_symtab.count == 0) || (aarch64_symtab.entries == NULL) ||
        (aarch64_symtab.strtab == NULL)) {
        return NULL;
    }

    return &aarch64_symtab;
}

static const char *lookup_symbol(
    uint64_t addr,
    uint64_t *sym_addr,
    uint32_t *sym_size)
{
    const struct aarch64_symtab *symtab = get_symtab();
    size_t lo;
    size_t hi;

    if (symtab == NULL) {
        return NULL;
    }

    lo = 0;
    hi = symtab->count;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) / 2);

        if (symtab->entries[mid].addr <= addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    if (lo == 0) {
        return NULL;
    }

    const struct aarch64_symtab_entry *entry = &symtab->entries[lo - 1];
    if ((entry->size != 0) && (addr >= (entry->addr + entry->size))) {
        return NULL;
    }

    if (sym_addr != NULL) {
        *sym_addr = entry->addr;
    }
    if (sym_size != NULL) {
        *sym_size = entry->size;
    }

    return symtab->strtab + entry->name_off;
}

static void log_trace_entry(uint64_t addr, bool is_fault)
{
    uint64_t sym_addr = 0;
    const char *name = lookup_symbol(addr, &sym_addr, NULL);

    if (name != NULL) {
        FWK_LOG_ERR(
            "  [<0x%016" PRIx64 ">] %s+0x%" PRIx64 "%s",
            addr,
            name,
            addr - sym_addr,
            is_fault ? " (fault)" : "");
    } else {
        FWK_LOG_ERR(
            "  [<0x%016" PRIx64 ">]%s", addr, is_fault ? " (fault)" : "");
    }
}

static void dump_backtrace(const struct aarch64_exception_context *context)
{
    const uint64_t lower = (uint64_t)(uintptr_t)&stack_bottom;
    const uint64_t upper = (uint64_t)(uintptr_t)&__stack;
    uint64_t fp = context->x[29];
    unsigned int depth;

    if ((upper - lower) < 32) {
        return;
    }

    FWK_LOG_ERR("Call trace:");

    /* First entry: faulting PC */
    log_trace_entry(context->elr_el2, true);

    for (depth = 0; depth < AARCH64_EXCEPTION_MAX_BACKTRACE_DEPTH; depth++) {
        if ((fp < (lower + AARCH64_EXCEPTION_FRAME_RECORD_SIZE)) ||
            ((fp + AARCH64_EXCEPTION_FRAME_RECORD_SIZE) > upper) ||
            ((fp & (AARCH64_EXCEPTION_STACK_ALIGNMENT - 1u)) != 0)) {
            break;
        }

        uint64_t next_fp = *((uint64_t *)fp);
        uint64_t lr = *((uint64_t *)(fp + 8));

        log_trace_entry(lr, false);

        if (next_fp <= fp) {
            break;
        }

        fp = next_fp;
    }
}

static void dump_stack(const struct aarch64_exception_context *context)
{
    const uint64_t lower = (uint64_t)(uintptr_t)&stack_bottom;
    const uint64_t upper = (uint64_t)(uintptr_t)&__stack;
    uint64_t start = context->sp_elx;
    uint64_t end;
    uint64_t addr;

    if (start < lower) {
        start = lower;
    } else if (start >= upper) {
        return;
    }

    end = context->sp_elx + AARCH64_EXCEPTION_STACK_DUMP_SIZE;
    if (end > upper) {
        end = upper;
    }

    start = (start + (AARCH64_EXCEPTION_STACK_ALIGNMENT - 1u)) &
        ~(uint64_t)(AARCH64_EXCEPTION_STACK_ALIGNMENT - 1u);
    if ((end - start) < AARCH64_EXCEPTION_FRAME_RECORD_SIZE) {
        return;
    }

    FWK_LOG_ERR("Stack: 0x%016" PRIx64 " to 0x%016" PRIx64 "", start, end);

    for (addr = start; (addr + AARCH64_EXCEPTION_FRAME_RECORD_SIZE) <= end;
         addr += AARCH64_EXCEPTION_FRAME_RECORD_SIZE) {
        const uint64_t *words = (const uint64_t *)(uintptr_t)addr;

        FWK_LOG_ERR(
            "  0x%016" PRIx64 ": 0x%016" PRIx64 " 0x%016" PRIx64,
            addr,
            words[0],
            words[1]);
    }
}

void arch_exception_dump(const struct aarch64_exception_context *context)
{
    const char *vector = "Unknown";
    unsigned int idx;

    if ((context->vector < FWK_ARRAY_SIZE(vector_descriptions)) &&
        (vector_descriptions[context->vector] != NULL)) {
        vector = vector_descriptions[context->vector];
    }

    FWK_LOG_ERR("Unhandled exception: %s", vector);
    FWK_LOG_ERR(
        "ELR_EL%" PRIu64 "  : 0x%016" PRIx64 "  SPSR_EL%" PRIu64
        ": 0x%016" PRIx64,
        context->current_el,
        context->elr_el2,
        context->current_el,
        context->spsr_el2);
    FWK_LOG_ERR(
        "ESR_EL%" PRIu64 "  : 0x%016" PRIx64 "   FAR_EL%" PRIu64
        ": 0x%016" PRIx64,
        context->current_el,
        context->esr_el2,
        context->current_el,
        context->far_el2);
    FWK_LOG_ERR("MPIDR_EL1: 0x%016" PRIx64, context->mpidr_el1);
    FWK_LOG_ERR("SP_ELx   : 0x%016" PRIx64, context->sp_elx);

    for (idx = 0; idx < 30; idx += 2) {
        FWK_LOG_ERR(
            "x%-2u: 0x%016" PRIx64 " x%-2u: 0x%016" PRIx64,
            idx,
            context->x[idx],
            idx + 1,
            context->x[idx + 1]);
    }

    FWK_LOG_ERR("x30: 0x%016" PRIx64, context->x[30]);

    dump_backtrace(context);
    dump_stack(context);

    fwk_log_flush();
}
#else
void arch_exception_dump(const struct aarch64_exception_context *context)
{
    (void)context;
}
#endif
