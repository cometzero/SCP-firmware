# TPS6594 board driver

The opt-in Apollo QVP `SCP_APOLLO_QVP_PMIC` build starts the timer and
`dw-apb-i2c`/`i2c` before this module, followed by `gpio`, `pmic`, `ppu-v1`,
`system-power` and `power-domain`. Its synchronous `start` callback checks the
single PMIC at `0x48`. SYS0 default power-on is deferred from PPU element init
to start for this configuration, so probe failure still prevents that power-on.
The default path reads `DEV_REV` plus eight read-only status blocks, with no
PMIC register writes. BUCK/LDO voltage, enable, GPIO, interrupt and retained
register state remain unchanged. An I2C timeout or NACK prevents startup.
The default FVP firmware does not include this module.

SI CL0 owns a QVP-only DW APB I2C controller at `0x2a800000`. Its polled
transport uses `mod_i2c_api.transfer_as_controller`, backed by the polled
`dw_apb_i2c` driver. The driver uses explicit repeated START and STOP, checks
abort and FIFO status, and bounds transactions with the timer HAL API. QVP
uses 100 ms to allow 10 ms quantum scheduling jitter: a completed nine-byte
read was observed at 12.355 ms CPU counter time. This budget is not a sleep;
STOP completes the transfer immediately. Completion is sampled again after
draining RX and before reporting timeout, since MMIO can yield to the model.
A timeout poisons the controller until reboot: disabling can flush queued
commands without canceling a command already in flight. The driver does
not require I2C interrupt delivery. Timer/I2C binding completes before startup.
Transfers contain at most 15 data bytes, below the QVP 16-entry RX FIFO.
The transport is separate from PMIC register policy for testing and reuse.

Probe logs each BUCK/LDO enable bit, selected raw VOUT register (including
BUCK VOUT bank), and its shared live STAT register. GPIO1..11 logs show mux,
direction, output latch and sampled input. These values are register snapshots,
not measured analog voltages; the multi-transfer snapshot is not atomic.
Logging precedes optional configuration and avoids latched INT/RTC reads.
A failed snapshot aborts startup without printing a partial snapshot.

The ready log reports `policy=preserve`; its check line reports `probe=PASS`,
`rail_config=SKIP`, and `gpio_test=SKIP`. Presence verification does not claim
regulator programming, GPIO traffic or fault handling validation. All nine
regulator and eleven GPIO APIs remain available for explicit later use.
The `module/gpio` HAL also exposes all eleven pins. Each HAL element binds the
TPS6594 `MOD_TPS6594_API_IDX_GPIO` driver API using a driver element whose
`mod_tps6594_element_config` selects a zero-based PMIC index and pin. Apollo maps
HAL elements 0..10 to PMIC 0 pins 0..10 (hardware GPIO1..11). The legacy PMIC
API remains at index zero. Calls are synchronous and raw high/low; direction
selection switches mux to GPIO while preserving pull, drive and deglitch bits.
GPIO IRQ callbacks and active-low translation are not provided by this HAL.
The `module/pmic` HAL exports the nine rails through a separate TPS6594 PMIC
driver element. GPIO elements use `MOD_TPS6594_ELEMENT_GPIO`; PMIC elements
use `MOD_TPS6594_ELEMENT_PMIC`. The driver API is
`MOD_TPS6594_API_IDX_PMIC_DRIVER`; the existing module-level API at index zero
remains available for compatibility. Voltage getters decode the active BUCK
VOUT bank and reject reserved LDO selectors. Values represent programmed
voltage/enable state, not analog voltage or power-good status.

Apollo RAMFW `config_pmic.c` maps PMIC HAL element 0 to the TPS6594 PMIC element
after its eleven GPIO elements. `config_si0_platform.c` supplies this HAL ID
and nine rails to `si0_platform`, which binds the HAL and reads voltage/enable
at start before notifying subsystem initialization. This adds 23 read-only
transactions after the driver's nine probe/status reads. The default firmware
does not program rail voltages or enables; mock-PSU DVFS wiring is retained.
The fault API reads live `STAT_BUCK1_2..STAT_READBACK_ERR`, without clearing
latched INT registers. Asynchronous PMIC IRQ dispatch is not implemented.
The module does not operate RTC registers or LDORTC; Linux retains PL031 RTC.

For explicit diagnostics, board configuration may set `configure_registers`
and supply all nine target voltages in `rail_uv`. This optional path configures
the active BUCK VOUT1/VOUT2 bank according to retained VSEL, preserves enable
bits and inactive voltage banks, masks/clears rail/VMON/GPIO interrupt leaves,
and sets GPIOs to input with register readback. It takes 17 I2C transfers.
Setting `gpio_self_test` additionally requires `configure_registers`: it runs
push-pull walking-one/all-zero tests and GPIO0-to-1/GPIO8-to-9 input loopbacks,
restores directions/latches, then clears and checks test-generated GPIO IRQs.
This adds 37 transfers. Both options are disabled in the shipped QVP config.
The standalone configuration routine takes 17 transfers; module startup adds
the preceding nine probe/status reads.
This diagnostic must not be used unchanged on boards with connected loads.
Runtime APIs expose voltage, enable, GPIO direction/value and fault
snapshot operations to other SCP modules. Calls are serialized by the firmware
thread; this API is not suitable for interrupt callbacks or concurrent masters.
This is board bring-up, not a replacement for existing mock-PSU DVFS wiring.

Run all seven native policy/HAL/transport tests from the SCP repository:

```sh
bash module/tps6594/test/run_tests.sh /path/to/build/tps6594-tests
```

Native policy test alone (set `test_output` to an existing build directory):

```sh
cc -std=c11 -Wall -Wextra -Werror -Iframework/include \
  -Imodule/tps6594/include -Imodule/tps6594/src \
  module/tps6594/src/tps6594.c module/tps6594/test/test_tps6594.c \
  -o "$test_output/test-tps6594"
"$test_output/test-tps6594"
```

These tests assert that the minimal probe makes one read and zero writes,
preserves all register bytes and propagates probe errors. Status tests check
all eight read blocks, no writes/INT/RTC access, and each failed read. They cover
selector boundaries, reserved/enable/RTC preservation, banked GPIO addressing,
W1C-only diagnostic initialization and failure propagation at every diagnostic
transaction. They do not substitute for QVP controller/IRQ/traffic
validation or establish analog settling-time fidelity.

## QVP GPIO and PMIC HAL runtime diagnostic

`SCP_APOLLO_QVP_PMIC_GPIO_TEST=ON` (default OFF) adds the test module in
`test/runtime` after GPIO startup and before PPU startup. It requires
`SCP_APOLLO_QVP_PMIC=ON` and the Apollo QVP fixture with all pins disconnected
except zero-based GPIO0->1 and GPIO8->9 loopbacks. Do not enable it on a board
with connected loads.

The test binds the public GPIO HAL and drives every pin high then low. It
checks all eleven input readbacks after each write (242 comparisons), then
checks both loopbacks with levels 0101 (8 comparisons). Pin direction, writes
and reads all traverse GPIO -> TPS6594 -> I2C HAL -> DW APB I2C. Direct I2C
access is used only to prepare push-pull drive and save/restore the fixture.
It verifies restored configuration, output latches and GPIO interrupt leaves;
preexisting interrupt leaves are preserved. Failure aborts startup.

QBox validation on 2026-09-21 passed all 250 comparisons, restoration and
subsequent BSP boot/login. Logs are under workspace
`build/qbox-apollo-qvp/tps6594/gpio-runtime/`; `gpio-diagnostic.sha256` identifies
the loader-override binary. Output-mode GPIO_IN readback comes from model
output state; only the two wired loopbacks independently validate external
GPIO signal propagation. This does not qualify physical pin timing or PMIC IRQ
delivery. The existing `gpio_self_test` board option is separate and remains
disabled, as does this runtime diagnostic in the default build.

The diagnostic also binds PMIC HAL element 0, saves all nine original rail
voltages/enables, and performs voltage programming/readback and enable
toggle/readback on each rail (18 checks). It attempts to restore every rail
and reads back voltage/enable even if a test fails. This fixture is QVP-only;
it does not validate voltage ramp timing or physical rail sequencing.
The PMIC HAL diagnostic and subsequent RAMFW status reads passed on QVP with
all nine rail settings restored; see workspace
`build/qbox-apollo-qvp/tps6594/pmic-validation.md` for the binary hashes and logs.

I2C asynchronous event descriptors now remain inside `module/i2c`; events
carry only a native pointer. `fwk_event.h` keeps its original 16-byte payload.
Synchronous PMIC/GPIO transfers do not allocate queued request descriptors.
