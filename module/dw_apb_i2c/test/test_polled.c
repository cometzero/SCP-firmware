/* SPDX-License-Identifier: BSD-3-Clause */
/* Native register/timer harness; unused framework entry points are discarded. */
#include "../src/mod_dw_apb_i2c.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t registers[64];
static uint32_t commands[512];
static unsigned int command_count, wait_calls;
static int transfer_error, disable_error;
static bool abort_transfer;
static uint64_t counter;

bool fwk_module_is_valid_element_id(fwk_id_t id)
{
    return id.value == FWK_ID_ELEMENT(FWK_MODULE_IDX_DW_APB_I2C, 0).value;
}

unsigned int fwk_id_get_element_idx(fwk_id_t id)
{
    (void)id;
    return 0;
}

unsigned int fwk_id_get_module_idx(fwk_id_t id)
{
    return id.common.module_idx;
}

static int wait_mock(
    fwk_id_t id, uint32_t timeout, bool (*cond)(void *), void *data)
{
    assert(id.value == FWK_ID_ELEMENT(FWK_MODULE_IDX_TIMER, 0).value);
    assert(timeout == 1000);
    wait_calls++;
    assert(cond == is_i2c_disabled);
    return disable_error ? disable_error :
                           (cond(data) ? FWK_SUCCESS : FWK_E_TIMEOUT);
}

static int timestamp_mock(fwk_id_t id, uint32_t us, uint64_t *ticks)
{
    (void)id;
    *ticks = (uint64_t)us * 10;
    return FWK_SUCCESS;
}

static int counter_mock(fwk_id_t id, uint64_t *value)
{
    (void)id;
    if (!registers[0x6c / 4]) {
        *value = counter = 0;
        return FWK_SUCCESS;
    }
    if (transfer_error == FWK_E_TIMEOUT) {
        *value = 10000;
        return FWK_SUCCESS;
    }
    if (transfer_error)
        return transfer_error;
    commands[command_count++] = registers[0x10 / 4];
    if (registers[0x10 / 4] & IC_DATA_CMD_READ) {
        registers[0x10 / 4] = 0xa5;
        registers[0x70 / 4] |= IC_STATUS_RFNE_MASK;
    } else {
        registers[0x70 / 4] &= ~IC_STATUS_RFNE_MASK;
    }
    if (commands[command_count - 1] & IC_DATA_CMD_STOP)
        registers[0x34 / 4] = IC_INTR_STOP_DET_MASK;
    if (abort_transfer)
        registers[0x34 / 4] = IC_INTR_TX_ABRT_MASK;
    counter += 5000;
    *value = counter;
    return FWK_SUCCESS;
}

int main(void)
{
    const fwk_id_t id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_DW_APB_I2C, 0);
    const struct mod_timer_api timer = {
        .wait = wait_mock,
        .time_to_timestamp = timestamp_mock,
        .get_counter = counter_mock,
    };
    const struct mod_dw_apb_i2c_dev_config config = {
        .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
        .polled = true,
        .transfer_timeout_us = 1000,
    };
    struct dw_apb_i2c_ctx ctx = {
        .config = &config,
        .timer_api = &timer,
        .i2c_reg = (struct dw_apb_i2c_reg *)registers,
        .ready = true,
    };
    uint8_t tx[] = { 0x10, 0x42 };
    uint8_t rx;
    struct mod_i2c_request request = {
        .target_address = 0x48,
        .transmit_data = tx,
        .transmit_byte_count = 2,
    };

    ctx_table = &ctx;
    registers[0x70 / 4] = IC_STATUS_TFNF_MASK;
    assert(transfer_as_controller(id, &request) == FWK_SUCCESS);
    assert(command_count == 2 && commands[0] == 0x10);
    assert(commands[1] == (0x42 | IC_DATA_CMD_STOP));
    assert(registers[0x6c / 4] == 0);

    registers[0x34 / 4] = 0;
    command_count = 0;
    request.transmit_byte_count = 1;
    request.receive_byte_count = 1;
    request.receive_data = &rx;
    assert(transfer_as_controller(id, &request) == FWK_SUCCESS);
    assert(rx == 0xa5 && command_count == 2);
    assert(commands[1] ==
        (IC_DATA_CMD_READ | IC_DATA_CMD_RESTART | IC_DATA_CMD_STOP));

    registers[0x34 / 4] = 0;
    registers[0x70 / 4] = IC_STATUS_TFNF_MASK;
    abort_transfer = true;
    assert(transfer_as_controller(id, &request) == FWK_E_DEVICE);
    assert(!ctx.failed && registers[0x6c / 4] == 0);
    abort_transfer = false;

    registers[0x34 / 4] = 0;
    transfer_error = FWK_E_TIMEOUT;
    assert(transfer_as_controller(id, &request) == FWK_E_TIMEOUT);
    assert(ctx.failed && registers[0x6c / 4] == 0);
    wait_calls = 0;
    assert(transfer_as_controller(id, &request) == FWK_E_STATE);
    assert(wait_calls == 0);

    ctx.failed = false;
    transfer_error = FWK_E_DEVICE;
    assert(transfer_as_controller(id, &request) == FWK_E_DEVICE);
    assert(ctx.failed && registers[0x6c / 4] == 0);

    ctx.failed = false;
    transfer_error = 0;
    disable_error = FWK_E_SUPPORT;
    assert(transfer_as_controller(id, &request) == FWK_E_SUPPORT);
    assert(ctx.failed);

    assert(transfer_as_controller(id, NULL) == FWK_E_PARAM);
    puts("DW APB I2C polled transfer: PASS");
    return 0;
}
