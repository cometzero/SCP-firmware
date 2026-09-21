# SPDX-License-Identifier: BSD-3-Clause
add_library(${SCP_MODULE_TARGET} SCP_MODULE)
target_sources(${SCP_MODULE_TARGET} PRIVATE "mod_test_tps6594.c")
target_link_libraries(${SCP_MODULE_TARGET} PRIVATE module-gpio module-i2c module-pmic)
