/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef MOD_VMCU_SAFETY_H
#define MOD_VMCU_SAFETY_H
#include <fwk_id.h>
#include <stdint.h>
struct mod_vmcu_safety_config {
    fwk_id_t uart_id;
    fwk_id_t alarm_id;
    fwk_id_t timer_id;
    unsigned int first_ap;
    unsigned int ap_count;
    uintptr_t gpio_base;
};
#endif
