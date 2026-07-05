/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SI0_SCMI_PERF_H
#define SI0_SCMI_PERF_H

#include "si0_mmap.h"

#include <mod_fch_polled.h>
#include <mod_scmi_perf.h>

#include <stdint.h>

#define FCH_POLLED_LEVEL_SET_LENGTH sizeof(uint32_t)
#define FCH_POLLED_LIMIT_SET_LENGTH \
    sizeof(struct mod_scmi_perf_fast_channel_limit)
#define FCH_POLLED_LEVEL_GET_LENGTH sizeof(uint32_t)
#define FCH_POLLED_LIMIT_GET_LENGTH \
    sizeof(struct mod_scmi_perf_fast_channel_limit)

#endif /* SI0_SCMI_PERF_H */
