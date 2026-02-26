/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "si0_mmap.h"

#include <mod_pl011.h>

#include <fwk_element.h>
#include <fwk_id.h>
#include <fwk_macros.h>
#include <fwk_module.h>

#if (PLATFORM_VARIANT == RD_ASPEN_VARIANT_RTL) && \
    (RD_ASPEN_RTL_VARIANT == RD_ASPEN_RTL_VARIANT_FPGA)
#    define PL011_BAUD_RATE_BPS 38400
#    define PL011_CLOCK_RATE_HZ (10 * FWK_MHZ)
#else
#    define PL011_BAUD_RATE_BPS 115200
#    define PL011_CLOCK_RATE_HZ (24 * FWK_MHZ)
#endif

static const struct fwk_element pl011_table[] = {
    {
        .name = "scp_uart",
        .data =
            &(struct mod_pl011_element_cfg){
                .reg_base = SI0_UART_BASE,
                .baud_rate_bps = PL011_BAUD_RATE_BPS,
                .clock_rate_hz = PL011_CLOCK_RATE_HZ,
                .clock_id = FWK_ID_NONE_INIT,
                .pd_id = FWK_ID_NONE_INIT,
            },
    },
    { 0 },
};

const struct fwk_module_config config_pl011 = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(pl011_table),
};
