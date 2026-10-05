/* SPDX-License-Identifier: BSD-3-Clause */
#include <assert.h>
#include <stdio.h>
#include "../src/mod_vmcu_safety.c"
static struct mod_pfdi_monitor_status cores[16];
static uint32_t gpio[0x500 / 4];
static uint64_t ticks;
static unsigned int alarm_starts, poll_events;
static uint64_t alarm_due;
int __fwk_put_event(struct fwk_event *event)
{ (void)event; poll_events++; return 0; }
static int schedule(fwk_id_t id, unsigned int us, enum mod_timer_alarm_type type,
    void (*callback)(uintptr_t), uintptr_t arg)
{
    (void)id; (void)callback; (void)arg;
    assert(type == MOD_TIMER_ALARM_TYPE_ONCE && us == POLL_US);
    alarm_starts++; alarm_due = ticks + us / 1000; return 0;
}
static const struct mod_timer_alarm_api alarms = { .start = schedule };
static int query(fwk_id_t id, struct mod_pfdi_monitor_status *state)
{ *state = cores[fwk_id_get_element_idx(id)]; return 0; }
static const struct mod_pfdi_monitor_status_api source = { .get = query };
int fwk_io_getch(const struct fwk_io_stream *stream, char *ch)
{ (void)stream; (void)ch; return FWK_PENDING; }
static int counter(fwk_id_t id, uint64_t *now) { (void)id; *now = ticks; return 0; }
static const struct mod_timer_api clock_api = { .get_counter = counter };
static int putch(const struct fwk_io_stream *stream, char ch)
{ (void)stream; (void)ch; return FWK_E_BUSY; }
static const struct fwk_io_adapter adapter = { .putch = putch };
static void request(uint8_t *p, uint32_t txid, uint32_t op, uint32_t arg)
{
    frame(p, 5, 0, txid, op, arg); p[6] = 2;
    write32(p + 24, crc32(p, 24));
}
void fwk_log_printf(const char *format, ...) { (void)format; }
static unsigned int fake_power, fake_recovery;
static int recover(void) { fake_recovery = 1; return 0; }
static unsigned int recovery_status(void) { return fake_recovery; }
static int arm(bool active) { fake_power = active ? 1 : 0; return 0; }
static unsigned int power_status(void) { return fake_power; }
static int wake(void) { fake_power = 4; return 0; }
static const struct mod_si0_vmcu_api controls = {
    .recover = recover, .recovery_status = recovery_status,
    .power_arm = arm, .power_status = power_status, .wake = wake };
static int voltage(fwk_id_t id, unsigned int rail, uint32_t *uv)
{ (void)id; *uv = 800000 + rail; return 0; }
static int enabled(fwk_id_t id, unsigned int rail, bool *value)
{ (void)id; (void)rail; *value = true; return 0; }
static const struct mod_pmic_api rails = { .get_voltage = voltage, .get_enabled = enabled };
static int faults(unsigned int pmic, uint8_t values[11])
{ (void)pmic; for (unsigned i = 0; i < 11; i++) values[i] = i + 16; return 0; }
static const struct mod_tps6594_api fault_source = { .read_faults = faults };
int main(void)
{
    struct mod_vmcu_safety_config config = {
        .gpio_base = (uintptr_t)gpio, .ap_count = 16 };
    uint32_t progress, argument;
    uint8_t bytes[28];
    unsigned int i;
    cfg = &config; pfdi_api = &source; control_api = &controls;
    pmic_api = &rails; fault_api = &fault_source; epoch = 0x12345678;
    timer_api = &clock_api; uart.adapter = &adapter; frequency = 1000;
    alarm_api = &alarms;
    assert(crc32((const uint8_t *)"123456789", 9) == 0xcbf43926);
    assert(snapshot(&progress, &argument) == 0 && progress == 0xffff);
    for (i = 0; i < 16; i++) cores[i].online = true;
    assert(snapshot(&progress, &argument) == FLAG_READY && progress == UINT32_MAX);
    config.ap_count = 4;
    assert(snapshot(&progress, &argument) == FLAG_READY && progress == 0x000f000f);
    assert(argument == 0); /* Absent CPUs must not become faults or offline CPUs. */
    config.ap_count = 16;
    cores[3].faults = MOD_PFDI_FAULT_ONLINE;
    assert(snapshot(&progress, &argument) == FLAG_FAULT && argument == 8);
    for (i = 0; i < 16; i++) { cores[i].online = false; cores[i].powered_off = true; }
    assert(snapshot(&progress, &argument) == FLAG_FAULT); /* Off cannot hide a fault. */
    cores[3].faults = 0;
    assert(snapshot(&progress, &argument) == FLAG_ALL_OFF && argument == 0xffff0000);
    send_health(); assert(tx_used == 28 && tx[3] == 7 && tx[6] == 3);
    assert(read32(tx + 24) == crc32(tx, 24)); tx_used = 0;
    request(bytes, 1, 3, 0); command(bytes);
    assert(!reporting && cached_reply[7] == 0); tx_used = 0;
    command(bytes); assert(tx_used == 28 && !memcmp(tx, cached_reply, 28)); tx_used = 0;
    request(bytes, 1, 3, 1); command(bytes);
    assert(tx[7] == 2 && !reporting); tx_used = 0;
    request(bytes, 2, 4, 0); command(bytes);
    assert(tx[7] == 1); tx_used = 0;
    request(bytes, 1, 1, 0); command(bytes); assert(tx[7] == 3); tx_used = 0;
    request(bytes, 3, 6, 0); gpio[0x3fc / 4] = 0x1c; command(bytes);
    assert(tx[7] == 0 && read32(tx + 20) == 0x1c); tx_used = 0;
    request(bytes, 4, 1, 0); bytes[8] ^= 1; command(bytes);
    assert(tx[7] == 3); tx_used = 0;
    request(bytes, 4, 3, 1); bytes[24] ^= 1;
    for (i = 0; i < sizeof(bytes); i++) receive(bytes[i]);
    assert(tx_used == 0 && !reporting);
    request(bytes, 4, 3, 1);
    for (i = 0; i < sizeof(bytes); i++) receive(bytes[i]);
    assert(reporting && tx_used == 28); tx_used = 0;
    request(bytes, 0, 7, 0); command(bytes);
    assert(tx[7] == 0 && read32(tx + 20) == 4 && last_transaction == 4); tx_used = 0;
    request(bytes, 5, 8, 8); command(bytes);
    assert(tx[7] == 0 && read32(tx + 20) == (0x80000000U | 800008)); tx_used = 0;
    request(bytes, 6, 8, 9); command(bytes); assert(tx[7] == 2); tx_used = 0;
    request(bytes, 7, 9, 10); command(bytes);
    assert(tx[7] == 0 && read32(tx + 20) == 26); tx_used = 0;
    request(bytes, 8, 10, 0); command(bytes); assert(fake_recovery == 1); tx_used = 0;
    request(bytes, 9, 11, 0); command(bytes); assert(read32(tx + 20) == 1); tx_used = 0;
    request(bytes, 10, 12, 1); command(bytes); assert(fake_power == 1); tx_used = 0;
    request(bytes, 11, 13, 0); command(bytes); assert(read32(tx + 20) == 1); tx_used = 0;
    fake_power = 0;
    sequence = 0; ticks = 0; process_event(NULL, NULL);
    assert(sequence == 1 && tx_used == 28);
    ticks = 4999; process_event(NULL, NULL); assert(sequence == 1);
    ticks = 5000; process_event(NULL, NULL); assert(sequence == 2);
    tx_used = 0;
    for (i = 0; i < 16; i++) { cores[i].online = true; cores[i].powered_off = false; }
    ticks = 5010; process_event(NULL, NULL);
    assert(sequence == 3 && tx[7] == FLAG_READY && next_report == 10000);
    tx_used = 0;
    cores[0].online = false;
    ticks = 5020; process_event(NULL, NULL);
    assert(sequence == 4 && tx[7] == 0 && next_report == 10000);
    tx_used = 0;
    ticks = 5030; process_event(NULL, NULL);
    assert(sequence == 4 && tx_used == 0);
    cores[0].faults = MOD_PFDI_FAULT_ONLINE;
    ticks = 5100; process_event(NULL, NULL);
    assert(sequence == 5 && tx[7] == FLAG_FAULT);
    tx_used = 0;
    cores[0].faults = 0;
    for (i = 0; i < 16; i++) { cores[i].online = false; cores[i].powered_off = true; }
    ticks = 5200; process_event(NULL, NULL);
    assert(sequence == 6 && tx[7] == FLAG_ALL_OFF);
    for (i = 0; i < 16; i++) { cores[i].online = true; cores[i].powered_off = false; }
    tx_used = TX_SIZE;
    ticks = 5300; process_event(NULL, NULL);
    assert(sequence == 6 && reported_flags == FLAG_ALL_OFF && tx_used == TX_SIZE);
    tx_used = 0;
    ticks = 5310; process_event(NULL, NULL);
    assert(sequence == 7 && reported_flags == FLAG_READY && next_report == 10000);
    tx_used = 0;
    ticks = 9999; process_event(NULL, NULL); assert(sequence == 7 && tx_used == 0);
    ticks = 10000; process_event(NULL, NULL); assert(sequence == 8 && tx_used == 28);
    tx_used = 0;
    request(bytes, 12, 3, 0); command(bytes); assert(!reporting); tx_used = 0;
    cores[0].online = false;
    ticks = 10010; process_event(NULL, NULL);
    assert(sequence == 8 && tx_used == 0 && gpio[0x004 / 4] == 0);
    ticks = 15000; process_event(NULL, NULL);
    assert(sequence == 8 && tx_used == 0 && next_report == 20000);
    request(bytes, 13, 3, 1); command(bytes); assert(reporting); tx_used = 0;
    ticks = 15010; process_event(NULL, NULL);
    assert(sequence == 9 && tx[7] == 0 && next_report == 20000);
    unsigned int before = alarm_starts;
    ticks = 25000; poll_callback(0);
    assert(poll_events == 1 && event_pending && alarm_starts == before);
    /* No timer is rearmed while dispatch is delayed by another five seconds. */
    ticks = 30000; process_event(NULL, NULL);
    assert(alarm_starts == before + 1 && alarm_due == 30010 && !event_pending);
    for (i = 0; i < 100; i++) enqueue(bytes);
    assert(tx_used <= TX_SIZE); /* Busy UART remains bounded. */
    puts("SI safety PASS: wire CRC, PFDI masks, 5s cadence, immediate flag transitions/retry/report-off, RPC epoch/duplicate/stale, GPIO, RX resync, bounded TX");
    return 0;
}
