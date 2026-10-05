#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail
output="${1:?Usage: run_tests.sh OUTPUT}"
mkdir -p "$output"
output="$(cd "$output" && pwd)"
cd "$(dirname "${BASH_SOURCE[0]}")/../../../../../.."
module=product/automotive-rd/apollo-qvp/module/vmcu_safety
flags=(-std=c11 -Wall -Wextra -Werror -Wno-unused-parameter
    -ffunction-sections -fdata-sections -DFWK_LOG_LEVEL=0 -DBUILD_HAS_NOTIFICATION -DBUILD_HAS_MOD_VMCU_SAFETY
    -I"$module/test/include" -I"$module/include"
    -Iproduct/automotive-rd/apollo-qvp/module/pfdi_monitor/include
    -Iframework/include -Iarch/none/host/include
    -Imodule/timer/include -Imodule/power_domain/include
    -Imodule/pmic/include -Imodule/tps6594/include
    -Iproduct/automotive-rd/apollo-qvp/module/si0_platform/include
    -Iproduct/automotive-rd/apollo-qvp/module/si0_platform/test
    -Imodule/apcontext/include -Imodule/ppu_v1/include -Imodule/scmi/include
    -Imodule/sds/include -Imodule/transport/include -Imodule/system_power/include
    -Imodule/scmi_system_power/include)
for test in snapshot safety control rse; do
    cc "${flags[@]}" framework/src/fwk_id.c "$module/test/test_$test.c" \
        -Wl,--gc-sections -o "$output/test-$test"
    "$output/test-$test"
done
