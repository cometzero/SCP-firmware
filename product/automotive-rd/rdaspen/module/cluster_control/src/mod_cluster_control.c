/*
 * Arm SCP/MCP Software
 * Copyright (c) 2025-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "internal/cluster_control_reg.h"

#include <mod_cluster_control.h>

#include <fwk_assert.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_macros.h>
#include <fwk_module.h>
#include <fwk_notification.h>

#include <stdint.h>

#define MOD_NAME "[CLUSTER_CONTROL] "

#define BROADCAST_BROADCASTMTE_BIT 3U

static int cluster_control_configure(
    fwk_id_t element_id,
    const struct mod_cluster_control_element_config *config)
{
    struct cluster_control_reg *reg;
    uint32_t rvbar_lw, rvbar_up;

    /* Compute the lower and upper field values from the 64-bit configuration
     * value */
    rvbar_lw = (uint32_t)(config->rvbar & UINT32_MAX);
    rvbar_up = (uint32_t)(config->rvbar >> 32);

    reg = (struct cluster_control_reg *)config->region;

    FWK_RW uint32_t *const astart[] = {
        &reg->ASTART0,
        &reg->ASTART1,
        &reg->ASTART2,
        &reg->ASTART3,
    };
    FWK_RW uint32_t *const aend[] = {
        &reg->AEND0,
        &reg->AEND1,
        &reg->AEND2,
        &reg->AEND3,
    };

    /* Enable MTE2 CPU feature as it is needed for the STL */
    reg->BROADCAST |= 1UL << BROADCAST_BROADCASTMTE_BIT;

    for (uint8_t i = 0; i < CLUSTER_CONTROL_PORT_REGION_COUNT; i++) {
        /* Allow peripheral port access */
        *astart[i] = config->astart[i];
        *aend[i] = config->aend[i];
    }

    reg->PE0_RVBARADDR_LW = rvbar_lw;
    reg->PE0_RVBARADDR_UP = rvbar_up;
    reg->PE1_RVBARADDR_LW = rvbar_lw;
    reg->PE1_RVBARADDR_UP = rvbar_up;
    reg->PE2_RVBARADDR_LW = rvbar_lw;
    reg->PE2_RVBARADDR_UP = rvbar_up;
    reg->PE3_RVBARADDR_LW = rvbar_lw;
    reg->PE3_RVBARADDR_UP = rvbar_up;

    FWK_LOG_INFO(
        MOD_NAME "%s Cluster control registers initialized",
        fwk_module_get_element_name(element_id));

    return FWK_SUCCESS;
}

static int cluster_control_init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *data)
{
    fwk_assert(element_count != 0);
    fwk_assert(data != NULL);

    return FWK_SUCCESS;
}

static int cluster_control_element_init(
    fwk_id_t element_id,
    unsigned int sub_element_count,
    const void *data)
{
    fwk_assert(sub_element_count == 0);
    fwk_assert(data != NULL);

    return FWK_SUCCESS;
}

static int cluster_control_start(fwk_id_t id)
{
    int status;
    const struct mod_cluster_control_config *mod_config;
    const struct mod_cluster_control_element_config *element_config;

    if (fwk_id_is_type(id, FWK_ID_TYPE_MODULE)) {
        return FWK_SUCCESS;
    }

    mod_config = fwk_module_get_data(fwk_module_id_cluster_control);

    if ((fwk_id_type_is_valid(mod_config->platform_notification.source_id)) &&
        (!fwk_id_is_equal(
            mod_config->platform_notification.source_id, FWK_ID_NONE))) {
        status = fwk_notification_subscribe(
            mod_config->platform_notification.notification_id,
            mod_config->platform_notification.source_id,
            id);
        if (status != FWK_SUCCESS) {
            FWK_LOG_CRIT(MOD_NAME
                         "Failed to subscribe platform "
                         "notification");
        }

        return status;
    }

    element_config = fwk_module_get_data(id);

    return cluster_control_configure(id, element_config);
}

static int cluster_control_process_notification(
    const struct fwk_event *event,
    struct fwk_event *resp_event)
{
    const struct mod_cluster_control_element_config *element_config;
    const struct mod_cluster_control_config *module_config =
        fwk_module_get_data(fwk_module_id_cluster_control);
    int status = FWK_SUCCESS;

    fwk_assert(fwk_id_is_type(event->target_id, FWK_ID_TYPE_ELEMENT));

    if (fwk_id_is_equal(
            event->id, module_config->platform_notification.notification_id)) {
        status = fwk_notification_unsubscribe(
            event->id, event->source_id, event->target_id);
        if (status != FWK_SUCCESS) {
            FWK_LOG_CRIT(MOD_NAME
                         "Failed to unsubscribe platform "
                         "notification");
            return status;
        }

        element_config = fwk_module_get_data(event->target_id);

        status = cluster_control_configure(event->target_id, element_config);
    }

    return status;
}

const struct fwk_module module_cluster_control = {
    .type = FWK_MODULE_TYPE_DRIVER,
    .init = cluster_control_init,
    .element_init = cluster_control_element_init,
    .start = cluster_control_start,
    .process_notification = cluster_control_process_notification,
};
