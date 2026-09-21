# Polled transfer host test

`test_polled.c` drives the synchronous driver through a mocked timer API and
memory-backed controller registers. It checks final STOP, repeated START for a
register read, receive data, abort handling, timeout poisoning, timer/disable
error propagation, and rejection of retries after an uncertain transfer.

From the SCP repository, set `generated_include` to the framework include
directory generated for a firmware containing `timer`, `i2c`, and `dw_apb_i2c`,
and `test_output` to an existing build-output directory:

```sh
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter \
  -ffunction-sections -fdata-sections \
  -Iframework/include -Iarch/none/host/include \
  -Imodule/timer/include -Imodule/i2c/include \
  -Imodule/dw_apb_i2c/include -Imodule/dw_apb_i2c/src \
  -I"$generated_include" module/dw_apb_i2c/test/test_polled.c \
  -Wl,--gc-sections -o "$test_output/test-dw-apb-i2c-polled"
"$test_output/test-dw-apb-i2c-polled"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter \
  -ffunction-sections -fdata-sections \
  -Iframework/include -Iarch/none/host/include -Imodule/i2c/include \
  -I"$generated_include" module/i2c/test/test_sync.c \
  -Wl,--gc-sections -o "$test_output/test-i2c-sync"
"$test_output/test-i2c-sync"
```

The HAL test checks driver-ID forwarding, exact synchronous error propagation,
busy rejection, safe rejection of asynchronous calls on a synchronous-only
driver, and continued event queuing for the existing asynchronous interface.

These mocks do not establish interrupt, electrical bus timing, or hardware FIFO
behavior; platform traffic remains required for runtime qualification.
