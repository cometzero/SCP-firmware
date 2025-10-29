/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "platform_core.h"
#include "si0_smcf.h"

#include <si0_cfgd_power_domain.h>

#include <mod_amu_smcf_drv.h>
#include <mod_platform_smcf.h>
#include <mod_power_domain.h>
#include <mod_sensor.h>
#include <mod_sensor_smcf_drv.h>
#include <mod_smcf.h>
#include <mod_smcf_client.h>

#include <interface_amu.h>

#include <fwk_core.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#ifdef BUILD_HAS_NOTIFICATION
#    include <fwk_notification.h>
#endif /* BUILD_HAS_NOTIFICATION */
#include <fwk_status.h>

#define MOD_NAME "[SMCF_CLIENT] "

static int print_is_on_flag = 0;

#ifdef BUILD_HAS_NOTIFICATION
static fwk_id_t pd_transition_notification_id = FWK_ID_NOTIFICATION_INIT(
    FWK_MODULE_IDX_POWER_DOMAIN,
    MOD_PD_NOTIFICATION_IDX_POWER_STATE_TRANSITION);
#endif /* BUILD_HAS_NOTIFICATION */

/* Module context */
struct si0_smcf_client_ctx {
    size_t mgi_count;
    struct smcf_data_api *platform_sampling_api;
    struct smcf_control_api *control_api;
    struct amu_api *amu_data_api;
    struct mod_sensor_smcf_drv_multiple_samples_api *sensor_data_api;
    const struct mod_smcf_client_mgi_conf **mgi_conf;
} ctx;

static int smcf_client_init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *data)
{
    int status = fwk_module_get_element_count(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF), &(ctx.mgi_count));
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(MOD_NAME "Can not get SMCF element count");
        return status;
    }

    /* Check if element count is 0 and return if so */
    if (element_count == 0) {
        ctx.mgi_conf = NULL;
        return FWK_SUCCESS;
    }

    /* Ensure that element count matches with SMCF element count */
    if (ctx.mgi_count != element_count) {
        FWK_LOG_ERR(MOD_NAME "element_count does not match SMCF element_count");
        return FWK_E_PARAM;
    }

    /* Allocate memory for array containing MGI configuration pointers */
    ctx.mgi_conf =
        fwk_mm_calloc(element_count, sizeof(struct mod_smcf_client_mgi_conf *));

    return FWK_SUCCESS;
}

static int smcf_client_element_init(
    fwk_id_t element_id,
    unsigned int sub_element_count,
    const void *data)
{
    unsigned int element_idx = fwk_id_get_element_idx(element_id);
    const struct mod_smcf_client_mgi_conf *config = data;

    fwk_assert(data);
    fwk_assert(element_idx < ctx.mgi_count);

    /* Store element config and sub_element_config */
    ctx.mgi_conf[element_idx] = config;

    return FWK_SUCCESS;
}

static int start_sampling_all_mgis(void)
{
    int status;
    unsigned int mgi_idx;

    for (mgi_idx = 0; mgi_idx < (unsigned long)ctx.mgi_count; mgi_idx++) {
        FWK_LOG_INFO(MOD_NAME "start data_sampling for MGI[%u]", mgi_idx);
        status = ctx.platform_sampling_api->start_data_sampling(
            FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, mgi_idx));
        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR(
                MOD_NAME "FAILED to start data_sampling for MGI[%u]", mgi_idx);
            return status;
        }
    }

    return FWK_SUCCESS;
}

static int stop_sampling_all_mgis(void)
{
    int status;
    unsigned int mgi_idx;

    for (mgi_idx = 0; mgi_idx < (unsigned long)ctx.mgi_count; mgi_idx++) {
        FWK_LOG_INFO(MOD_NAME "Stop data_sampling for MGI[%u]", mgi_idx);
        status = ctx.platform_sampling_api->stop_data_sampling(
            FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, mgi_idx));
        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR(
                MOD_NAME "FAILED to stop data_sampling for MGI[%u]", mgi_idx);
            return status;
        }
    }

    return FWK_SUCCESS;
}

static int toggle_print(void)
{
    return (print_is_on_flag = !print_is_on_flag);
}

const struct mod_smcf_client_control_api smcf_client_ctrl_api = {
    .start_sampling_all_mgis = start_sampling_all_mgis,
    .stop_sampling_all_mgis = stop_sampling_all_mgis,
    .toggle_print = toggle_print,
};

static int smcf_client_bind(fwk_id_t id, unsigned int round)
{
    int status;

    /* Only bind in the first round of calls */
    if (round > 0) {
        return FWK_SUCCESS;
    }

    /* Bind to Platform SMCF sampling API */
    status = fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_PLATFORM_SMCF),
        FWK_ID_API(
            FWK_MODULE_IDX_PLATFORM_SMCF,
            MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API),
        &ctx.platform_sampling_api);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(MOD_NAME "Bind to Platform SMCF sampling API FAILED");
        return status;
    }

    /* Bind to SMCF control API */
    status = fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_SMCF),
        FWK_ID_API(FWK_MODULE_IDX_SMCF, MOD_SMCF_API_IDX_CONTROL),
        &ctx.control_api);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(MOD_NAME "Bind to SMCF control API FAILED");
        return status;
    }

    /* Bind to AMU SMCF Drv data API */
    status = fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_AMU_SMCF_DRV),
        FWK_ID_API(FWK_MODULE_IDX_AMU_SMCF_DRV, MOD_AMU_SMCF_DRV_API_IDX_DATA),
        &ctx.amu_data_api);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(MOD_NAME "Bind to AMU SMCF Drv data API FAILED");
        return status;
    }

    /* Bind to Sensor SMCF Drv get multiple smaples API */
    status = fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_SENSOR_SMCF_DRV),
        FWK_ID_API(
            FWK_MODULE_IDX_SENSOR_SMCF_DRV,
            MOD_SENSOR_SMCF_DRV_API_IDX_GET_MULTIPLE_SAMPLES),
        &ctx.sensor_data_api);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(MOD_NAME "Bind to Sensor Sensor API FAILED");
        return status;
    }

    return FWK_SUCCESS;
}

enum SMCF_CLIENT_MSRMT_DATA_TYPE {
    /* Data is stored in 64-bit unsigned int values */
    SMCF_CLIENT_MSRMT_DATA_TYPE_UI64,
    /* Data is stored in 32-bit unsigned int values */
    SMCF_CLIENT_MSRMT_DATA_TYPE_UI32
};

struct smcf_client_msrmt_data {
    /* Type of data union */
    enum SMCF_CLIENT_MSRMT_DATA_TYPE type;
    /* Pointer to data */
    union {
        const uint32_t *ui32;
        const uint64_t *ui64;
    } data;
    /* Number of 32-bit data samples */
    unsigned int num_data;
};

static void smcf_client_print_msrmt_data(
    const struct smcf_client_msrmt_data *msrmt)
{
    unsigned int val_idx;
    uint32_t val;
    uint64_t v;

    fwk_assert(msrmt);

    switch (msrmt->type) {
    case SMCF_CLIENT_MSRMT_DATA_TYPE_UI32:
        fwk_assert(msrmt->data.ui32);
        for (val_idx = 0; val_idx < msrmt->num_data; val_idx++) {
            fwk_log_printf(
                MOD_NAME "Value[%u] data = 0x%x",
                val_idx,
                msrmt->data.ui32[val_idx]);
        }
        break;
    case SMCF_CLIENT_MSRMT_DATA_TYPE_UI64:
        fwk_assert(msrmt->data.ui64);
        for (val_idx = 0; val_idx < msrmt->num_data; val_idx++) {
            v = msrmt->data.ui64[val_idx / 2];
            val = (val_idx & 1u) ? (uint32_t)(v >> 32) :
                                   (uint32_t)(v & UINT64_C(0xFFFFFFFF));
            fwk_log_printf(MOD_NAME "Value[%u] data = 0x%x", val_idx, val);
        }
        break;
    default:
        FWK_LOG_WARN(MOD_NAME "Unrecognized data type.");
        break;
    }
}

#ifdef BUILD_HAS_NOTIFICATION
static int smcf_client_process_new_sample(const struct fwk_event *event)
{
    int status = FWK_SUCCESS;
    unsigned int mgi_idx, mli_idx;
    size_t mli_count;
    uint64_t counter_values[NUM_OF_AMU_COUNTERS];
    uint32_t sensor_values[NUM_OF_SENSOR_VALUES];
    struct smcf_client_msrmt_data msrmt_data;
    const char *mgi_name = fwk_module_get_element_name(event->source_id);
    struct mod_smcf_client_mli_conf *mli_cfg = NULL;

    mgi_idx = fwk_id_get_element_idx(event->source_id);

    status = fwk_module_get_sub_element_count(event->source_id, &mli_count);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(MOD_NAME "Can not get sub element count.");
        return status;
    }

    if (mgi_idx >= (unsigned long)ctx.mgi_count) {
        FWK_LOG_ERR(MOD_NAME "mgi_idx out of range");
        return FWK_E_RANGE;
    }

    for (mli_idx = 0; mli_idx < mli_count; mli_idx++) {
        mli_cfg = &(ctx.mgi_conf[mgi_idx]->mlis[mli_idx]);
        switch (mli_cfg->type) {
        case MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_AMU:
            /* AMU */
            status = ctx.amu_data_api->get_counters(
                FWK_ID_SUB_ELEMENT(FWK_MODULE_IDX_SMCF, mgi_idx, mli_idx),
                counter_values,
                NUM_OF_AMU_COUNTERS);
            if (status != FWK_SUCCESS) {
                FWK_LOG_ERR(
                    MOD_NAME
                    "Failed to get data for MGI %u MLI %u (AMU) ERROR %d",
                    mgi_idx,
                    mli_idx,
                    status);
                return status;
            }
            if (print_is_on_flag) {
                msrmt_data.type = SMCF_CLIENT_MSRMT_DATA_TYPE_UI64;
                msrmt_data.data.ui64 = counter_values;
                msrmt_data.num_data = NUM_OF_AMU_COUNTERS;
                fwk_log_printf(
                    MOD_NAME "Values for MGI %s MLI %u (AMU)",
                    mgi_name,
                    mli_idx);
                smcf_client_print_msrmt_data(&msrmt_data);
            }
            break;
        case MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_SENSOR:
            /* Sensor */
            status = ctx.sensor_data_api->get_samples(
                FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, mgi_idx),
                sensor_values,
                NUM_OF_SENSOR_VALUES * sizeof(uint32_t));
            if (status != FWK_SUCCESS) {
                FWK_LOG_ERR(
                    MOD_NAME
                    "Failed to get data for MGI %u MLI %u (Sensor) ERROR %d",
                    mgi_idx,
                    mli_idx,
                    status);
                return status;
            }
            if (print_is_on_flag) {
                msrmt_data.type = SMCF_CLIENT_MSRMT_DATA_TYPE_UI32;
                msrmt_data.data.ui32 = sensor_values;
                msrmt_data.num_data = NUM_OF_SENSOR_VALUES;
                fwk_log_printf(
                    MOD_NAME "Values for MGI %s MLI %u (Sensor)",
                    mgi_name,
                    mli_idx);
                smcf_client_print_msrmt_data(&msrmt_data);
            }
            break;
        default:
            FWK_LOG_ERR(MOD_NAME "MLI type not supported");
            return FWK_E_SUPPORT;
        }
    }
    return FWK_SUCCESS;
}

static int smcf_client_process_pd_transition(const struct fwk_event *event)
{
    int status = FWK_SUCCESS;
    struct mod_pd_power_state_transition_notification_params *params =
        (struct mod_pd_power_state_transition_notification_params *)
            event->params;

    switch (params->state) {
    case (unsigned int)MOD_PD_STATE_OFF:
    case (unsigned int)MOD_PD_STATE_OFF_0:
    case (unsigned int)MOD_PD_STATE_OFF_1:
    case (unsigned int)MOD_PD_STATE_OFF_2:
    case (unsigned int)MOD_PD_STATE_SLEEP:
        /* Stop sampling all MGIs */
        status = stop_sampling_all_mgis();
        break;
    case (unsigned int)MOD_PD_STATE_ON:
        /* Start sampling all MGIs */
        status = start_sampling_all_mgis();
        break;
    default:
        /* Unsupported Power Domain notification, do nothing */
        FWK_LOG_ERR(MOD_NAME "Power Domain notification not supported");
        status = FWK_E_SUPPORT;
    }
    return status;
}

static int smcf_client_process_notification(
    const struct fwk_event *event,
    struct fwk_event *resp_event)
{
    int status = FWK_SUCCESS;
    if (fwk_id_is_equal(
            event->id, mod_smcf_notification_id_new_data_sample_ready)) {
        /* New data sample */
        status = smcf_client_process_new_sample(event);
    } else if (fwk_id_is_equal(event->id, pd_transition_notification_id)) {
        /* PD transition */
        status = smcf_client_process_pd_transition(event);
    } else {
        /* Unknown notification */
        FWK_LOG_ERR(MOD_NAME "Notification not supported");
        status = FWK_E_SUPPORT;
    }
    return status;
}
#endif /* BUILD_HAS_NOTIFICATION */

static int smcf_client_process_bind_request(
    fwk_id_t requester_id,
    fwk_id_t pd_id,
    fwk_id_t api_id,
    const void **api)
{
    enum mod_smcf_client_api_idx api_id_type;

    api_id_type = (enum mod_smcf_client_api_idx)fwk_id_get_api_idx(api_id);

    switch (api_id_type) {
    case MOD_SMCF_CLIENT_API_IDX_CONTROL:
        *api = &smcf_client_ctrl_api;
        break;
    default:
        return FWK_E_PARAM;
    }

    return FWK_SUCCESS;
}

static int smcf_client_start(fwk_id_t id)
{
    unsigned mgi_idx;
    fwk_id_t mgi;
    int status = FWK_SUCCESS;
    fwk_id_t pd_transition_source_id;
    size_t element_count;

    if (fwk_id_is_type(id, FWK_ID_TYPE_ELEMENT)) {
        /* Elements have already been started with module start */
        return FWK_SUCCESS;
    }

    status = fwk_module_get_element_count(id, &element_count);
    if (status != FWK_SUCCESS) {
        return status;
    }
    if (element_count == 0) {
        /* No elements assigned, we do not need to continue */
        return FWK_SUCCESS;
    }

#ifdef BUILD_HAS_NOTIFICATION
    /* Subscribe to PD transition notification */
    pd_transition_source_id = fwk_id_build_element_id(
        fwk_module_id_power_domain, PD_STATIC_DEV_IDX_SYSTOP);
    status = fwk_notification_subscribe(
        pd_transition_notification_id, pd_transition_source_id, id);
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR(
            MOD_NAME "Failed to subscribe to Power Domain notification for %s",
            fwk_module_get_element_name(id));
        return status;
    }

    /* Subscribe to new-sample-ready notification for each MGI */
    for (mgi_idx = 0; mgi_idx < (unsigned long)ctx.mgi_count; mgi_idx++) {
        mgi = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, mgi_idx);
        status = fwk_notification_subscribe(
            mod_smcf_notification_id_new_data_sample_ready, mgi, id);
        if (status != FWK_SUCCESS) {
            FWK_LOG_CRIT(MOD_NAME "Failed to subscribe platform notification");
            return status;
        }
    }
#endif /* BUILD_HAS_NOTIFICATION */
    return FWK_SUCCESS;
}

const struct fwk_module module_smcf_client = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .api_count = MOD_SMCF_CLIENT_API_COUNT,
    .init = smcf_client_init,
    .element_init = smcf_client_element_init,
    .bind = smcf_client_bind,
    .process_bind_request = smcf_client_process_bind_request,
    .start = smcf_client_start,
#ifdef BUILD_HAS_NOTIFICATION
    .process_notification = smcf_client_process_notification,
#endif /* BUILD_HAS_NOTIFICATION */
};
