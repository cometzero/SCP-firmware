# PMIC HAL

Each element represents one PMIC. `mod_pmic_dev_config` selects the driver
device, driver API and rail count. Clients bind `MOD_PMIC_API_IDX_PMIC` and
call `mod_pmic_api` with a HAL element ID and a zero-based, driver-defined rail
index. The HAL forwards synchronous voltage and enable operations through
`mod_pmic_driver_api`; it checks element IDs, rail bounds and output pointers.
Absent callbacks return `FWK_E_SUPPORT`, and driver errors propagate unchanged.

Voltages are expressed in microvolts. `get_voltage()` reads the programmed
voltage, not a measured output voltage; `get_enabled()` reads the programmed
enable state, not power-good status. Voltage encoding and range constraints,
sequencing, faults and transport access belong to the driver. Reading state
does not program voltage or enable a rail.

From the SCP-firmware root, run the standalone test using a chosen build path:

```sh
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Iframework/include -Imodule/pmic/include \
    framework/src/fwk_id.c module/pmic/src/mod_pmic.c \
    module/pmic/test/test_pmic.c -o /path/to/build/test_pmic
/path/to/build/test_pmic
```

The test checks all nine configured rails, device and rail forwarding,
voltage and enable readback, invalid configuration/IDs/rails/pointers, missing
callbacks, initialization state, binding failures and driver error propagation.
It does not qualify physical PMIC voltage, timing or power sequencing.
