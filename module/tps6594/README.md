# TPS6594 early board presence check

The opt-in Apollo QVP `SCP_APOLLO_QVP_PMIC` build inserts this module before
`ppu-v1`, `system-power` and `power-domain`. Its synchronous `init` callback
checks the single PMIC at `0x48` before framework power initialization.
The default path performs exactly one read of the side-effect-free `DEV_REV`
register and no PMIC register writes. BUCK/LDO voltage, enable, GPIO, interrupt
and retained register state remain unchanged. An I2C timeout or NACK prevents
subsequent initialization.
The default FVP firmware does not include this module.

SI CL0 owns a QVP-only DW APB I2C controller at `0x2a800000`. Its polled
transport uses explicit repeated START and STOP, checks abort and FIFO status,
and bounds each transaction by a board-configured Generic Timer deadline. QVP
uses 100 ms to allow 10 ms quantum scheduling jitter: a completed nine-byte
read was observed at 12.355 ms CPU counter time. This budget is not a sleep;
STOP completes the transfer immediately. Completion is sampled again after
draining RX and before reporting timeout, since MMIO can yield to the model.
A timeout poisons the controller until reboot: disabling can flush queued
commands without canceling a command already in flight. The driver does
not require framework timer binding or interrupt delivery during early init.
Transfers contain at most 15 data bytes, below the QVP 16-entry RX FIFO.
The transport is separate from PMIC register policy for testing and reuse.

The ready log reports `policy=preserve`; its check line reports `probe=PASS`,
`rail_config=SKIP`, and `gpio_test=SKIP`. Presence verification does not claim
regulator programming, GPIO traffic or fault handling validation. All nine
regulator and eleven GPIO APIs remain available for explicit later use.
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
This diagnostic must not be used unchanged on boards with connected loads.
Runtime APIs expose voltage, enable, GPIO direction/value and fault
snapshot operations to other SCP modules. Calls are serialized by the firmware
thread; this API is not suitable for interrupt callbacks or concurrent masters.
This is board bring-up, not a replacement for existing mock-PSU DVFS wiring.

Native policy test (from the SCP repository; output directory supplied by user):

```sh
cc -std=c11 -Wall -Wextra -Werror -Iframework/include \
  -Imodule/tps6594/include -Imodule/tps6594/src \
  module/tps6594/src/tps6594.c module/tps6594/test/test_tps6594.c \
  -o /tmp/test-tps6594
/tmp/test-tps6594
```

These tests assert that the minimal probe makes one read and zero writes,
preserves all register bytes and propagates probe errors. They also cover
selector boundaries, reserved/enable/RTC preservation, banked GPIO addressing,
W1C-only diagnostic initialization and failure propagation at every diagnostic
transaction. They do not substitute for QVP controller/IRQ/traffic
validation or establish analog settling-time fidelity.
