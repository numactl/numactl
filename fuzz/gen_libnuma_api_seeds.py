#!/usr/bin/env python3
"""Generate the seed corpus for fuzz_libnuma_api.

Deterministic op-streams that cover every harness op family (bitmask API,
nodemask compat, policy set/get, bind/run, sched affinity, alloc family,
page placement, migration, topology queries, home node) so the fuzzer starts
inside every switch arm instead of discovering the op grammar from scratch.

Usage: gen_libnuma_api_seeds.py <output zip>
"""
import itertools
import struct
import sys
import zipfile


def ops(*pairs):
    """ops((op, a0, a1), ...) -> packed op stream bytes"""
    out = bytearray()
    for op, a0, a1 in pairs:
        out += struct.pack("<BBH", op, a0, a1)
    return bytes(out)


SEEDS = {
    # bitmask component API: tiny/large masks, set/clear/isbitset/weight/nbytes
    "bitmask_basic": ops((0, 1, 0), (0, 63, 63), (0, 255, 1), (0, 200, 0)),
    # bitmask slots: equality and copy_bitmask_to_bitmask across sizes
    "bitmask_slots": ops((1, 0, 1), (1, 1, 0), (1, 2, 2048)),
    # nodemask compat inline surface (nodemask_zero/set/clr/isset/equal/copies)
    "nodemask_compat": ops((2, 0, 0), (2, 1, 0), (2, 127, 0), (2, 64, 64)),
    # policy set: membind/interleave/localalloc/preferred_many/weighted
    "policy_set": ops((3, 0, 0), (3, 1, 1), (3, 0, 2), (3, 0, 3), (3, 1, 4)),
    # policy get: membind/interleave/mems_allowed/weighted/compat getters
    "policy_get": ops((4, 0, 0), (4, 1, 0), (4, 2, 0), (4, 3, 0), (4, 4, 0), (4, 5, 0)),
    # bind + run_on_node_mask(_all) + run_on_node + get_run_node_mask
    "bind_run": ops((5, 0, 1), (5, 0, 0), (5, 0, 3), (5, 1, 2)),
    # sched affinity compat (len 1..16 longs) and v2 bitmask affinity
    "sched_affinity": ops((6, 0, 0), (6, 15, 1), (6, 31, 63), (6, 7, 5)),
    # alloc family: plain/onnode/interleaved/subset/local/weighted + realloc
    "alloc_family": ops((7, 0, 0), (7, 1, 0), (7, 2, 1), (7, 3, 0), (7, 5, 0),
                        (7, 6, 1), (7, 7, 0), (7, 3, 1)),
    # page placement: tonode/tonodemask/interleave/weighted/police/setlocal
    "page_place": ops((8, 0, 0), (8, 1, 1), (8, 0, 2), (8, 0, 3), (8, 0, 4),
                      (11, 0, 0), (11, 1, 3)),
    # migration: numa_migrate_pages + numa_move_pages with/without MPOL_MF_MOVE
    "migrate": ops((9, 0, 0), (9, 1, 1), (9, 2, 0), (9, 3, 1)),
    # topology: node_to_cpus v2+compat, is_cpuless, node_of_cpu, sizes
    "topology": ops((10, 0, 0), (10, 0, 1), (10, 1, 1023), (10, 0, 63)),
    # home node API (guarded by numa_has_home_node at runtime)
    "home_node": ops((11, 0, 2), (11, 1, 5), (11, 0, 1)),
    # strictness option matrix interleaved with policy ops
    "strict_matrix": ops((6, 1, 0), (3, 0, 0), (7, 0, 0), (6, 1, 7), (7, 3, 0),
                         (20, 0, 0), (13, 1, 1)),
    # long random-ish stream: cross-family statefulness
    "mixed_stream": ops((3, 0, 1), (1, 0, 0), (7, 3, 1), (8, 0, 1), (9, 1, 1),
                        (10, 0, 0), (5, 0, 1), (4, 2, 0), (2, 5, 0), (0, 100, 10),
                        (6, 3, 2), (11, 0, 4)),
    # seed that exercises every case selector once (op%12 == 0..11)
    "all_cases": ops(*[(op, 1, 1) for op in range(12)]),
    # empty-ish stream (must be a no-op)
    "empty": b"",
}


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    with zipfile.ZipFile(sys.argv[1], "w") as zf:
        for name, blob in SEEDS.items():
            zf.writestr(name, blob)
    print("wrote %d seeds to %s" % (len(SEEDS), sys.argv[1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
