/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <internal/fwk_context.h>
#include <internal/fwk_core.h>

#include <fwk_event.h>
#include <fwk_list.h>
#include <fwk_macros.h>
#include <fwk_status.h>
#include <fwk_test.h>

#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>

#define SAVED_INTERRUPT_FLAGS 0x40000001U

static jmp_buf test_context;
static struct __fwk_ctx *ctx;
static struct fwk_event event;
static struct fwk_slist *inject_queue;
static bool interrupts_masked;
static bool interrupt_pending;
static bool interrupt_serviced;
static unsigned int disable_count;
static unsigned int suspend_count;
static unsigned int log_count;
static int log_status;

unsigned int test_arch_interrupts_disable(void)
{
    assert(!interrupts_masked);
    disable_count++;
    if ((disable_count == 2) && (inject_queue != NULL)) {
        /* An ISR ran after the drain and before the final idle check. */
        fwk_list_push_tail(inject_queue, &event.slist_node);
    }
    interrupts_masked = true;
    return SAVED_INTERRUPT_FLAGS;
}

void test_arch_interrupts_enable(unsigned int flags)
{
    assert(flags == SAVED_INTERRUPT_FLAGS);
    assert(interrupts_masked);
    interrupts_masked = false;
    if (disable_count == 2) {
        if (interrupt_pending) {
            interrupt_serviced = true;
            fwk_list_push_tail(&ctx->isr_event_queue, &event.slist_node);
        }
        longjmp(test_context, 1);
    }
}

int __wrap_fwk_log_unbuffer(void)
{
    assert(interrupts_masked);
    log_count++;
    return log_status;
}

void __wrap_fwk_arch_suspend(void)
{
    assert(interrupts_masked);
    assert(disable_count == 2);
    assert(fwk_list_is_empty(&ctx->event_queue));
    assert(fwk_list_is_empty(&ctx->isr_event_queue));
    assert(log_count == 1);
    assert(log_status == FWK_SUCCESS);
    suspend_count++;
    /* Model an interrupt becoming pending while DAIF remains masked. */
    interrupt_pending = true;
}

static void test_case_setup(void)
{
    ctx = __fwk_get_ctx();
    *ctx = (struct __fwk_ctx){ 0 };
    fwk_list_init(&ctx->event_queue);
    fwk_list_init(&ctx->isr_event_queue);
    event = (struct fwk_event){ 0 };
    inject_queue = NULL;
    interrupts_masked = false;
    interrupt_pending = false;
    interrupt_serviced = false;
    disable_count = 0;
    suspend_count = 0;
    log_count = 0;
    log_status = FWK_SUCCESS;
}

static void run_idle_iteration(void)
{
    if (setjmp(test_context) == 0) {
        __fwk_run_main_loop();
    }
    assert(!interrupts_masked);
    assert(disable_count == 2);
}

static void test_idle_interrupt_wakeup(void)
{
    run_idle_iteration();
    assert(suspend_count == 1);
    assert(interrupt_serviced);
    assert(ctx->isr_event_queue.head == &event.slist_node);
}

static void test_idle_skips_isr_work(void)
{
    inject_queue = &ctx->isr_event_queue;
    run_idle_iteration();
    assert(suspend_count == 0);
    assert(log_count == 0);
    assert(ctx->isr_event_queue.head == &event.slist_node);
}

static void test_idle_skips_framework_work(void)
{
    inject_queue = &ctx->event_queue;
    run_idle_iteration();
    assert(suspend_count == 0);
    assert(log_count == 0);
    assert(ctx->event_queue.head == &event.slist_node);
}

static void test_idle_drains_pending_log(void)
{
    log_status = FWK_PENDING;
    run_idle_iteration();
    assert(suspend_count == 0);
    assert(log_count == 1);
}

static void test_idle_skips_log_error(void)
{
    log_status = FWK_E_BUSY;
    run_idle_iteration();
    assert(suspend_count == 0);
    assert(log_count == 1);
}

static const struct fwk_test_case_desc test_case_table[] = {
    FWK_TEST_CASE(test_idle_interrupt_wakeup),
    FWK_TEST_CASE(test_idle_skips_isr_work),
    FWK_TEST_CASE(test_idle_skips_framework_work),
    FWK_TEST_CASE(test_idle_drains_pending_log),
    FWK_TEST_CASE(test_idle_skips_log_error),
};

struct fwk_test_suite_desc test_suite = {
    .name = "fwk_idle",
    .test_case_setup = test_case_setup,
    .test_case_count = FWK_ARRAY_SIZE(test_case_table),
    .test_case_table = test_case_table,
};
