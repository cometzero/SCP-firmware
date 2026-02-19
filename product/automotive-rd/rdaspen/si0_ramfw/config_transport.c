/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Configuration data for module 'transport'.
 */

#include "platform_core.h"
#include "si0_cfgd_mhu3.h"
#include "si0_cfgd_power_domain.h"
#include "si0_cfgd_scmi.h"
#include "si0_cfgd_transport.h"
#include "si0_mmap.h"

#include <mod_fch_polled.h>
#include <mod_mhu3.h>
#include <mod_ras_handlers.h>
#include <mod_si0_platform.h>
#include <mod_transport.h>

#include <fwk_element.h>
#include <fwk_id.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

/* Secure transport channel with mailbox initialization policy */
#define TRANSPORT_CH_SEC_MBX_INIT \
    (MOD_TRANSPORT_POLICY_INIT_MAILBOX | MOD_TRANSPORT_POLICY_SECURE)

/* Subsystem initialized notification id (platform notification) */
#define PLATFORM_SI0_NOTIFICATION_ID \
    FWK_ID_NOTIFICATION_INIT( \
        FWK_MODULE_IDX_SI0_PLATFORM, \
        MOD_SI0_PLATFORM_NOTIFICATION_IDX_SUBSYS_INITIALIZED)

#define TRANSPORT_PFDI_MONITOR_AP(cluster, core) \
    { \
        .name = "TRANSPORT_PFDI_MONITOR_AP_CLUSTER_" #cluster "_CORE_" #core, \
        .data = &((struct mod_transport_channel_config){ \
            .transport_type = MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_OUT_BAND, \
            .policies = TRANSPORT_CH_SEC_MBX_INIT, \
            .channel_type = MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER, \
            .out_band_mailbox_address = \
                (uintptr_t)SI0_SCMI_PFDI_MONITOR_S_A2P_BASE + \
                (((cluster * CORES_PER_CLUSTER) + core) * \
                 SI0_SCMI_PFDI_MONITOR_SIZE_CORE), \
            .out_band_mailbox_size = SI0_SCMI_PFDI_MONITOR_SIZE_CORE, \
            .driver_id = FWK_ID_SUB_ELEMENT_INIT( \
                FWK_MODULE_IDX_MHU3, \
                SI0_CFGD_MOD_MHU3_EIDX_SI0_AP_DOMAIN3_S, \
                SI0_CFGD_MOD_MHU3_SI0_AP_S_EIDX_PFDI_MONITOR_AP_CLUSTER_##cluster##_CORE_##core), \
            .driver_api_id = FWK_ID_API_INIT( \
                FWK_MODULE_IDX_MHU3, MOD_MHU3_API_IDX_TRANSPORT_DRIVER), \
        }), \
    }
#if !RD_ASPEN_VARIANT_CFG1
#    define TRANSPORT_PFDI_MONITOR_SI_CL1(core) \
        { \
            .name = "TRANSPORT_PFDI_MONITOR_SI_CLUSTER1_CORE_" #core, \
            .data = &((struct mod_transport_channel_config){ \
                .transport_type = \
                    MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_OUT_BAND, \
                .policies = TRANSPORT_CH_SEC_MBX_INIT, \
                .channel_type = MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER, \
                .out_band_mailbox_address = \
                    (uintptr_t)SI0_SCMI_PFDI_MONITOR_CL1_2_CL0_BASE + \
                    ((core)*SI0_SCMI_PFDI_MONITOR_SIZE_CORE), \
                .out_band_mailbox_size = SI0_SCMI_PFDI_MONITOR_SIZE_CORE, \
                .driver_id = FWK_ID_SUB_ELEMENT_INIT( \
                    FWK_MODULE_IDX_MHU3, \
                    SI0_CFGD_MOD_MHU3_EIDX_CL1_CL0, \
                    SI0_CFGD_MOD_MHU3_SI0_CL1_EIDX_PFDI_MONITOR_SI_CL1_CORE##core), \
                .driver_api_id = FWK_ID_API_INIT( \
                    FWK_MODULE_IDX_MHU3, MOD_MHU3_API_IDX_TRANSPORT_DRIVER), \
            }), \
        }
#endif /* RD_ASPEN_VARIANT_CFG1 */

/* Module 'transport' element configuration table */
static const struct fwk_element element_table[]  = {
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_RSE] = {
        .name = "SI0_RSE_SCMI_TRANSPORT",
        .data = &((
            struct mod_transport_channel_config) {
                .transport_type = MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_OUT_BAND,
                .policies = TRANSPORT_CH_SEC_MBX_INIT,
                .channel_type = MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
                .out_band_mailbox_address =
                    (uintptr_t) SI0_RSE_SCMI_PAYLOAD_BASE,
                .out_band_mailbox_size = SI0_SCMI_PAYLOAD_SIZE,
                .driver_id =
                    FWK_ID_SUB_ELEMENT_INIT(
                        FWK_MODULE_IDX_MHU3,
                        SI0_CFGD_MOD_MHU3_EIDX_SI0_RSE,
                        0),
                .driver_api_id =
                    FWK_ID_API_INIT(
                        FWK_MODULE_IDX_MHU3,
                        MOD_MHU3_API_IDX_TRANSPORT_DRIVER),
                .platform_notification = {
                    .notification_id = FWK_ID_NONE,
                    .source_id = FWK_ID_NONE,
                },
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PSCI] = {
        .name = "PSCI",
        .data = &((
            struct mod_transport_channel_config) {
                .transport_type = MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_OUT_BAND,
                .policies = TRANSPORT_CH_SEC_MBX_INIT,
                .channel_type = MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
                .out_band_mailbox_address =
                    (uintptr_t) SI0_SCMI_PAYLOAD_S_A2P_BASE,
                .out_band_mailbox_size = SI0_SCMI_PAYLOAD_SIZE,
                .driver_id =
                    FWK_ID_SUB_ELEMENT_INIT(
                        FWK_MODULE_IDX_MHU3,
                        SI0_CFGD_MOD_MHU3_EIDX_SI0_AP_DOMAIN1_S,
                        0),
                .driver_api_id =
                    FWK_ID_API_INIT(
                        FWK_MODULE_IDX_MHU3,
                        MOD_MHU3_API_IDX_TRANSPORT_DRIVER),
                .platform_notification = {
                    .notification_id = PLATFORM_SI0_NOTIFICATION_ID,
                    .source_id = FWK_ID_MODULE_INIT(
                        FWK_MODULE_IDX_SI0_PLATFORM),
                },
        }),
    },
#ifdef BUILD_HAS_SCMI_NOTIFICATIONS
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_RSE_P2A] = {
        .name = "SI0_RSE_SCMI_TRANSPORT_P2A",
        .data = &((
            struct mod_transport_channel_config) {
                .transport_type = MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_OUT_BAND,
                .policies = TRANSPORT_CH_SEC_MBX_INIT,
                .channel_type = MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
                .out_band_mailbox_address =
                    (uintptr_t) SI0_RSE_SCMI_P2A_PAYLOAD_BASE,
                .out_band_mailbox_size = SI0_SCMI_PAYLOAD_SIZE,
                .driver_id =
                    FWK_ID_SUB_ELEMENT_INIT(
                        FWK_MODULE_IDX_MHU3,
                        SI0_CFGD_MOD_MHU3_EIDX_SI0_RSE,
                        1),
                .driver_api_id =
                    FWK_ID_API_INIT(
                        FWK_MODULE_IDX_MHU3,
                        MOD_MHU3_API_IDX_TRANSPORT_DRIVER),
        }),
    },
#endif
#if !RD_ASPEN_VARIANT_CFG1
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_SI_CLUSTER1_CORE_0] = TRANSPORT_PFDI_MONITOR_SI_CL1(0),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_SI_CLUSTER1_CORE_1] = TRANSPORT_PFDI_MONITOR_SI_CL1(1),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_SI_CLUSTER1_CORE_2] = TRANSPORT_PFDI_MONITOR_SI_CL1(2),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_SI_CLUSTER1_CORE_3] = TRANSPORT_PFDI_MONITOR_SI_CL1(3),
#endif /* RD_ASPEN_VARIANT_CFG1 */
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_0_CORE_0] = TRANSPORT_PFDI_MONITOR_AP(0, 0),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_0_CORE_1] = TRANSPORT_PFDI_MONITOR_AP(0, 1),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_0_CORE_2] = TRANSPORT_PFDI_MONITOR_AP(0, 2),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_0_CORE_3] = TRANSPORT_PFDI_MONITOR_AP(0, 3),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_1_CORE_0] = TRANSPORT_PFDI_MONITOR_AP(1, 0),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_1_CORE_1] = TRANSPORT_PFDI_MONITOR_AP(1, 1),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_1_CORE_2] = TRANSPORT_PFDI_MONITOR_AP(1, 2),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_1_CORE_3] = TRANSPORT_PFDI_MONITOR_AP(1, 3),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_2_CORE_0] = TRANSPORT_PFDI_MONITOR_AP(2, 0),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_2_CORE_1] = TRANSPORT_PFDI_MONITOR_AP(2, 1),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_2_CORE_2] = TRANSPORT_PFDI_MONITOR_AP(2, 2),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_2_CORE_3] = TRANSPORT_PFDI_MONITOR_AP(2, 3),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_3_CORE_0] = TRANSPORT_PFDI_MONITOR_AP(3, 0),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_3_CORE_1] = TRANSPORT_PFDI_MONITOR_AP(3, 1),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_3_CORE_2] = TRANSPORT_PFDI_MONITOR_AP(3, 2),
    [SI0_CFGD_MOD_TRANSPORT_EIDX_PFDI_MONITOR_AP_CLUSTER_3_CORE_3] = TRANSPORT_PFDI_MONITOR_AP(3, 3),
    [SI0_CFGD_MOD_TRANSPORT_IDX_OSPM_A2P] = {
        .name = "OSPM_A2P",
        .data = &((
            struct mod_transport_channel_config) {
                .transport_type = MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_OUT_BAND,
                .policies = MOD_TRANSPORT_POLICY_INIT_MAILBOX,
                .channel_type = MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
                .out_band_mailbox_address =
                    (uintptr_t) SI0_AP_SCMI_PAYLOAD_NS_A2P_BASE,
                .out_band_mailbox_size = SI0_SCMI_PAYLOAD_SIZE,
                .driver_id =
                    FWK_ID_SUB_ELEMENT_INIT(
                        FWK_MODULE_IDX_MHU3,
                        SI0_CFGD_MOD_MHU3_EIDX_SI0_AP_NS,
                        1),
                .driver_api_id =
                    FWK_ID_API_INIT(
                        FWK_MODULE_IDX_MHU3,
                        MOD_MHU3_API_IDX_TRANSPORT_DRIVER),
        }),
    },
#ifdef BUILD_HAS_SCMI_NOTIFICATIONS
    [SI0_CFGD_MOD_TRANSPORT_IDX_OSPM_P2A] = {
        .name = "OSPM_P2A",
        .data = &((
            struct mod_transport_channel_config) {
                .transport_type = MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_OUT_BAND,
                .policies = MOD_TRANSPORT_POLICY_INIT_MAILBOX,
                .channel_type = MOD_TRANSPORT_CHANNEL_TYPE_REQUESTER,
                .out_band_mailbox_address =
                    (uintptr_t) SI0_AP_SCMI_PAYLOAD_NS_P2A_BASE,
                .out_band_mailbox_size = SI0_SCMI_PAYLOAD_SIZE,
                .driver_id =
                    FWK_ID_SUB_ELEMENT_INIT(
                        FWK_MODULE_IDX_MHU3,
                        SI0_CFGD_MOD_MHU3_EIDX_SI0_AP_NS,
                        2),
                .driver_api_id =
                    FWK_ID_API_INIT(
                        FWK_MODULE_IDX_MHU3,
                        MOD_MHU3_API_IDX_TRANSPORT_DRIVER),
        }),
    },
#endif
#ifdef BUILD_HAS_MOD_TRANSPORT_FC
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER0_LEVEL_SET] = {
        .name = "FCH_CLUSTER0_PERF_LEVEL_SET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER0_PERF_LEVEL_SET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER0_LIMIT_SET] = {
        .name = "FCH_CLUSTER0_PERF_LIMIT_SET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER0_PERF_LIMIT_SET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER0_LEVEL_GET] = {
        .name = "FCH_CLUSTER0_PERF_LEVEL_GET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER0_PERF_LEVEL_GET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER0_LIMIT_GET] = {
        .name = "FCH_CLUSTER0_PERF_LIMIT_GET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER0_PERF_LIMIT_GET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },

    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER1_LEVEL_SET] = {
        .name = "FCH_CLUSTER1_PERF_LEVEL_SET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER1_PERF_LEVEL_SET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER1_LIMIT_SET] = {
        .name = "FCH_CLUSTER1_PERF_LIMIT_SET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER1_PERF_LIMIT_SET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER1_LEVEL_GET] = {
        .name = "FCH_CLUSTER1_PERF_LEVEL_GET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER1_PERF_LEVEL_GET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER1_LIMIT_GET] = {
        .name = "FCH_CLUSTER1_PERF_LIMIT_GET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER1_PERF_LIMIT_GET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },

    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER2_LEVEL_SET] = {
        .name = "FCH_CLUSTER2_PERF_LEVEL_SET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER2_PERF_LEVEL_SET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER2_LIMIT_SET] = {
        .name = "FCH_CLUSTER2_PERF_LIMIT_SET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER2_PERF_LIMIT_SET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER2_LEVEL_GET] = {
        .name = "FCH_CLUSTER2_PERF_LEVEL_GET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER2_PERF_LEVEL_GET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER2_LIMIT_GET] = {
        .name = "FCH_CLUSTER2_PERF_LIMIT_GET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER2_PERF_LIMIT_GET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },

    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER3_LEVEL_SET] = {
        .name = "FCH_CLUSTER3_PERF_LEVEL_SET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER3_PERF_LEVEL_SET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER3_LIMIT_SET] = {
        .name = "FCH_CLUSTER3_PERF_LIMIT_SET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER3_PERF_LIMIT_SET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER3_LEVEL_GET] = {
        .name = "FCH_CLUSTER3_PERF_LEVEL_GET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER3_PERF_LEVEL_GET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
    [SI0_CFGD_MOD_TRANSPORT_EIDX_SCMI_PERF_FCH_CLUSTER3_LIMIT_GET] = {
        .name = "FCH_CLUSTER3_PERF_LIMIT_GET",
        .data = &((
            struct mod_transport_channel_config){
            .transport_type =
                MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_FAST_CHANNELS,
            .channel_type =
                MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,
            .driver_id = FWK_ID_ELEMENT_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                RDASPEN_PLAT_FCH_CLUSTER3_PERF_LIMIT_GET),
            .driver_api_id = FWK_ID_API_INIT(
                FWK_MODULE_IDX_FCH_POLLED,
                MOD_FCH_POLLED_API_IDX_TRANSPORT),
        }),
    },
#endif

    [SI0_CFGD_MOD_TRANSPORT_EIDX_RSE_WARM_SYNC] = {
        .name = "SI0_RSE_WARM_SYNC",
        .data = &((
            struct mod_transport_channel_config) {
                .transport_type = MOD_TRANSPORT_CHANNEL_TRANSPORT_TYPE_NONE,
                .policies = MOD_TRANSPORT_POLICY_NONE,
                .channel_type = MOD_TRANSPORT_CHANNEL_TYPE_COMPLETER,

                /* Use DBCH index 2: PBX FLAG 3 / MBX FLAG 3 */
                .driver_id =
                    FWK_ID_SUB_ELEMENT_INIT(
                        FWK_MODULE_IDX_MHU3,
                        SI0_CFGD_MOD_MHU3_EIDX_SI0_RSE,
                        2),
                .driver_api_id =
                    FWK_ID_API_INIT(
                        FWK_MODULE_IDX_MHU3,
                        MOD_MHU3_API_IDX_TRANSPORT_DRIVER),

                .signal_api_id =
                    FWK_ID_API_INIT(
                        FWK_MODULE_IDX_SI0_PLATFORM,
                        MOD_SCP_PLATFORM_API_IDX_TRANSPORT_SIGNAL),
        }),
    },

    [SI0_CFGD_MOD_TRANSPORT_EIDX_COUNT] = { 0 },
};

const struct fwk_module_config config_transport = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(element_table),
};
