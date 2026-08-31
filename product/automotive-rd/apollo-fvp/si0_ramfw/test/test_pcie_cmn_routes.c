/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEST_PCIE_CMN_ROUTES_MODULE_IDX
#    define TEST_PCIE_CMN_ROUTES_MODULE_IDX
#    define FWK_TEST_MODULE_IDX_H "test_pcie_cmn_routes.c"

#    include "../config_cmn_cyprus.c"

#    include <internal/cmn_cyprus_rnsam_reg.h>

#    include <inttypes.h>
#    include <stdbool.h>
#    include <stdio.h>

#    define EXPECTED_REGION_COUNT 11U
#    define BASELINE_REGION_COUNT 8U

static const struct mod_cmn_cyprus_mem_region_map baseline_mmap[] = {
    { .base = UINT64_C(0x000000000000),
      .size = UINT64_C(256) * FWK_TIB,
      .type = MOD_CMN_CYPRUS_MEM_REGION_TYPE_SYSCACHE },
    { .base = UINT64_C(0x000000000000),
      .size = UINT64_C(128) * FWK_MIB,
      .type = MOD_CMN_CYPRUS_MEM_REGION_TYPE_SYSCACHE_SUB,
      .node_id = NODE_ID_SBSX0 },
    { .base = UINT64_C(0x0010000000),
      .size = UINT64_C(768) * FWK_MIB,
      .type = MOD_CMN_CYPRUS_MEM_REGION_TYPE_IO,
      .node_id = NODE_ID_HND },
    { .base = UINT64_C(0x0040000000),
      .size = UINT64_C(256) * FWK_MIB,
      .type = MOD_CMN_CYPRUS_MEM_REGION_TYPE_IO,
      .node_id = NODE_ID_HNI1 },
    { .base = UINT64_C(0x0100000000),
      .size = UINT64_C(1) * FWK_GIB,
      .type = MOD_CMN_CYPRUS_MEM_REGION_TYPE_IO,
      .node_id = NODE_ID_HND },
    { .base = UINT64_C(0x140000000),
      .size = UINT64_C(1) * FWK_GIB,
      .type = MOD_CMN_CYPRUS_MEM_REGION_TYPE_IO,
      .node_id = NODE_ID_HND },
    { .base = UINT64_C(0x180000000),
      .size = UINT64_C(1) * FWK_GIB,
      .type = MOD_CMN_CYPRUS_MEM_REGION_TYPE_IO,
      .node_id = NODE_ID_HND },
    { .base = UINT64_C(0x01C0000000),
      .size = UINT64_C(128) * FWK_MIB,
      .type = MOD_CMN_CYPRUS_MEM_REGION_TYPE_IO,
      .node_id = NODE_ID_HNP1 },
};

static const struct mod_cmn_cyprus_mem_region_map pcie_mmap[] = {
    { .base = UINT64_C(0x10040000000),
      .size = UINT64_C(256) * FWK_MIB,
      .type = MOD_CMN_CYPRUS_MEM_REGION_TYPE_IO,
      .node_id = NODE_ID_HNP1 },
    { .base = UINT64_C(0x60000000),
      .size = UINT64_C(512) * FWK_MIB,
      .type = MOD_CMN_CYPRUS_MEM_REGION_TYPE_IO,
      .node_id = NODE_ID_HNP1 },
    { .base = UINT64_C(0x10160000000),
      .size = UINT64_C(512) * FWK_MIB,
      .type = MOD_CMN_CYPRUS_MEM_REGION_TYPE_IO,
      .node_id = NODE_ID_HNP1 },
};

static bool regions_overlap(
    const struct mod_cmn_cyprus_mem_region_map *left,
    const struct mod_cmn_cyprus_mem_region_map *right)
{
    return (left->base < (right->base + right->size)) &&
        (right->base < (left->base + left->size));
}

static int require(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }

    return 0;
}

int main(void)
{
    unsigned int non_hashed_count = 0;
    unsigned int pcie_match_count = 0;
    size_t idx;
    size_t other;
    int failures = 0;

    failures += require(
        FWK_ARRAY_SIZE(mmap) == EXPECTED_REGION_COUNT,
        "Apollo-FVP CMN route count must be 11");

    for (idx = 0; idx < BASELINE_REGION_COUNT; ++idx) {
        failures += require(
            mmap[idx].base == baseline_mmap[idx].base &&
                mmap[idx].size == baseline_mmap[idx].size &&
                mmap[idx].type == baseline_mmap[idx].type &&
                mmap[idx].node_id == baseline_mmap[idx].node_id,
            "existing Apollo-FVP CMN route changed");
    }

    for (idx = 0; idx < FWK_ARRAY_SIZE(mmap); ++idx) {
        if (mmap[idx].type != MOD_CMN_CYPRUS_MEM_REGION_TYPE_SYSCACHE) {
            non_hashed_count++;
        }

        for (other = 0; other < FWK_ARRAY_SIZE(pcie_mmap); ++other) {
            if (mmap[idx].base == pcie_mmap[other].base &&
                mmap[idx].size == pcie_mmap[other].size &&
                mmap[idx].type == pcie_mmap[other].type &&
                mmap[idx].node_id == pcie_mmap[other].node_id) {
                pcie_match_count++;
                failures += require(
                    (mmap[idx].base % mmap[idx].size) == 0,
                    "PCIe CMN route is not naturally aligned");
                printf(
                    "PCIE_CMN_ROUTE base=0x%" PRIx64 " size=0x%" PRIx64
                    " type=%u node=%u\n",
                    mmap[idx].base,
                    mmap[idx].size,
                    mmap[idx].type,
                    mmap[idx].node_id);
            }
        }
    }

    failures += require(
        pcie_match_count == FWK_ARRAY_SIZE(pcie_mmap),
        "not all three PCIe CMN routes were found");
    failures += require(
        non_hashed_count <= RNSAM_NON_HASH_MEM_REGION_COUNT,
        "Apollo-FVP CMN routes exceed RN-SAM non-hashed capacity");

    for (idx = 0; idx < FWK_ARRAY_SIZE(mmap); ++idx) {
        if (mmap[idx].type != MOD_CMN_CYPRUS_MEM_REGION_TYPE_IO) {
            continue;
        }
        for (other = idx + 1; other < FWK_ARRAY_SIZE(mmap); ++other) {
            if (mmap[other].type == MOD_CMN_CYPRUS_MEM_REGION_TYPE_IO) {
                failures += require(
                    !regions_overlap(&mmap[idx], &mmap[other]),
                    "Apollo-FVP CMN I/O routes overlap");
            }
        }
    }

    if (failures == 0) {
        printf(
            "PASS: routes=%zu pcie_routes=%u non_hashed=%u capacity=%u\n",
            FWK_ARRAY_SIZE(mmap),
            pcie_match_count,
            non_hashed_count,
            RNSAM_NON_HASH_MEM_REGION_COUNT);
    }

    return failures == 0 ? 0 : 1;
}

#else

enum fwk_module_idx {
    FWK_MODULE_IDX_CMN_CYPRUS,
    FWK_MODULE_IDX_TIMER,
    FWK_MODULE_IDX_COUNT,
};

#endif
