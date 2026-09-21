# SPDX-License-Identifier: BSD-3-Clause
add_library(${SCP_MODULE_TARGET} SCP_MODULE)
target_include_directories(${SCP_MODULE_TARGET} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_sources(${SCP_MODULE_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mod_tps6594.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tps6594.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tps6594_i2c.c")
target_link_libraries(${SCP_MODULE_TARGET} PRIVATE
    module-gpio module-pmic module-i2c module-timer)
