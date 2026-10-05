add_library(${SCP_MODULE_TARGET} SCP_MODULE)
target_include_directories(${SCP_MODULE_TARGET} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_sources(${SCP_MODULE_TARGET} PRIVATE src/mod_vmcu_safety.c)
target_link_libraries(${SCP_MODULE_TARGET} PRIVATE module-timer module-pl011 module-pfdi-monitor module-pmic module-tps6594 module-si0-platform)
