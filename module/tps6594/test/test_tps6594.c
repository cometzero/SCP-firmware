/* SPDX-License-Identifier: BSD-3-Clause */
#include "tps6594.h"

#include <fwk_status.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t registers[256];
static unsigned int transfers;
static unsigned int writes;
static int fail_at;
static bool bad_readback;

static int transfer(void *ctx, uint8_t address, uint8_t reg,
    uint8_t *data, size_t count, bool read)
{
    size_t i;

    (void)ctx;
    assert(address == 0x48);
    assert(count && count <= 15);
    /* No RTC, LDORTC, reserved summary/status writes. */
    assert(reg + count <= 0x78);
    assert(!(reg <= 0x22 && reg + count > 0x22));
    if (!read) {
        assert(!(reg <= 0x5f && reg + count > 0x5f));
        if (reg <= 0x63 && reg + count > 0x63)
            assert(reg == 0x63 && count == 1 && data[0] == 7);
    }
    transfers++;
    if (!read)
        writes++;
    if (fail_at > 0 && transfers == (unsigned int)fail_at)
        return FWK_E_TIMEOUT;
    for (i = 0; i < count; i++) {
        if (read)
            data[i] = registers[reg + i] ^
                (bad_readback && reg == 0x49 ? 1 : 0);
        else if (reg + i >= 0x5c && reg + i <= 0x64)
            registers[reg + i] &= ~data[i];
        else
            registers[reg + i] = data[i];
    }
    return FWK_SUCCESS;
}

int main(void)
{
    const struct tps6594_bus bus = { .transfer = transfer };
    uint32_t uv[9] = {
        900000, 900000, 900000, 900000, 900000,
        1800000, 1800000, 1800000, 1800000,
    };
    unsigned int i, successful_count;
    uint8_t before[256];
    bool value;

    memset(registers, 0xa5, sizeof(registers));
    memcpy(before, registers, sizeof(before));
    assert(tps6594_probe(&bus, 0x48) == FWK_SUCCESS);
    assert(transfers == 1 && writes == 0);
    assert(memcmp(before, registers, sizeof(before)) == 0);
    transfers = 0;
    fail_at = 1;
    assert(tps6594_probe(&bus, 0x48) == FWK_E_TIMEOUT);
    assert(transfers == 1 && writes == 0);
    assert(memcmp(before, registers, sizeof(before)) == 0);
    fail_at = 0;
    transfers = 0;
    assert(tps6594_init(&bus, 0x48, uv) == FWK_SUCCESS);
    successful_count = transfers;
    assert(successful_count == 17);
    for (i = 0; i < 5; i++) {
        assert(registers[0x0e + i * 2] == 0x4b);
        assert(registers[0x0f + i * 2] == 0xa5);
        assert(registers[0x04 + i * 2] == 0xa5);
    }
    for (i = 0; i < 3; i++)
        assert(registers[0x23 + i] == 0xb9);
    assert(registers[0x26] == 0xb8);
    assert(registers[0x22] == 0xa5);
    assert(tps6594_voltage(&bus, 0x48, 0, 580001) == FWK_E_RANGE);
    assert(tps6594_voltage(&bus, 0x48, 0, 3340000) == FWK_SUCCESS);
    assert(registers[0x0e] == 0xff);
    assert(tps6594_voltage(&bus, 0x48, 9, 900000) == FWK_E_PARAM);
    assert(tps6594_voltage(&bus, 0x48, 8, 1199999) == FWK_E_RANGE);
    assert(tps6594_enable(&bus, 0x48, 8, false) == FWK_SUCCESS);
    assert(registers[0x20] == 0xa4);
    assert(tps6594_gpio_direction(&bus, 0x48, 10, true) == FWK_SUCCESS);
    assert((registers[0x3b] & 0xe1) == 1);
    assert(tps6594_gpio_write(&bus, 0x48, 10, true) == FWK_SUCCESS);
    assert(registers[0x3e] & 4);
    registers[0x40] = 4;
    assert(tps6594_gpio_read(&bus, 0x48, 10, &value) == FWK_SUCCESS && value);
    assert(tps6594_gpio_read(&bus, 0x48, 11, &value) == FWK_E_PARAM);
    /* Retained VSEL must select VOUT2 without overwriting inactive VOUT1. */
    registers[0x04] |= 8;
    registers[0x0e] = 0x42;
    assert(tps6594_init(&bus, 0x48, uv) == FWK_SUCCESS);
    assert(registers[0x04] & 8);
    assert(registers[0x0e] == 0x42 && registers[0x0f] == 0x4b);
    assert(tps6594_voltage(&bus, 0x48, 0, 1100000) == FWK_SUCCESS);
    assert(registers[0x0e] == 0x42 && registers[0x0f] == 0x73);
    for (i = 1; i <= successful_count; i++) {
        transfers = 0;
        fail_at = i;
        assert(tps6594_init(&bus, 0x48, uv) == FWK_E_TIMEOUT);
        assert(transfers == i);
    }
    fail_at = 0;
    bad_readback = true;
    assert(tps6594_init(&bus, 0x48, uv) == FWK_E_DEVICE);
    puts("TPS6594 unit tests PASS: read-only probe, selectors, GPIO, W1C, errors");
    return 0;
}
