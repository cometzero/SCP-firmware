#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

test_output="${1:?Usage: bash module/tps6594/test/run_tests.sh OUTPUT_DIR}"
mkdir -p "$test_output"
test_output="$(cd "$test_output" && pwd)"
cd "$(dirname "${BASH_SOURCE[0]}")/../../.."

flags=(-std=c11 -Wall -Wextra -Werror -Wno-unused-parameter
    -Imodule/tps6594/test/host_include -Iframework/include
    -Imodule/tps6594/include -Imodule/tps6594/src
    -Imodule/gpio/include -Imodule/pmic/include -Imodule/i2c/include -Imodule/timer/include)

cc "${flags[@]}" module/tps6594/src/tps6594.c \
    module/tps6594/test/test_tps6594.c -o "$test_output/test-policy"
cc "${flags[@]}" framework/src/fwk_id.c module/gpio/src/mod_gpio.c \
    module/gpio/test/test_gpio.c -o "$test_output/test-gpio"
cc "${flags[@]}" framework/src/fwk_id.c module/pmic/src/mod_pmic.c \
    module/pmic/test/test_pmic.c -o "$test_output/test-pmic"
cc "${flags[@]}" -DFWK_LOG_LEVEL=1 framework/src/fwk_id.c \
    module/gpio/src/mod_gpio.c module/pmic/src/mod_pmic.c module/tps6594/src/mod_tps6594.c \
    module/tps6594/src/tps6594.c module/tps6594/src/tps6594_i2c.c \
    module/tps6594/test/test_tps6594_module.c -o "$test_output/test-module"
cc "${flags[@]}" -ffunction-sections -fdata-sections \
    -Iarch/none/host/include -Imodule/dw_apb_i2c/include \
    -Imodule/dw_apb_i2c/src module/dw_apb_i2c/test/test_polled.c \
    -Wl,--gc-sections -o "$test_output/test-dw-polled"
cc "${flags[@]}" -ffunction-sections -fdata-sections \
    -Iarch/none/host/include module/i2c/test/test_sync.c \
    -Wl,--gc-sections -o "$test_output/test-i2c-sync"
cc "${flags[@]}" -ffunction-sections -fdata-sections \
    -Iarch/none/host/include framework/src/fwk_id.c module/i2c/test/test_async.c \
    -Wl,--gc-sections -o "$test_output/test-i2c-async"

for test in policy gpio pmic module dw-polled i2c-sync i2c-async; do
    "$test_output/test-$test"
done
