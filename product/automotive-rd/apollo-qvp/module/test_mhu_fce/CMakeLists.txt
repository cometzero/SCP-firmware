# SPDX-License-Identifier: BSD-3-Clause
add_library(${SCP_MODULE_TARGET} SCP_MODULE)
target_sources(${SCP_MODULE_TARGET} PRIVATE src/mod_test_mhu_fce.c)
target_include_directories(${SCP_MODULE_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../si0_ramfw/include")
target_link_libraries(${SCP_MODULE_TARGET} PRIVATE
    module-integration-test module-mhu3 module-transport module-timer)
