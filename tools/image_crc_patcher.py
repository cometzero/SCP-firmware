#!/usr/bin/env python3
#
# Arm SCP/MCP Software
# Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import binascii
import os


def calculate_and_append_crc32(input_filepath):
    """
    Calculates the CRC32 of a binary file, appends it to the end.

    Args:
        input_filepath (str): The path to the input binary file.
    """
    try:
        with open(input_filepath, 'rb') as f_in:
            file_content = f_in.read()

        # Exclude the last 4 bytes for CRC calculation since the
        # last 4 bytes hold the CRC itself
        file_content_WO_last_4_bytes = file_content[:-4]

        # Calculate CRC32
        crc32_value = binascii.crc32(file_content_WO_last_4_bytes) & 0xFFFFFFFF

        # Convert CRC32 to bytes (little-endian for common usage)
        crc32_bytes = crc32_value.to_bytes(4, byteorder='little')

        # Append CRC32 to the file content
        new_file_content = file_content_WO_last_4_bytes + crc32_bytes

        # Write the new content to the output file
        with open(input_filepath, 'wb') as f_out:
            f_out.write(new_file_content)

        print(f"CRC32 calculated: 0x{crc32_value:08X}")
        print(f"CRC updated file: {input_filepath}")

    except FileNotFoundError:
        print(f"Error: Input file not found at '{input_filepath}'")
    except Exception as e:
        print(f"An error occurred: {e}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 crc_appender.py <input_binary_path>")
        sys.exit(1)

    input_path = sys.argv[1]

    calculate_and_append_crc32(input_path)
