/* SPDX-License-Identifier: BSD-3-Clause */
#include "../src/mod_i2c.c"

#include <assert.h>
#include <stdio.h>

static struct fwk_event queued[16], delayed[16], responses[16];
static unsigned int queued_count, delayed_count, response_count, allocations;
static bool fail_alloc, fail_put;
static int tx_status = FWK_PENDING, rx_status = FWK_PENDING;
static unsigned int tx_calls, rx_calls;
static uint8_t *expected_tx, *expected_rx;

void *fwk_mm_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

int fwk_module_bind(fwk_id_t target, fwk_id_t api, const void *out)
{
    return FWK_E_SUPPORT;
}

void *fwk_mm_alloc_notrap(size_t count, size_t size)
{
    void *ptr;

    if (fail_alloc)
        return NULL;
    ptr = calloc(count, size);
    assert(ptr != NULL);
    allocations++;
    return ptr;
}

void fwk_mm_free(void *ptr)
{
    assert(ptr != NULL && allocations > 0);
    allocations--;
    free(ptr);
}

bool fwk_module_is_valid_element_id(fwk_id_t id)
{
    return id.common.type == FWK_ID_TYPE_ELEMENT;
}

int __fwk_put_event(struct fwk_event *event)
{
    if (fail_put)
        return FWK_E_NOMEM;
    if (event->is_response) {
        assert(delayed_count > 0);
        assert(event->cookie == delayed[0].cookie);
        responses[response_count++] = *event;
        memmove(delayed, delayed + 1,
            --delayed_count * sizeof(delayed[0]));
    } else {
        assert(queued_count < 16);
        queued[queued_count++] = *event;
    }
    return FWK_SUCCESS;
}

int fwk_get_first_delayed_response(fwk_id_t id, struct fwk_event *event)
{
    assert(delayed_count > 0);
    *event = delayed[0];
    return FWK_SUCCESS;
}

int fwk_is_delayed_response_list_empty(fwk_id_t id, bool *empty)
{
    *empty = delayed_count == 0;
    return FWK_SUCCESS;
}

static int driver_tx(fwk_id_t id, struct mod_i2c_request *request)
{
    assert(request->transmit_data == expected_tx);
    assert(request->transmit_byte_count == 1);
    tx_calls++;
    return tx_status;
}

static int driver_rx(fwk_id_t id, struct mod_i2c_request *request)
{
    assert(request->receive_data == expected_rx);
    assert(request->receive_byte_count == 1);
    rx_calls++;
    return rx_status;
}

static void dispatch(void)
{
    static uint32_t cookie;
    struct fwk_event event, response;

    assert(queued_count > 0);
    event = queued[0];
    memmove(queued, queued + 1, --queued_count * sizeof(queued[0]));
    response = event;
    response.cookie = ++cookie;
    response.is_response = true;
    assert(mod_i2c_process_event(&event, &response) == FWK_SUCCESS);
    if (event.response_requested) {
        if (response.is_delayed_response)
            delayed[delayed_count++] = response;
        else
            responses[response_count++] = response;
    }
}

static int result(unsigned int index)
{
    struct mod_i2c_event_param param;

    memcpy(&param, responses[index].params, sizeof(param));
    return param.status;
}

int main(void)
{
    const fwk_id_t id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_I2C, 0);
    const struct mod_i2c_dev_config config = { .driver_id = id };
    const struct mod_i2c_driver_api driver = {
        .transmit_as_controller = driver_tx,
        .receive_as_controller = driver_rx,
    };
    struct mod_i2c_dev_ctx ctx = {
        .config = &config, .driver_api = &driver,
    };
    uint8_t tx[2] = { 1, 2 }, rx[2] = { 0 };

    assert(FWK_EVENT_PARAMETERS_SIZE == 16);
    ctx_table = &ctx;
    expected_tx = tx;
    expected_rx = rx;
    assert(transmit_then_receive_as_controller(
        id, 0x48, tx, rx, 1, 1) == FWK_PENDING);
    assert(transmit_as_controller(id, 0x49, tx + 1, 1) == FWK_PENDING);
    assert(allocations == 2);
    dispatch();
    dispatch();
    assert(allocations == 1 && delayed_count == 2 && tx_calls == 1);
    transaction_completed(id, FWK_SUCCESS);
    dispatch();
    assert(rx_calls == 1 && delayed_count == 2);
    expected_tx = tx + 1;
    transaction_completed(id, FWK_SUCCESS);
    dispatch();
    assert(allocations == 0 && tx_calls == 2 && delayed_count == 1);
    assert(result(0) == FWK_SUCCESS);
    transaction_completed(id, FWK_E_TIMEOUT);
    dispatch();
    assert(result(1) == FWK_E_DEVICE && delayed_count == 0);
    assert(ctx.state == MOD_I2C_DEV_IDLE);

    /* Immediate driver failure and pure RX, including synchronous completion. */
    tx_status = FWK_E_DEVICE;
    assert(transmit_as_controller(id, 0x48, tx + 1, 1) == FWK_PENDING);
    dispatch();
    assert(result(2) == FWK_E_DEVICE && allocations == 0);
    rx_status = FWK_SUCCESS;
    assert(receive_as_controller(id, 0x48, rx, 1) == FWK_PENDING);
    dispatch();
    assert(result(3) == FWK_SUCCESS && allocations == 0);

    fail_alloc = true;
    assert(transmit_as_controller(id, 0x48, tx, 1) == FWK_E_NOMEM);
    fail_alloc = false;
    fail_put = true;
    assert(transmit_as_controller(id, 0x48, tx, 1) == FWK_E_NOMEM);
    fail_put = false;
    assert(allocations == 0 && queued_count == 0);

    ctx.state = MOD_I2C_DEV_PANIC;
    assert(transmit_as_controller(id, 0x48, tx, 1) == FWK_PENDING);
    dispatch();
    assert(result(4) == FWK_E_PANIC && allocations == 0);

    /* Queued immediate responses must reload the remaining request in order. */
    ctx.state = MOD_I2C_DEV_IDLE;
    tx_status = FWK_PENDING;
    expected_tx = tx;
    assert(transmit_as_controller(id, 0x48, tx, 1) == FWK_PENDING);
    assert(receive_as_controller(id, 0x48, rx, 1) == FWK_PENDING);
    assert(receive_as_controller(id, 0x48, rx, 1) == FWK_PENDING);
    dispatch();
    dispatch();
    dispatch();
    assert(allocations == 2 && delayed_count == 3);
    transaction_completed(id, FWK_SUCCESS);
    dispatch();
    assert(result(5) == FWK_SUCCESS && result(6) == FWK_SUCCESS);
    assert(allocations == 1 && queued_count == 1);
    dispatch();
    assert(result(7) == FWK_SUCCESS && allocations == 0);
    assert(delayed_count == 0 && ctx.state == MOD_I2C_DEV_IDLE);

    /* TX failure must complete TX/RX without starting the receive phase. */
    assert(transmit_then_receive_as_controller(
        id, 0x48, tx, rx, 1, 1) == FWK_PENDING);
    dispatch();
    {
        unsigned int previous_rx_calls = rx_calls;

        transaction_completed(id, FWK_E_TIMEOUT);
        dispatch();
        assert(rx_calls == previous_rx_calls);
    }
    assert(result(8) == FWK_E_DEVICE && allocations == 0);

    /* Framework duplicate_event failure can drop a busy delayed response. */
    assert(transmit_as_controller(id, 0x48, tx, 1) == FWK_PENDING);
    assert(receive_as_controller(id, 0x48, rx, 1) == FWK_PENDING);
    dispatch();
    dispatch();
    assert(delayed_count == 2 && allocations == 1);
    /* Simulate failed delayed-response allocation after the busy handler. */
    delayed_count--;
    transaction_completed(id, FWK_SUCCESS);
    dispatch();
    assert(allocations == 0 && ctx.pending_requests == NULL);
    assert(ctx.state == MOD_I2C_DEV_IDLE && delayed_count == 0);

    /* Terminal panic releases owned busy descriptors, not undelivered ones. */
    assert(transmit_as_controller(id, 0x48, tx, 1) == FWK_PENDING);
    assert(receive_as_controller(id, 0x48, rx, 1) == FWK_PENDING);
    assert(receive_as_controller(id, 0x48, rx, 1) == FWK_PENDING);
    dispatch();
    dispatch();
    assert(allocations == 2 && queued_count == 1);
    {
        struct fwk_event bad = {
            .target_id = id,
            .id = FWK_ID_EVENT_INIT(FWK_MODULE_IDX_I2C,
                MOD_I2C_EVENT_IDX_TOTAL_COUNT),
        };
        struct fwk_event response;

        assert(mod_i2c_process_event(&bad, &response) == FWK_E_PANIC);
    }
    assert(ctx.state == MOD_I2C_DEV_PANIC);
    assert(allocations == 1 && ctx.pending_requests == NULL);
    {
        struct fwk_event stale_reload = {
            .target_id = id,
            .id = FWK_ID_EVENT_INIT(FWK_MODULE_IDX_I2C,
                MOD_I2C_EVENT_IDX_RELOAD),
        };
        struct fwk_event response;

        assert(mod_i2c_process_event(&stale_reload, &response) == FWK_E_PANIC);
        stale_reload.id = mod_i2c_event_id_request_completed;
        assert(mod_i2c_process_event(&stale_reload, &response) == FWK_E_PANIC);
        assert(allocations == 1);
    }
    dispatch();
    assert(allocations == 0);
    puts("I2C asynchronous requests and allocation lifetime: PASS");
    return 0;
}
