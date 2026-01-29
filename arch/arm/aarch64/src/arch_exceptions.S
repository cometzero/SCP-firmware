/*
 * Arm SCP/MCP Software
 * Copyright (c) 2024-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_exceptions.h>

.macro ventry label
.align	7
b \label
.endm

.macro aarch64_exception_handler label, vector_id
\label:
    sub sp, sp, #AARCH64_EXCEPTION_CONTEXT_SIZE
    stp x0, x1, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X0]
    stp x2, x3, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X2]
    stp x4, x5, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X4]
    stp x6, x7, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X6]
    stp x8, x9, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X8]
    stp x10, x11, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X10]
    stp x12, x13, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X12]
    stp x14, x15, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X14]
    stp x16, x17, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X16]
    stp x18, x19, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X18]
    stp x20, x21, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X20]
    stp x22, x23, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X22]
    stp x24, x25, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X24]
    stp x26, x27, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X26]
    stp x28, x29, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X28]
    str x30, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_X30]

    add x2, sp, #AARCH64_EXCEPTION_CONTEXT_SIZE
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_SP]

    mov x2, #\vector_id
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_VECTOR]

    mrs x3, CurrentEL
    lsr x3, x3, #2
    str x3, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_CURRENT_EL]

    mrs x2, mpidr_el1
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_MPIDR_EL1]

    cmp x3, #1
    b.eq 1f
    cmp x3, #2
    b.eq 2f
#ifdef ARMV8A64_EL3_SUPPORT
    cmp x3, #3
    b.eq 3f
#endif
    b 2f
1:
    mrs x2, spsr_el1
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_SPSR_ELX]
    mrs x2, elr_el1
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_ELR_ELX]
    mrs x2, esr_el1
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_ESR_ELX]
    mrs x2, far_el1
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_FAR_ELX]
    b 4f
2:
    mrs x2, spsr_el2
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_SPSR_ELX]
    mrs x2, elr_el2
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_ELR_ELX]
    mrs x2, esr_el2
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_ESR_ELX]
    mrs x2, far_el2
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_FAR_ELX]
    b 4f
#ifdef ARMV8A64_EL3_SUPPORT
3:
    mrs x2, spsr_el3
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_SPSR_ELX]
    mrs x2, elr_el3
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_ELR_ELX]
    mrs x2, esr_el3
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_ESR_ELX]
    mrs x2, far_el3
    str x2, [sp, #AARCH64_EXCEPTION_CONTEXT_OFFSET_FAR_ELX]
#endif
4:

    mov x0, sp
    bl arch_exception_dump

1:
    wfe
    b 1b
.endm

.align 11
.global exception_vectors
.extern arch_exception_dump
exception_vectors:
    /* current EL, SP_EL0 */
    ventry sync_current_el_sp0    /* synchronous */
    ventry irq_current_el_sp0     /* IRQ */
    ventry fiq_current_el_sp0     /* FIQ */
    ventry serror_current_el_sp0  /* SError */

    /* current EL, SP_ELX */
    ventry sync_current_el_spx    /* synchronous */
    ventry irq_current_el_spx     /* IRQ */
    ventry fiq_handler            /* FIQ */
    ventry serror_current_el_spx  /* SError */

    /* lower EL using AArch64 */
    ventry sync_lower_el_aarch64   /* synchronous */
    ventry irq_lower_el_aarch64    /* IRQ */
    ventry fiq_lower_el_aarch64    /* FIQ */
    ventry serror_lower_el_aarch64 /* SError */

    /* lower EL using AArch32 */
    ventry sync_lower_el_aarch32   /* synchronous */
    ventry irq_lower_el_aarch32    /* IRQ */
    ventry fiq_lower_el_aarch32    /* FIQ */
    ventry serror_lower_el_aarch32 /* SError */

aarch64_exception_handler sync_current_el_sp0, \
    AARCH64_EXCEPTION_VECTOR_IDX_SYNC_CURRENT_EL_SP0
aarch64_exception_handler irq_current_el_sp0, \
    AARCH64_EXCEPTION_VECTOR_IDX_IRQ_CURRENT_EL_SP0
aarch64_exception_handler fiq_current_el_sp0, \
    AARCH64_EXCEPTION_VECTOR_IDX_FIQ_CURRENT_EL_SP0
aarch64_exception_handler serror_current_el_sp0, \
    AARCH64_EXCEPTION_VECTOR_IDX_SERROR_CURRENT_EL_SP0

aarch64_exception_handler sync_current_el_spx, \
    AARCH64_EXCEPTION_VECTOR_IDX_SYNC_CURRENT_EL_SPX
aarch64_exception_handler irq_current_el_spx, \
    AARCH64_EXCEPTION_VECTOR_IDX_IRQ_CURRENT_EL_SPX
aarch64_exception_handler serror_current_el_spx, \
    AARCH64_EXCEPTION_VECTOR_IDX_SERROR_CURRENT_EL_SPX

aarch64_exception_handler sync_lower_el_aarch64, \
    AARCH64_EXCEPTION_VECTOR_IDX_SYNC_LOWER_EL_AARCH64
aarch64_exception_handler irq_lower_el_aarch64, \
    AARCH64_EXCEPTION_VECTOR_IDX_IRQ_LOWER_EL_AARCH64
aarch64_exception_handler fiq_lower_el_aarch64, \
    AARCH64_EXCEPTION_VECTOR_IDX_FIQ_LOWER_EL_AARCH64
aarch64_exception_handler serror_lower_el_aarch64, \
    AARCH64_EXCEPTION_VECTOR_IDX_SERROR_LOWER_EL_AARCH64

aarch64_exception_handler sync_lower_el_aarch32, \
    AARCH64_EXCEPTION_VECTOR_IDX_SYNC_LOWER_EL_AARCH32
aarch64_exception_handler irq_lower_el_aarch32, \
    AARCH64_EXCEPTION_VECTOR_IDX_IRQ_LOWER_EL_AARCH32
aarch64_exception_handler fiq_lower_el_aarch32, \
    AARCH64_EXCEPTION_VECTOR_IDX_FIQ_LOWER_EL_AARCH32
aarch64_exception_handler serror_lower_el_aarch32, \
    AARCH64_EXCEPTION_VECTOR_IDX_SERROR_LOWER_EL_AARCH32

fiq_handler:
    /* Save registers onto stack */
    str x30, [sp, #-16]!
    stp x28, x29, [sp, #-16]!
    stp x26, x27, [sp, #-16]!
    stp x24, x25, [sp, #-16]!
    stp x22, x23, [sp, #-16]!
    stp x20, x21, [sp, #-16]!
    stp x18, x19, [sp, #-16]!
    stp x16, x17, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x8, x9, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x0, x1, [sp, #-16]!

    /* Save exception state on stack to support nested exceptions */
    mrs x0, spsr_el2
    mrs x1, elr_el2
    stp x0, x1, [sp, #-16]!

.extern irq_global
    bl irq_global

    /* Restore exception state */
    ldp x0, x1, [sp], #16
    msr spsr_el2, x0
    msr elr_el2, x1

    /* Restore registers from stack */
    ldp x0, x1, [sp], #16
    ldp x2, x3, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x8, x9, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x16, x17, [sp], #16
    ldp x18, x19, [sp], #16
    ldp x20, x21, [sp], #16
    ldp x22, x23, [sp], #16
    ldp x24, x25, [sp], #16
    ldp x26, x27, [sp], #16
    ldp x28, x29, [sp], #16
    ldr x30, [sp], #16

    eret
