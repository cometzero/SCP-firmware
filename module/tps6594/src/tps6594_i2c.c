/* SPDX-License-Identifier: BSD-3-Clause */
#include "tps6594.h"

#include <mod_i2c.h>
#include <fwk_status.h>

#include <string.h>

int tps6594_i2c_transfer(void *ctx, uint8_t address, uint8_t reg,
    uint8_t *data, size_t count, bool read)
{
    struct tps6594_i2c *i2c = ctx;
    uint8_t tx[16];
    struct mod_i2c_request request = {
        .target_address = address,
        .transmit_data = tx,
    };

    if (!i2c || !data || !count || count > sizeof(tx) - 1 ||
        !address || address > 0x7f)
        return FWK_E_PARAM;
    if (!i2c->api || !i2c->api->transfer_as_controller)
        return FWK_E_SUPPORT;
    tx[0] = reg;
    if (read) {
        request.transmit_byte_count = 1;
        request.receive_data = data;
        request.receive_byte_count = count;
    } else {
        memcpy(tx + 1, data, count);
        request.transmit_byte_count = count + 1;
    }
    /* Synchronous HAL call keeps stack buffers alive until STOP or failure. */
    return i2c->api->transfer_as_controller(i2c->id, &request);
}
