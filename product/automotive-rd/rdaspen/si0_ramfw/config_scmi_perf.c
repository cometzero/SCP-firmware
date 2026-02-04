/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "si0_cfgd_dvfs.h"
#include "si0_cfgd_scmi.h"
#include "si0_cfgd_timer.h"
#include "si0_cfgd_transport.h"

#include <internal/scmi_perf.h>
#include <si0_mmap.h>

#include <mod_scmi_perf.h>
#include <mod_transport.h>

#include <fwk_module.h>
#include <fwk_module_idx.h>

#include <stdint.h>

static const struct mod_scmi_perf_domain_config
    domains[DVFS_ELEMENT_IDX_COUNT] = {
        [DVFS_ELEMENT_IDX_CLUSTER0] = {
#ifdef BUILD_HAS_SCMI_PERF_FAST_CHANNELS
         .fch_config = (struct scmi_perf_fch_config[]) {
            [MOD_SCMI_PERF_FAST_CHANNEL_LEVEL_SET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER0_LEVEL_SET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LIMIT_SET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER0_LIMIT_SET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LEVEL_GET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER0_LEVEL_GET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LIMIT_GET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER0_LIMIT_GET),
        },
        .supports_fast_channels = true,
#endif
        },
        [DVFS_ELEMENT_IDX_CLUSTER1] = {
#ifdef BUILD_HAS_SCMI_PERF_FAST_CHANNELS
         .fch_config = (struct scmi_perf_fch_config[]) {
            [MOD_SCMI_PERF_FAST_CHANNEL_LEVEL_SET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER1_LEVEL_SET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LIMIT_SET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER1_LIMIT_SET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LEVEL_GET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER1_LEVEL_GET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LIMIT_GET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER1_LIMIT_GET),
         },
         .supports_fast_channels = true,
#endif
        },
        [DVFS_ELEMENT_IDX_CLUSTER2] = {
#ifdef BUILD_HAS_SCMI_PERF_FAST_CHANNELS
         .fch_config = (struct scmi_perf_fch_config[]) {
            [MOD_SCMI_PERF_FAST_CHANNEL_LEVEL_SET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER2_LEVEL_SET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LIMIT_SET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER2_LIMIT_SET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LEVEL_GET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER2_LEVEL_GET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LIMIT_GET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER2_LIMIT_GET),
         },
         .supports_fast_channels = true,
#endif
        },
        [DVFS_ELEMENT_IDX_CLUSTER3] = {
#ifdef BUILD_HAS_SCMI_PERF_FAST_CHANNELS
         .fch_config = (struct scmi_perf_fch_config[]) {
            [MOD_SCMI_PERF_FAST_CHANNEL_LEVEL_SET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER3_LEVEL_SET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LIMIT_SET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER3_LIMIT_SET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LEVEL_GET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER3_LEVEL_GET),
            [MOD_SCMI_PERF_FAST_CHANNEL_LIMIT_GET] =
                FCH_INIT(SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER3_LIMIT_GET),
         },
         .supports_fast_channels = true,
#endif
        },
    };

const struct fwk_module_config config_scmi_perf = {
    .data = &((struct mod_scmi_perf_config){
        .domains = &domains,
        .perf_doms_count = FWK_ARRAY_SIZE(domains),
#ifdef BUILD_HAS_SCMI_PERF_FAST_CHANNELS
        .fast_channels_rate_limit = RDASPEN_FCH_RATE,
#endif
    }),
};
