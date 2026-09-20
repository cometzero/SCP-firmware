/* SPDX-License-Identifier: BSD-3-Clause */
#include "tps6594.h"

#include <fwk_status.h>
#include <fwk_log.h>

#define REG(i2c, offset) (*(volatile uint32_t *)((i2c)->base + (offset)))
#define CON 0x00
#define TAR 0x04
#define DATA 0x10
#define MASK 0x30
#define RAW 0x34
#define CLEAR 0x40
#define ENABLE 0x6c
#define STATUS 0x70
#define ENABLE_STATUS 0x9c
#define TXFLR 0x74
#define RXFLR 0x78
#define ABORT_SOURCE 0x80
#define COMP_TYPE 0xfc
#define ABORT (1U << 6)
#define STOP (1U << 9)
#define READ (1U << 8)
#define RESTART (1U << 10)
#define RFNE (1U << 3)
#define TFNF (1U << 1)
static int disabled(struct tps6594_i2c *i2c)
{
    uint64_t start = i2c->time_us();

    REG(i2c, ENABLE) = 0;
    while (REG(i2c, ENABLE_STATUS) & 1U) {
        if (i2c->time_us() - start >= i2c->timeout_us) {
            i2c->failed = true;
            FWK_LOG_ERR("[TPS6594-I2C] disable timeout enable=0x%x status=0x%x",
                REG(i2c, ENABLE_STATUS), REG(i2c, STATUS));
            return FWK_E_TIMEOUT;
        }
    }
    return FWK_SUCCESS;
}

int tps6594_i2c_init(struct tps6594_i2c *i2c)
{
    int status;
    uint32_t component;

    if (!i2c || !i2c->base || !i2c->time_us || !i2c->timeout_us)
        return FWK_E_PARAM;
    if (i2c->failed)
        return FWK_E_STATE;
    component = REG(i2c, COMP_TYPE);
    if (component != 0x44570140) {
        FWK_LOG_ERR("[TPS6594-I2C] invalid component=0x%x", component);
        i2c->failed = true;
        return FWK_E_DEVICE;
    }
    status = disabled(i2c);
    if (status != FWK_SUCCESS)
        return status;
    REG(i2c, MASK) = 0;
    /* Master, fast speed, restart enabled, slave disabled. */
    REG(i2c, CON) = 0x65;
    (void)REG(i2c, CLEAR);
    return FWK_SUCCESS;
}

int tps6594_i2c_transfer(void *ctx, uint8_t address, uint8_t reg,
    uint8_t *data, size_t count, bool read)
{
    struct tps6594_i2c *i2c = ctx;
    uint64_t start;
    size_t sent = 0, received = 0;
    uint32_t command, irq;
    int status;

    if (!data || count == 0 || count > 15 || address == 0 || address > 0x7f)
        return FWK_E_PARAM;
    if (i2c->failed)
        return FWK_E_STATE;
    status = disabled(i2c);
    if (status != FWK_SUCCESS)
        return status;
    (void)REG(i2c, CLEAR);
    REG(i2c, TAR) = address;
    REG(i2c, ENABLE) = 1;
    start = i2c->time_us();
    for (;;) {
        irq = REG(i2c, RAW);
        if (irq & ABORT) {
            status = FWK_E_DEVICE;
            break;
        }
        while (read && received < count && (REG(i2c, STATUS) & RFNE))
            data[received++] = (uint8_t)REG(i2c, DATA);
        /* MMIO reads yield to the model: STOP may arrive while draining RX. */
        irq = REG(i2c, RAW);
        if (irq & ABORT) {
            status = FWK_E_DEVICE;
            break;
        }
        if (irq & STOP) {
            /* STOP may have arrived after the preceding RFNE sample. */
            while (read && received < count && (REG(i2c, STATUS) & RFNE))
                data[received++] = (uint8_t)REG(i2c, DATA);
            status = (sent == count + 1 && (!read || received == count)) ?
                FWK_SUCCESS : FWK_E_DEVICE;
            break;
        }
        if (sent < count + 1 && (REG(i2c, STATUS) & TFNF)) {
            command = sent == 0 ? reg : (read ? READ : data[sent - 1]);
            if (read && sent == 1)
                command |= RESTART;
            if (sent == count)
                command |= STOP;
            REG(i2c, DATA) = command;
            sent++;
        }
        if (i2c->time_us() - start >= i2c->timeout_us) {
            /* Completion wins if it arrived during the deadline sample. */
            if (REG(i2c, RAW) & (ABORT | STOP))
                continue;
            status = FWK_E_TIMEOUT;
            break;
        }
    }
    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR("[TPS6594-I2C] failed address=0x%02x reg=0x%02x read=%u count=%u error=%d",
            address, reg, read, (unsigned int)count, status);
        FWK_LOG_ERR("[TPS6594-I2C] sent=%u received=%u elapsed_us=%u",
            (unsigned int)sent, (unsigned int)received,
            (unsigned int)(i2c->time_us() - start));
        FWK_LOG_ERR("[TPS6594-I2C] raw=0x%x status=0x%x tx=%u rx=%u abort=0x%x",
            REG(i2c, RAW), REG(i2c, STATUS), REG(i2c, TXFLR),
            REG(i2c, RXFLR), REG(i2c, ABORT_SOURCE));
    }
    if (status == FWK_E_TIMEOUT)
        i2c->failed = true;
    /* Flush queued commands. A popped command may survive: never retry timeout. */
    if (disabled(i2c) != FWK_SUCCESS && status == FWK_SUCCESS)
        status = FWK_E_TIMEOUT;
    (void)REG(i2c, CLEAR);
    return status;
}
