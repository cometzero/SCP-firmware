# GPIO HAL

Each GPIO element forwards synchronous direction, output-latch and input-level
operations to the pin specified by `mod_gpio_dev_config.driver_id`, using
`driver_api_id` to bind `mod_gpio_driver_api`. Clients bind
`MOD_GPIO_API_IDX_GPIO` and use the GPIO element ID with `mod_gpio_api`.

`set_direction(id, true)` selects output; `false` selects input. `write()`
updates the output latch without changing direction. `read()` samples the pin.
Levels are physical high/low values without active-low inversion. Pin muxing,
electrical restrictions and bus error reporting belong to the driver.
Unsupported driver callbacks return `FWK_E_SUPPORT`.

Run the standalone HAL test from the SCP-firmware root, with `test_gpio` placed
in a chosen build output directory:

```sh
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Iframework/include -Imodule/gpio/include \
    framework/src/fwk_id.c module/gpio/src/mod_gpio.c \
    module/gpio/test/test_gpio.c -o /path/to/build/test_gpio
/path/to/build/test_gpio
```

The test covers driver ID forwarding, both directions, output/input levels,
unsupported operations, invalid IDs, NULL read arguments, binding failures and
driver error propagation. It does not qualify a physical GPIO controller.
