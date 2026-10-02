/*
 * Arm SCP/MCP Software
 * Copyright (c) 2022-2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_HELPERS_H
#define ARCH_HELPERS_H
/*
 * This variable is used to ensure spurious nested calls won't
 * enable interrupts. This is been defined in fwk_test.c
 */

extern unsigned int critical_section_nest_level;

#ifdef BUILD_TEST_IDLE_INTERRUPTS
void test_arch_interrupts_enable(unsigned int flags);
unsigned int test_arch_interrupts_disable(void);
#endif

/*!
 * \brief Enables global CPU interrupts. (stub)
 *
 */
inline static void arch_interrupts_enable(unsigned int not_used)
{
#ifdef BUILD_TEST_IDLE_INTERRUPTS
    test_arch_interrupts_enable(not_used);
#else
    /* Decrement critical_section_nest_level only if in critical section */
    if (critical_section_nest_level > 0) {
        critical_section_nest_level--;
    }
#endif
}

/*!
 * \brief Disables global CPU interrupts. (stub)
 *
 */
inline static unsigned int arch_interrupts_disable(void)
{
#ifdef BUILD_TEST_IDLE_INTERRUPTS
    return test_arch_interrupts_disable();
#else
    critical_section_nest_level++;

    return 0;
#endif
}

/*!
 * \brief Suspend execution of current CPU.
 *
 */
inline static void arch_suspend(void)
{
}

int arch_interrupt_init(void);

#endif /* ARCH_HELPERS_H */
