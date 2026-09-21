/* SPDX-License-Identifier: BSD-3-Clause */
#include "../src/mod_i2c.c"

#include <assert.h>
#include <stdio.h>

static unsigned int calls, events;

void *fwk_mm_alloc_notrap(size_t count, size_t size)
{
    return calloc(count, size);
}

void fwk_mm_free(void *ptr)
{
    free(ptr);
}

bool fwk_module_is_valid_element_id(fwk_id_t id)
{
    return id.common.type == FWK_ID_TYPE_ELEMENT;
}

unsigned int fwk_id_get_module_idx(fwk_id_t id)
{
    return id.common.module_idx;
}

unsigned int fwk_id_get_element_idx(fwk_id_t id)
{
    return id.element.element_idx;
}

int __fwk_put_event(struct fwk_event *event)
{
    /* This test does not dispatch queued asynchronous requests. */
    fwk_mm_free(event_request(event));
    events++;
    return FWK_SUCCESS;
}

static int driver_transfer(fwk_id_t id, struct mod_i2c_request *request)
{
    assert(id.value == FWK_ID_ELEMENT(FWK_MODULE_IDX_I2C, 7).value);
    assert(request->transmit_byte_count == 1);
    calls++;
    return FWK_E_TIMEOUT;
}

int main(void)
{
    const fwk_id_t id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_I2C, 0);
    const struct mod_i2c_dev_config config = {
        .driver_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_I2C, 7),
    };
    struct mod_i2c_driver_api driver = {
        .transfer_as_controller = driver_transfer,
    };
    struct mod_i2c_dev_ctx ctx = {
        .config = &config,
        .driver_api = &driver,
    };
    uint8_t data = 0x10;
    struct mod_i2c_request request = {
        .target_address = 0x48,
        .transmit_byte_count = 1,
        .transmit_data = &data,
    };

    ctx_table = &ctx;
    assert(transfer_as_controller(id, &request) == FWK_E_TIMEOUT);
    assert(calls == 1 && events == 0 && ctx.state == MOD_I2C_DEV_IDLE);
    ctx.state = MOD_I2C_DEV_RX;
    assert(transfer_as_controller(id, &request) == FWK_E_BUSY);
    assert(calls == 1);
    ctx.state = MOD_I2C_DEV_IDLE;
    assert(transmit_as_controller(id, 0x48, &data, 1) == FWK_E_SUPPORT);
    assert(receive_as_controller(id, 0x48, &data, 1) == FWK_E_SUPPORT);
    assert(transmit_then_receive_as_controller(
        id, 0x48, &data, &data, 1, 1) == FWK_E_SUPPORT);
    assert(events == 0);

    driver.transfer_as_controller = NULL;
    assert(transfer_as_controller(id, &request) == FWK_E_SUPPORT);
    driver.transmit_as_controller = driver_transfer;
    assert(transmit_as_controller(id, 0x48, &data, 1) == FWK_PENDING);
    assert(events == 1 && calls == 1);

    assert(transfer_as_controller(id, NULL) == FWK_E_PARAM);
    request.target_address = 0x80;
    assert(transfer_as_controller(id, &request) == FWK_E_PARAM);
    assert(transfer_as_controller(FWK_ID_MODULE(FWK_MODULE_IDX_I2C), &request) ==
        FWK_E_PARAM);
    puts("I2C synchronous HAL: PASS");
    return 0;
}
