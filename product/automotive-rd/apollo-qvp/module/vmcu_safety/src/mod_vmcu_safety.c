/* SPDX-License-Identifier: BSD-3-Clause */
/* QVP SI CL0 safety endpoint. Wire format is little-endian, CRC32/IEEE. */
#include <mod_vmcu_safety.h>
#include <mod_pfdi_monitor.h>
#include <mod_timer.h>
#include <mod_pmic.h>
#include <mod_tps6594.h>
#include <mod_si0_platform.h>
#include <fwk_core.h>
#include <fwk_io.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>
#include <fwk_status.h>
#include <stdbool.h>
#include <string.h>

#define FRAME_SIZE 28U
#define TX_SIZE 256U
#define REPORT_MS 5000U
#define POLL_US 10000U
#define FLAG_READY 1U
#define FLAG_FAULT 2U
#define FLAG_ALL_OFF 4U

static const struct mod_vmcu_safety_config *cfg;
static const struct mod_timer_alarm_api *alarm_api;
static const struct mod_timer_api *timer_api;
static const struct mod_pfdi_monitor_status_api *pfdi_api;
static const struct mod_pmic_api *pmic_api;
static const struct mod_tps6594_api *fault_api;
static const struct mod_si0_vmcu_api *control_api;
static struct fwk_io_stream uart;
static uint8_t rx[FRAME_SIZE], tx[TX_SIZE], cached_command[FRAME_SIZE];
static uint8_t cached_reply[FRAME_SIZE];
static unsigned int rx_used, tx_head, tx_used;
static uint32_t epoch, sequence, last_transaction, frequency;
static uint64_t next_report, next_gpio;
static bool reporting = true, have_command, event_pending, have_report;
static uint8_t previous_inputs, reported_flags;

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
        (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void write32(uint8_t *p, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        p[i] = (uint8_t)(value >> (8 * i));
}

static uint32_t crc32(const uint8_t *p, unsigned int count)
{
    uint32_t crc = UINT32_MAX;
    unsigned int bit;
    while (count--) {
        crc ^= *p++;
        for (bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((0U - (crc & 1U)) & 0xedb88320U);
    }
    return ~crc;
}

static void frame(uint8_t *p, uint8_t type, uint8_t flags,
    uint32_t transaction, uint32_t progress, uint32_t argument)
{
    p[0] = 0xa5; p[1] = 0x5a; p[2] = 1; p[3] = type;
    p[4] = FRAME_SIZE; p[5] = 0; p[6] = 3; p[7] = flags;
    write32(p + 8, epoch);
    write32(p + 12, transaction);
    write32(p + 16, progress);
    write32(p + 20, argument);
    write32(p + 24, crc32(p, 24));
}

static bool enqueue(const uint8_t *p)
{
    unsigned int i;
    /* A slow/disconnected peer must never block the PFDI event loop. */
    if (TX_SIZE - tx_used < FRAME_SIZE)
        return false;
    for (i = 0; i < FRAME_SIZE; i++) {
        tx[(tx_head + tx_used) % TX_SIZE] = p[i];
        tx_used++;
    }
    return true;
}

static uint32_t gpio_read(void)
{
    return *(volatile uint32_t *)(cfg->gpio_base + 0x3fc) & 0x1f;
}

static void gpio_error(bool level)
{
    /* PL061 masked data alias: only SOC_ERROR changes. */
    *(volatile uint32_t *)(cfg->gpio_base + 0x004) = level ? 1 : 0;
}

static uint8_t snapshot(uint32_t *progress, uint32_t *argument)
{
    uint32_t active = 0, online = 0, faults = 0, off = 0;
    unsigned int i;
    struct mod_pfdi_monitor_status state;
    int status;

    for (i = 0; i < cfg->ap_count; i++) {
        status = pfdi_api->get(FWK_ID_ELEMENT(FWK_MODULE_IDX_PFDI_MONITOR,
            cfg->first_ap + i), &state);
        if (status != FWK_SUCCESS) {
            active |= 1U << i;
            faults |= 1U << i;
            continue;
        }
        if (state.powered_off)
            off |= 1U << i;
        else
            active |= 1U << i;
        if (state.online)
            online |= 1U << i;
        if (state.faults)
            faults |= 1U << i;
    }
    *progress = active | (online << 16);
    *argument = faults | (off << 16);
    if (faults)
        return FLAG_FAULT;
    if (!active)
        return FLAG_ALL_OFF;
    return active == online ? FLAG_READY : 0;
}

static void send_health(void)
{
    uint8_t bytes[FRAME_SIZE], flags;
    uint32_t progress, argument;
    flags = snapshot(&progress, &argument);
    frame(bytes, 7, flags, sequence + 1, progress, argument);
    if (enqueue(bytes)) {
        sequence++;
        reported_flags = flags;
        have_report = true;
    }
}

static void command(const uint8_t *p)
{
    uint8_t reply[FRAME_SIZE], result = 0;
    uint32_t transaction = read32(p + 12), op = read32(p + 16);
    uint32_t argument = read32(p + 20), response = 0;
    bool status_query = false;
    int status = FWK_SUCCESS;

    if (p[3] != 5 || p[6] != 2)
        return;
    if (read32(p + 8) != epoch)
        result = 3;
    else if (op == 7 && !transaction && !p[7] && !argument) {
        frame(reply, 6, 0, 0, op, have_command ? last_transaction : 0);
        enqueue(reply);
        return;
    } else if (!transaction || p[7])
        result = 2;
    else if (have_command && transaction == last_transaction) {
        if (!memcmp(p, cached_command, FRAME_SIZE)) {
            enqueue(cached_reply);
            return;
        }
        result = 2;
    } else if (have_command && (int32_t)(transaction - last_transaction) <= 0)
        result = 3;
    else {
        switch (op) {
        case 1:
        case 2:
            if (argument)
                result = 2;
            else
                status_query = op == 2;
            break;
        case 3:
            if (argument > 1)
                result = 2;
            else
                reporting = argument != 0;
            break;
        case 6:
            if (argument)
                result = 2;
            else
                response = gpio_read();
            break;
        case 8: {
            uint32_t uv;
            bool enabled;
            if (argument >= TPS6594_RAIL_COUNT) {
                result = 2;
                break;
            }
            status = pmic_api->get_voltage(FWK_ID_ELEMENT(FWK_MODULE_IDX_PMIC, 0), argument, &uv);
            if (status == FWK_SUCCESS)
                status = pmic_api->get_enabled(FWK_ID_ELEMENT(FWK_MODULE_IDX_PMIC, 0), argument, &enabled);
            if (status == FWK_SUCCESS)
                response = uv | (enabled ? 0x80000000U : 0);
            break;
        }
        case 9: {
            uint8_t faults[11];
            if (argument >= sizeof(faults)) {
                result = 2;
                break;
            }
            status = fault_api->read_faults(0, faults);
            if (status == FWK_SUCCESS)
                response = faults[argument];
            break;
        }
        case 10:
            if (argument) result = 2;
            else status = control_api->recover();
            break;
        case 11:
            if (argument) result = 2;
            else response = control_api->recovery_status();
            break;
        case 12:
            if (argument > 1) result = 2;
            else status = control_api->power_arm(argument != 0);
            break;
        case 13:
            if (argument) result = 2;
            else response = control_api->power_status();
            break;
        default:
            result = 1;
            break;
        }
        if (status != FWK_SUCCESS) {
            result = status == FWK_E_SUPPORT ? 1 : 2;
            response = (uint32_t)status;
        } else if (op <= 3)
            response = reporting ? 1 : 0;
        frame(reply, 6, result, transaction, op, response);
        memcpy(cached_command, p, FRAME_SIZE);
        memcpy(cached_reply, reply, FRAME_SIZE);
        have_command = true;
        last_transaction = transaction;
        enqueue(reply);
        if (status_query && result == 0)
            send_health();
        return;
    }
    frame(reply, 6, result, transaction, op, response);
    enqueue(reply);
}

static void receive(uint8_t byte)
{
    rx[rx_used++] = byte;
    while (rx_used && (rx[0] != 0xa5 ||
        (rx_used > 1 && rx[1] != 0x5a) ||
        (rx_used > 2 && rx[2] != 1) ||
        (rx_used > 5 && (rx[4] != FRAME_SIZE || rx[5])))) {
        memmove(rx, rx + 1, --rx_used);
    }
    if (rx_used != FRAME_SIZE)
        return;
    if (read32(rx + 24) == crc32(rx, 24)) {
        command(rx);
        rx_used = 0;
    } else {
        memmove(rx, rx + 1, --rx_used);
    }
}

static void poll_callback(uintptr_t unused)
{
    struct fwk_event event = {
        .source_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_VMCU_SAFETY),
        .target_id = FWK_ID_MODULE_INIT(FWK_MODULE_IDX_VMCU_SAFETY),
        .id = FWK_ID_EVENT_INIT(FWK_MODULE_IDX_VMCU_SAFETY, 0),
    };
    (void)unused;
    if (!event_pending && fwk_put_event(&event) == FWK_SUCCESS)
        event_pending = true;
    else if (!event_pending)
        FWK_LOG_ERR("[VMCU_SAFETY] Poll enqueue failed; safety link will time out");
}

static int process_event(const struct fwk_event *event,
    struct fwk_event *response_event)
{
    unsigned int i;
    char ch;
    uint64_t now;
    uint32_t progress, argument;
    uint8_t flags, inputs;
    (void)event;
    (void)response_event;
    event_pending = false;
    for (i = 0; i < 64; i++) {
        if (fwk_io_getch(&uart, &ch) != FWK_SUCCESS)
            break;
        receive((uint8_t)ch);
    }
    if (timer_api->get_counter(cfg->timer_id, &now) != FWK_SUCCESS)
        return FWK_E_DEVICE;
    flags = snapshot(&progress, &argument);
    if (flags != FLAG_READY)
        gpio_error(false);
    else if (now >= next_gpio) {
        gpio_error(!(gpio_read() & 1));
        next_gpio = now + frequency / 2;
    }
    unsigned int power = control_api->power_status();
    /* SOC_PWR_REQ indicates the shutdown request; IST_DONE_N only follows
     * verified AP core OFF, never an acknowledgement or timer expiry. */
    *(volatile uint32_t *)(cfg->gpio_base + 0x018) =
        power == SI0_AP_OFF ? 2 :
        (power == SI0_AP_ARMED || power == SI0_AP_QUIESCING ? 6 : 4);
    inputs = gpio_read() & 0x18;
    if ((inputs & 16) && !(previous_inputs & 16) && power == SI0_AP_OFF) {
        int wake_status = control_api->wake();
        FWK_LOG_INFO("[VMCU_SAFETY] AP wake request status=%d", wake_status);
    }
    if (inputs != previous_inputs) {
        FWK_LOG_INFO("[VMCU_SAFETY] GPIO reset_n=%u wake=%u",
            !!(inputs & 8), !!(inputs & 16));
        previous_inputs = inputs;
    }
    bool periodic = now >= next_report;
    if (periodic)
        next_report = now + (uint64_t)frequency * REPORT_MS / 1000;
    if (reporting && (periodic || !have_report || flags != reported_flags))
        send_health();
    for (i = 0; i < 64 && tx_used; i++) {
        if (uart.adapter->putch(&uart, (char)tx[tx_head]) != FWK_SUCCESS)
            break;
        tx_head = (tx_head + 1) % TX_SIZE;
        tx_used--;
    }
    /* Rearm from the event loop: missed polling ticks have no useful work. */
    return alarm_api->start(cfg->alarm_id, POLL_US,
        MOD_TIMER_ALARM_TYPE_ONCE, poll_callback, 0);
}

static int init(fwk_id_t id, unsigned int count, const void *data)
{
    (void)id; (void)count;
    cfg = data;
    return cfg && cfg->ap_count && cfg->ap_count <= 16 ?
        FWK_SUCCESS : FWK_E_DATA;
}

static int bind(fwk_id_t id, unsigned int round)
{
    int status;
    (void)id;
    if (round)
        return FWK_SUCCESS;
    status = fwk_module_bind(cfg->alarm_id, MOD_TIMER_API_ID_ALARM, &alarm_api);
    if (status != FWK_SUCCESS)
        return status;
    status = fwk_module_bind(cfg->timer_id, MOD_TIMER_API_ID_TIMER, &timer_api);
    if (status != FWK_SUCCESS)
        return status;
    status = fwk_module_bind(FWK_ID_ELEMENT(FWK_MODULE_IDX_PMIC, 0),
        FWK_ID_API(FWK_MODULE_IDX_PMIC, MOD_PMIC_API_IDX_PMIC), &pmic_api);
    if (status != FWK_SUCCESS) return status;
    status = fwk_module_bind(FWK_ID_MODULE(FWK_MODULE_IDX_TPS6594),
        FWK_ID_API(FWK_MODULE_IDX_TPS6594, MOD_TPS6594_API_IDX_PMIC), &fault_api);
    if (status != FWK_SUCCESS) return status;
    status = fwk_module_bind(FWK_ID_MODULE(FWK_MODULE_IDX_SI0_PLATFORM),
        FWK_ID_API(FWK_MODULE_IDX_SI0_PLATFORM, MOD_SI0_PLATFORM_API_IDX_VMCU_CONTROL), &control_api);
    if (status != FWK_SUCCESS) return status;
    return fwk_module_bind(FWK_ID_MODULE(FWK_MODULE_IDX_PFDI_MONITOR),
        FWK_ID_API(FWK_MODULE_IDX_PFDI_MONITOR, MOD_PFDI_MONITOR_API_IDX_STATUS),
        &pfdi_api);
}

static int start(fwk_id_t id)
{
    uint64_t now;
    int status;
    (void)id;
    status = fwk_io_open(&uart, cfg->uart_id,
        FWK_IO_MODE_READ | FWK_IO_MODE_WRITE | FWK_IO_MODE_BINARY);
    if (status != FWK_SUCCESS)
        return status;
    status = timer_api->get_frequency(cfg->timer_id, &frequency);
    if (status != FWK_SUCCESS || frequency < 2)
        return FWK_E_DEVICE;
    status = timer_api->get_counter(cfg->timer_id, &now);
    if (status != FWK_SUCCESS)
        return status;
    epoch = (uint32_t)(now ^ (now >> 32));
    if (!epoch)
        epoch = 1;
    /* Set output latch before directions: IST_DONE_N must stay inactive. */
    *(volatile uint32_t *)(cfg->gpio_base + 0x01c) = 4;
    *(volatile uint32_t *)(cfg->gpio_base + 0x400) = 7;
    previous_inputs = gpio_read() & 0x18;
    next_report = now;
    next_gpio = now;
    FWK_LOG_INFO("[VMCU_SAFETY] INIT uart=0x2a820000 gpio=0x2a830000 "
        "period_ms=5000 source=PFDI epoch=%u", epoch);
    return alarm_api->start(cfg->alarm_id, POLL_US,
        MOD_TIMER_ALARM_TYPE_ONCE, poll_callback, 0);
}

const struct fwk_module module_vmcu_safety = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .init = init,
    .bind = bind,
    .start = start,
    .process_event = process_event,
    .event_count = 1,
};
