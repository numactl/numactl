#!/usr/bin/env python3
"""Generate the seed corpus for fuzz_parse_bitmap.

Seeds cover the numa_parse_bitmap hex-bitmask grammar (comma-separated
32-bit groups, newline-terminated) and the *_all node/cpu string grammar.

Usage: gen_parse_bitmap_seeds.py <output zip>
"""
import sys
import zipfile


def seed(ncpus, bitmap, extra=b""):
    return bytes([ncpus]) + bitmap + extra


SEEDS = {
    "two_groups": seed(64, b"ff,ff\n"),
    "one_group": seed(1, b"ffffffff\n"),
    "empty_line": seed(2, b"\n"),
    "zero_groups": seed(8, b"0,0,0\n"),
    "long_mask": seed(128, b"ffffffff,ffffffff,ffffffff,ffffffff\n"),
    "trailing_comma": seed(16, b"ff,\n"),
    "leading_comma": seed(16, b",ff\n"),
    "bare_commas": seed(16, b",,,,\n"),
    "hex_garbage": seed(32, b"zz,gg\n"),
    "no_newline_seed": seed(32, b"deadbeef"),
    "newline_only": seed(255, b"\n"),
    "max_ncpus_mask": seed(255, b"1,2,3,4,5,6,7,8\n"),
    "nodes_all": seed(64, b"ff\n", b"all"),
    "nodes_range": seed(64, b"ff\n", b"0-1023"),
    "nodes_invert": seed(64, b"ff\n", b"!0"),
    "nodes_relative": seed(64, b"ff\n", b"+0-7"),
    "nodes_out_of_range": seed(64, b"ff\n", b"99999"),
    "nodes_garbage": seed(64, b"ff\n", b"abc,def"),
    "cpus_all": seed(64, b"ff\n", b"all\x001024"),
    "mixed_two_streams": seed(8, b"f0f0,0f0f\n", b"1,3,5-7,!2"),
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
