# SI CL0 vMCU safety and board control endpoint

This Apollo QVP-only module reports existing PFDI monitor state. It does not
create an AP heartbeat or workload counter or change PFDI deadlines.
`SCP_APOLLO_QVP_VMCU_SAFETY` requires SCMI PFDI monitoring and the SI-owned PMIC.

The console remains PL011 element 0 at `0x2a400000`. Safety traffic uses element
1 at `0x2a820000`, 115200 baud. This UART and PL061 GPIO at `0x2a830000` are QVP
board extensions, not Zena CSS architectural registers. UART RX/TX is bounded
at 64 bytes per iteration with a fixed 256-byte TX queue. Polling work runs in
the SCP event loop, with the next one-shot scheduled 10 ms after completion;
a slow peer never blocks the PFDI event loop. Safety, AP control and RSE recovery
polling skip missed ticks rather than replaying overdue periodic interrupts.
Their absolute control deadlines are unchanged: a delayed observation of cores
already OFF still fails if the confirmation deadline has expired. Poll enqueue
or rearm failures remain visible as an error and a stopped/failing safety link.

Every 5000 ms, type 7 / sender 3 reports configured AP core masks from PFDI's
read-only snapshot API. The 28-byte v1 frame has IEEE CRC32 over its first 24
bytes. `progress` contains active and online masks; `argument` contains latched
fault and powered-off masks (16 bits each, low half first). Only
`PC_CONFIGURED_CORES_COUNT` instantiated cores are queried; absent cores have
zero bits. Flags are READY=1, FAULT=2, ALL_OFF=4. Boot-pending cores never become
ready. OFF/ON clears online evidence until a new successful ONL report. Failure
history remains latched until the existing explicit `prepare_restart` boundary.
CL1 cores are excluded; PFDI input remains SCMI protocol 0x90.
Flag changes (including loss of READY) also produce an immediate report without
moving the regular 5000 ms deadline. This informs the MCU before an intentional
SOC_ERROR heartbeat stop can be mistaken for a stuck GPIO. A full TX queue keeps
the transition pending for the next bounded poll. Disabling periodic reporting
also disables these transition reports; it does not disable the GPIO watchdog.

Commands use type 5 / sender 2 and replies type 6 / sender 3. Replies return
OK=0, UNSUPPORTED=1, INVALID=2 or STALE=3. Framework failures return INVALID and
the signed framework status in `argument`. Epoch, transaction and exact
request contents are checked. An identical duplicate replays its cached reply;
changed duplicates and older transactions cannot repeat an operation.

| Operation | Argument | Successful reply argument |
|---|---|---|
| 1 ping / 2 status | 0 | Reporting enabled; status also emits a safety frame |
| 3 periodic reports | 0/1 | Reporting enabled |
| 6 GPIO read | 0 | GPIO bits 0..4 |
| 7 session query | 0, transaction 0 | Last completed transaction; cache unchanged |
| 8 PMIC rail | Rail 0..8 | Bit 31 programmed enable, bits 30..0 programmed microvolts |
| 9 PMIC faults | Byte 0..10 | Non-destructive TPS6594 live STAT byte |
| 10 AP recovery | 0 | Request queued; poll operation 11 |
| 11 recovery status | 0 | 0 idle, 1 pending, 2 off, 3 reload, 4 boot, 5 complete, 6 failed |
| 12 AP poweroff preparation | 1 arm / 0 cancel | Request accepted |
| 13 AP power status | 0 | 0 run, 1 armed, 2 quiescing, 3 AP off, 4 waking, 5 failed |

Session query lets a restarted MCU resume above the existing cursor without
resetting replay protection. PMIC rail reads use `mod_pmic`; fault reads use
`mod_tps6594`. SI CL0 remains the sole register writer. Programmed enable/voltage
are configuration values, not measured power-good or voltage. No fault bits are
cleared and no rails are switched by these commands.

AP recovery reuses the existing coordinated watchdog recovery path: core PPUs
off, RSE authenticated BL2 reload/acknowledgement, AP PFDI generation reset,
AP context/mailbox reset, boot CPU release and watchdog IRQ rearm. COMPLETE
means firmware handoff and IRQ rearm, not verified Linux/PFDI service recovery.
The latter must be verified separately. The recovery transaction is bounded to
30 simulation seconds and preserves failure state; it never restarts QEMU.

Orderly AP-only poweroff requires the MCU to arm SI CL0 before asking the AP
management service to drain its UART response, sync filesystems and issue Linux
kernel poweroff. Only the trusted PSCI SCMI shutdown channel completes an armed
request. A platform-HANDLED response suppresses global RSE shutdown notification
and avoids the ordinary whole-system power-off path. SI verifies every AP core
PPU is OFF before reporting completion. Arm expires after 30 simulation seconds;
AP off confirmation has a 10-second bound. A timeout remains a failure, never
an inferred completion. Ordinary unarmed shutdown retains its existing behavior.

GPIO 0/1/2 are SOC_ERROR, SOC_PWR_REQ and IST_DONE_N outputs. SOC_ERROR toggles
every 500 ms only while PFDI is ready. SOC_PWR_REQ is high during arm/quiesce/off;
IST_DONE_N goes low only after verified AP core OFF. Inputs 3/4 are SOC_RESET_N
and MCU_SOC_WAKE. A wake rising edge while AP_OFF starts coordinated recovery.
Disabling periodic UART reports leaves PFDI and GPIO active.

This is AP core power management with retained SYSTOP, SI, RSE and PMIC rails.
It does not implement SC7, PMIC rail power cycling or physical electrical timing.
Raw SOC_RESET_N and coordinated AP recovery remain distinct operations.

Native checks:

```sh
bash product/automotive-rd/apollo-qvp/module/vmcu_safety/test/run_tests.sh OUTPUT
```

Tests exercise PFDI state transitions, UART protocol, timer cadence, bounded
queues, PMIC dispatch and control-policy state transitions with host fixtures.
Signed-image boot, live PMIC traffic, AP recovery, shutdown/wake and CAN/SIL Kit
paths require separate full-system runtime evidence.
