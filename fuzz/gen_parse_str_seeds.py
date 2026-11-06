#!/usr/bin/env python3
"""Generate the seed corpus for the existing fuzz_parse_str target.

Covers the numa_parse_nodestring / numa_parse_cpustring grammar:
absolute numbers, ranges, comma lists, 'all', inversion '!', cpuset-relative
'+', the resolve_affinity forms (netdev:/file:/ip:/block:/pci:), whitespace
and number-parsing edges (huge values, negatives, hex).

Usage: gen_parse_str_seeds.py <output zip>
"""
import sys
import zipfile

SEEDS = {
    "all": b"all",
    "single": b"0",
    "range": b"0-3",
    "list_ranges": b"1,3,5-7",
    "inverted": b"!0",
    "inverted_all": b"!all",
    "relative": b"+0-1",
    "relative_single": b"+0",
    "netdev": b"netdev:lo",
    "netdev_missing": b"netdev:nosuchif0",
    "file": b"file:/etc/hosts",
    "ip": b"ip:127.0.0.1",
    "ip_bad": b"ip:999.999.999.999",
    "block": b"block:sda",
    "pci_full": b"pci:0000:00:1f.2",
    "pci_short": b"pci:00:1f.2",
    "pci_garbage": b"pci:zz:zz",
    "huge_number": b"99999999999999999999",
    "negative": b"-1",
    "hex": b"0x10",
    "octal": b"010",
    "empty": b"",
    "space": b" ",
    "trailing_comma": b"0,",
    "double_comma": b"0,,1",
    "open_range": b"0-",
    "mixed_affinity": b"netdev:lo,0-1",
    "garbage": b"garbage",
    "newline": b"0\n1",
    "big_range": b"0-99999999999",
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
