#
# Arm SCP/MCP Software
# Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include_guard(GLOBAL)

function(rdaspen_apply_platform_variant target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR
            "rdaspen_apply_platform_variant: target '${target}' does not exist.")
    endif()

    target_compile_definitions(${target} PUBLIC
        RD_ASPEN_VARIANT_FVP=0
        RD_ASPEN_VARIANT_RTL=1
    )

    if(SCP_PLATFORM_VARIANT STREQUAL "fvp")
        target_compile_definitions(${target}
            PUBLIC PLATFORM_VARIANT=RD_ASPEN_VARIANT_FVP)
    elseif(SCP_PLATFORM_VARIANT STREQUAL "rtl")
        if(SCP_RTL_VARIANT STREQUAL "fpga")
            target_compile_definitions(${target}
                PUBLIC
                    PLATFORM_VARIANT=RD_ASPEN_VARIANT_RTL
                    RD_ASPEN_RTL_VARIANT_FPGA=0
                    RD_ASPEN_RTL_VARIANT_EMU=1
                    RD_ASPEN_RTL_VARIANT=RD_ASPEN_RTL_VARIANT_FPGA)
        elseif(SCP_RTL_VARIANT STREQUAL "emu")
            target_compile_definitions(${target}
                PUBLIC
                    PLATFORM_VARIANT=RD_ASPEN_VARIANT_RTL
                    RD_ASPEN_RTL_VARIANT_FPGA=0
                    RD_ASPEN_RTL_VARIANT_EMU=1
                    RD_ASPEN_RTL_VARIANT=RD_ASPEN_RTL_VARIANT_EMU)
        else()
            message(FATAL_ERROR
                "Unknown SCP_RTL_VARIANT='${SCP_RTL_VARIANT}'.Expected 'emu' or 'fpga'.")
        endif()
    else()
        message(FATAL_ERROR
            "Unknown SCP_PLATFORM_VARIANT='${SCP_PLATFORM_VARIANT}'.Expected 'fvp' or 'rtl'.")
    endif()
endfunction()
