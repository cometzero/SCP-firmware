/*
 * Arm SCP/MCP Software
 * Copyright (c) 2019-2024, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <dw_apb_i2c.h>

#include <mod_dw_apb_i2c.h>
#include <mod_i2c.h>
#include <mod_timer.h>

#include <fwk_id.h>
#include <fwk_interrupt.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_status.h>

#include <stdbool.h>
#include <stddef.h>

struct dw_apb_i2c_ctx {
    const struct mod_dw_apb_i2c_dev_config *config;
    const struct mod_i2c_driver_response_api *i2c_api;
    const struct mod_timer_api *timer_api;
    fwk_id_t i2c_id;
    struct dw_apb_i2c_reg *i2c_reg;
    bool read_on_going;
    uint8_t byte_count;
    uint8_t *data;
    bool failed;
    bool ready;
};

static struct dw_apb_i2c_ctx *ctx_table;

/*
 * Static helpers
 */
static bool is_i2c_disabled(void *param)
{
    struct dw_apb_i2c_reg *i2c_reg = (struct dw_apb_i2c_reg *)param;

    return ((i2c_reg->IC_ENABLE_STATUS & IC_ENABLE_STATUS_MASK) ==
        IC_ENABLE_STATUS_DISABLED);
}

static int disable_i2c(struct dw_apb_i2c_ctx *ctx)
{
    int status;
    fwk_id_t timer_id;
    const struct mod_timer_api *timer_api;
    struct dw_apb_i2c_reg *i2c_reg;

    timer_api = ctx->timer_api;
    timer_id = ctx->config->timer_id;
    i2c_reg = ctx->i2c_reg;

    /* Check whether the device is already disabled */
    if (is_i2c_disabled(i2c_reg)) {
        return FWK_SUCCESS;
    }

    /* The bus should be idle */
    if ((ctx->i2c_reg->IC_STATUS & IC_STATUS_MST_ACTIVITY_MASK) != 0) {
        return FWK_E_DEVICE;
    }

    /* Disable the I2C device */
    ctx->i2c_reg->IC_ENABLE = IC_ENABLE_STATUS_DISABLED;

    /* Wait until the device is disabled */
    status = timer_api->wait(timer_id, I2C_TIMEOUT_US, is_i2c_disabled,
        i2c_reg);
    if (status != FWK_SUCCESS) {
        return FWK_E_TIMEOUT;
    }

    return FWK_SUCCESS;
}

static int enable_i2c(struct dw_apb_i2c_ctx *ctx, uint8_t target_address)
{
    int status;
    struct dw_apb_i2c_reg *i2c_reg;

    i2c_reg = ctx->i2c_reg;

    /* Disable the I2C device to configure it */
    status = disable_i2c(ctx);
    if (status != FWK_SUCCESS) {
        return FWK_E_DEVICE;
    }

    /* Program the target address */
    i2c_reg->IC_TAR = (target_address & IC_TAR_ADDRESS);

    /* Enable STOP detected interrupt and TX aborted interrupt */
    i2c_reg->IC_INTR_MASK = (IC_INTR_STOP_DET_MASK | IC_INTR_TX_ABRT_MASK);

    /* Enable the I2C device */
    i2c_reg->IC_ENABLE = IC_ENABLE_STATUS_ENABLED;

    return FWK_SUCCESS;
}

/*
 * An IRQ is triggered if the transaction has been completed successfully or
 * if the transaction has been aborted.
 */
static void i2c_isr(uintptr_t data)
{
    unsigned int i;
    int i2c_status = FWK_E_DEVICE;
    struct dw_apb_i2c_reg *i2c_reg;
    struct dw_apb_i2c_ctx *ctx = (struct dw_apb_i2c_ctx *)data;

    i2c_reg = ctx->i2c_reg;

    /* The transaction has completed successfully */
    if (i2c_reg->IC_INTR_STAT & IC_INTR_STOP_DET_MASK) {
        (void)i2c_reg->IC_CLR_STOP_DET;
        i2c_status = FWK_SUCCESS;
        if (ctx->read_on_going) {
            ctx->read_on_going = false;
            /* Read the data from the device buffer */
            for (i = 0; i < ctx->byte_count; i++) {
                ctx->data[i] =
                    (uint8_t)(i2c_reg->IC_DATA_CMD & IC_DATA_CMD_DATA_MASK);
            }
        }
    }

    /* The transaction has been aborted */
    if (i2c_reg->IC_INTR_STAT & IC_INTR_TX_ABRT_MASK) {
        (void)i2c_reg->IC_CLR_TX_ABRT;
    }

    ctx->i2c_api->transaction_completed(ctx->i2c_id, i2c_status);
}

/*
 * Driver API
 */
static int transmit_as_controller(
    fwk_id_t dev_id,
    struct mod_i2c_request *transmit_request)
{
    int status;
    unsigned int sent_bytes, flags;
    struct dw_apb_i2c_ctx *ctx;

    if (transmit_request->transmit_byte_count > I2C_TRANSMIT_BUFFER_LENGTH) {
        return FWK_E_SUPPORT;
    }

    if (transmit_request->target_address == 0) {
        return FWK_E_PARAM;
    }

    ctx = ctx_table + fwk_id_get_element_idx(dev_id);

    status = enable_i2c(ctx, transmit_request->target_address);
    if (status != FWK_SUCCESS) {
        return FWK_E_DEVICE;
    }

    /* The program of the I2C controller cannot be interrupted. */
    flags = fwk_interrupt_global_disable();

    for (sent_bytes = 0; sent_bytes < transmit_request->transmit_byte_count;
         sent_bytes++) {
        ctx->i2c_reg->IC_DATA_CMD = transmit_request->transmit_data[sent_bytes];
    }

    fwk_interrupt_global_enable(flags);

    /*
     * The data has been pushed to the I2C FIFO for transmission to the
     * target device. An interrupt will signal the completion of the
     * transfer. The i2c_isr() interrupt handler will be invoked to notify
     * the caller.
     */
    return FWK_PENDING;
}

static int receive_as_controller(
    fwk_id_t dev_id,
    struct mod_i2c_request *receive_request)
{
    int status;
    unsigned int i, flags;
    struct dw_apb_i2c_ctx *ctx;

    if (receive_request->receive_byte_count > I2C_RECEIVE_BUFFER_LENGTH) {
        return FWK_E_SUPPORT;
    }

    if (receive_request->target_address == 0) {
        return FWK_E_PARAM;
    }

    ctx = ctx_table + fwk_id_get_element_idx(dev_id);

    ctx->byte_count = receive_request->receive_byte_count;
    ctx->data = receive_request->receive_data;
    ctx->read_on_going = true;

    status = enable_i2c(ctx, receive_request->target_address);
    if (status != FWK_SUCCESS) {
        return FWK_E_DEVICE;
    }

    /* The program of the I2C controller cannot be interrupted. */
    flags = fwk_interrupt_global_disable();

    /* Program the I2C controller with the expected reply length in bytes. */
    for (i = 0; i < receive_request->receive_byte_count; i++) {
        ctx->i2c_reg->IC_DATA_CMD = IC_DATA_CMD_READ;
    }

    fwk_interrupt_global_enable(flags);

    /*
     * The command has been sent to the I2C for requesting data from
     * the target device. An interrupt will signal the completion of the
     * transfer. The i2c_isr() interrupt handler will be invoked to notify
     * the caller.
     */
    return FWK_PENDING;
}

struct polled_transfer {
    struct dw_apb_i2c_ctx *ctx;
    const struct mod_i2c_request *request;
    unsigned int sent;
    unsigned int received;
    int status;
    bool completion_only;
};

static int disable_polled(struct dw_apb_i2c_ctx *ctx)
{
    int status;

    ctx->i2c_reg->IC_ENABLE = IC_ENABLE_STATUS_DISABLED;
    status = ctx->timer_api->wait(
        ctx->config->timer_id, ctx->config->transfer_timeout_us,
        is_i2c_disabled, ctx->i2c_reg);
    if (status != FWK_SUCCESS)
        ctx->failed = true;
    return status;
}

static void drain_polled_rx(struct polled_transfer *transfer)
{
    struct dw_apb_i2c_reg *reg = transfer->ctx->i2c_reg;

    while (transfer->received < transfer->request->receive_byte_count &&
           (reg->IC_STATUS & IC_STATUS_RFNE_MASK))
        transfer->request->receive_data[transfer->received++] =
            (uint8_t)reg->IC_DATA_CMD;
}

static bool poll_transfer(void *data)
{
    struct polled_transfer *transfer = data;
    const struct mod_i2c_request *request = transfer->request;
    struct dw_apb_i2c_reg *reg = transfer->ctx->i2c_reg;
    unsigned int total =
        request->transmit_byte_count + request->receive_byte_count;
    uint32_t command, irq;

    irq = reg->IC_RAW_INTR_STAT;
    if (irq & IC_INTR_TX_ABRT_MASK) {
        transfer->status = FWK_E_DEVICE;
        return true;
    }
    drain_polled_rx(transfer);
    irq = reg->IC_RAW_INTR_STAT;
    if (irq & IC_INTR_TX_ABRT_MASK) {
        transfer->status = FWK_E_DEVICE;
        return true;
    }
    if (irq & IC_INTR_STOP_DET_MASK) {
        /* STOP can arrive after the preceding RX-not-empty sample. */
        drain_polled_rx(transfer);
        transfer->status =
            transfer->sent == total &&
                transfer->received == request->receive_byte_count ?
            FWK_SUCCESS : FWK_E_DEVICE;
        return true;
    }
    if (transfer->completion_only || transfer->sent >= total ||
        !(reg->IC_STATUS & IC_STATUS_TFNF_MASK))
        return false;

    if (transfer->sent < request->transmit_byte_count) {
        command = request->transmit_data[transfer->sent];
    } else {
        /* Limit outstanding reads to the RX FIFO capacity. */
        if (transfer->sent - request->transmit_byte_count - transfer->received >=
            I2C_RECEIVE_BUFFER_LENGTH)
            return false;
        command = IC_DATA_CMD_READ;
        if (request->transmit_byte_count &&
            transfer->sent == request->transmit_byte_count)
            command |= IC_DATA_CMD_RESTART;
    }
    if (transfer->sent + 1 == total)
        command |= IC_DATA_CMD_STOP;
    reg->IC_DATA_CMD = command;
    transfer->sent++;
    return false;
}

static int transfer_as_controller(
    fwk_id_t dev_id, struct mod_i2c_request *request)
{
    struct dw_apb_i2c_ctx *ctx;
    struct polled_transfer transfer;
    uint64_t start, ticks, now;
    int status, disable_status;

    if (!fwk_module_is_valid_element_id(dev_id) ||
        fwk_id_get_module_idx(dev_id) != FWK_MODULE_IDX_DW_APB_I2C || !request ||
        !request->target_address || request->target_address >= 0x80 ||
        (!request->transmit_byte_count && !request->receive_byte_count) ||
        (request->transmit_byte_count && !request->transmit_data) ||
        (request->receive_byte_count && !request->receive_data))
        return FWK_E_PARAM;
    ctx = ctx_table + fwk_id_get_element_idx(dev_id);
    if (!ctx->ready || ctx->failed)
        return FWK_E_STATE;
    status = disable_polled(ctx);
    if (status != FWK_SUCCESS)
        return status;
    (void)ctx->i2c_reg->IC_CLR_INTR;
    ctx->i2c_reg->IC_TAR = request->target_address;
    status = ctx->timer_api->time_to_timestamp(
        ctx->config->timer_id, ctx->config->transfer_timeout_us, &ticks);
    if (status == FWK_SUCCESS)
        status = ctx->timer_api->get_counter(ctx->config->timer_id, &start);
    if (status != FWK_SUCCESS) {
        ctx->failed = true;
        return status;
    }
    ctx->i2c_reg->IC_ENABLE = IC_ENABLE_STATUS_ENABLED;
    transfer = (struct polled_transfer) {
        .ctx = ctx,
        .request = request,
        .status = FWK_E_DEVICE,
    };
    for (;;) {
        if (poll_transfer(&transfer)) {
            status = transfer.status;
            break;
        }
        status = ctx->timer_api->get_counter(ctx->config->timer_id, &now);
        if (status != FWK_SUCCESS) {
            ctx->failed = true;
            break;
        }
        if (now - start >= ticks) {
            /* Completion may arrive during the counter sample. */
            transfer.completion_only = true;
            if (poll_transfer(&transfer)) {
                status = transfer.status;
            } else {
                status = FWK_E_TIMEOUT;
                /* A command may survive disable: never retry a timeout. */
                ctx->failed = true;
            }
            break;
        }
    }
    disable_status = disable_polled(ctx);
    (void)ctx->i2c_reg->IC_CLR_INTR;
    return status == FWK_SUCCESS ? disable_status : status;
}

static const struct mod_i2c_driver_api polled_driver_api = {
    .transfer_as_controller = transfer_as_controller,
};

static const struct mod_i2c_driver_api driver_api = {
    .transmit_as_controller = transmit_as_controller,
    .receive_as_controller = receive_as_controller
};

/*
 * Framework handlers
 */
static int dw_apb_i2c_init(fwk_id_t module_id,
                               unsigned int element_count,
                               const void *data)
{
    ctx_table = fwk_mm_calloc(element_count, sizeof(*ctx_table));

    return FWK_SUCCESS;
}

static int dw_apb_i2c_element_init(fwk_id_t element_id,
                                       unsigned int sub_element_count,
                                       const void *data)
{
    struct mod_dw_apb_i2c_dev_config *config =
        (struct mod_dw_apb_i2c_dev_config *)data;

    if (config->reg == 0 || (config->polled && !config->transfer_timeout_us)) {
        return FWK_E_DATA;
    }

    ctx_table[fwk_id_get_element_idx(element_id)].config = config;
    ctx_table[fwk_id_get_element_idx(element_id)].i2c_reg =
        (struct dw_apb_i2c_reg *)config->reg;

    return FWK_SUCCESS;
}

static int dw_apb_i2c_bind(fwk_id_t id, unsigned int round)
{
    int status;
    struct dw_apb_i2c_ctx *ctx;
    const struct mod_dw_apb_i2c_dev_config *config;

    if (!fwk_module_is_valid_element_id(id) || (round == 0)) {
        return FWK_SUCCESS;
    }

    ctx = ctx_table + fwk_id_get_element_idx(id);
    config = ctx->config;

    status = fwk_module_bind(config->timer_id, MOD_TIMER_API_ID_TIMER,
        &ctx->timer_api);
    if (status != FWK_SUCCESS) {
        return status;
    }

    if (config->polled)
        return FWK_SUCCESS;

    return fwk_module_bind(ctx->i2c_id, mod_i2c_api_id_driver_response,
        &ctx->i2c_api);
}

static int dw_apb_i2c_process_bind_request(fwk_id_t source_id,
                                           fwk_id_t target_id,
                                           fwk_id_t api_id,
                                           const void **api)
{
    struct dw_apb_i2c_ctx *ctx;

    if (!fwk_module_is_valid_element_id(target_id)) {
        return FWK_E_PARAM;
    }

    ctx = ctx_table + fwk_id_get_element_idx(target_id);

    if (!fwk_id_is_equal(api_id, mod_dw_apb_i2c_api_id_driver)) {
        return FWK_E_PARAM;
    }

    ctx->i2c_id = source_id;

    *api = ctx->config->polled ? &polled_driver_api : &driver_api;

    return FWK_SUCCESS;
}

static int dw_apb_i2c_start(fwk_id_t id)
{
    int status;
    struct dw_apb_i2c_ctx *ctx;
    unsigned int i2c_irq;

    /* Nothing to do for the module */
    if (!fwk_module_is_valid_element_id(id)) {
        return FWK_SUCCESS;
    }

    ctx = ctx_table + fwk_id_get_element_idx(id);
    if (ctx->config->polled) {
        if (ctx->i2c_reg->IC_COMP_TYPE != UINT32_C(0x44570140))
            return FWK_E_DEVICE;
        ctx->i2c_reg->IC_INTR_MASK = 0;
        status = disable_polled(ctx);
        if (status != FWK_SUCCESS)
            return status;
        /* Master, fast speed, repeated START, slave disabled. */
        ctx->i2c_reg->IC_CON = UINT32_C(0x65);
        (void)ctx->i2c_reg->IC_CLR_INTR;
        ctx->ready = true;
        return FWK_SUCCESS;
    }
    i2c_irq = ctx->config->i2c_irq;

    status = fwk_interrupt_set_isr_param(i2c_irq, i2c_isr, (uintptr_t)ctx);
    if (status != FWK_SUCCESS) {
        return FWK_E_DEVICE;
    }

    status = fwk_interrupt_clear_pending(i2c_irq);
    if (status != FWK_SUCCESS) {
        return FWK_E_DEVICE;
    }

    status = fwk_interrupt_enable(i2c_irq);
    if (status != FWK_SUCCESS) {
        return FWK_E_DEVICE;
    }

    return FWK_SUCCESS;
}

const struct fwk_module module_dw_apb_i2c = {
    .api_count = (unsigned int)MOD_DW_APB_I2C_API_IDX_COUNT,
    .type = FWK_MODULE_TYPE_DRIVER,
    .init = dw_apb_i2c_init,
    .element_init = dw_apb_i2c_element_init,
    .bind = dw_apb_i2c_bind,
    .start = dw_apb_i2c_start,
    .process_bind_request = dw_apb_i2c_process_bind_request,
};
