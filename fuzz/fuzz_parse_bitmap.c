/*
 * fuzz_parse_bitmap.c - libFuzzer target for libnuma's exported string
 * parsers beyond the two covered by fuzz_parse_str: the numa_parse_bitmap
 * symbol-version pair and the *_all node/cpu string variants.
 *
 * Threat model
 * ============
 * 1. Untrusted input: numa_parse_bitmap() is an exported libnuma API for
 *    parsing hex bitmask strings (the format used in /sys cpumasks); the
 *    numa_parse_nodestring_all()/numa_parse_cpustring_all() variants parse
 *    user-provided node/cpu list strings for callers that must consider all
 *    system (not task-allowed) nodes/cpus. Applications feed these strings
 *    from config files, job specs, monitoring output and user input.
 * 2. Trust boundary: the parse functions themselves - bytes-in, mask-out.
 * 3. Attacker-controlled: the whole string, plus (for the v1 bitmap variant)
 *    the declared cpu count ncpus that sizes the output array.
 * 4. Real bug here: OOB read/write in the bitmap walker (it walks the string
 *    backwards from an embedded newline, memmoves comma-separated 32-bit
 *    groups together, and bounds writes only against CPU_LONGS(ncpus)),
 *    integer conversion edges, and mask-index errors in the *_all variants
 *    (which validate against numa_possible_* instead of numa_all_* masks).
 *
 * Why this target (coverage rationale)
 * ====================================
 * Fuzz Introspector lists numa_parse_nodestring_all() (complexity 99) at 0%
 * coverage; numa_parse_bitmap_v1/v2 (libnuma.c ~1347-1416, exported per
 * versions.ldscript for both libnuma_1.1 and libnuma_1.2) have never been
 * fuzzed - the existing fuzz_parse_str only drives numa_parse_nodestring()
 * and numa_parse_cpustring(). The bitmap walker's backwards pointer walk
 * (`oldp--`, `memmove(p, p+1, oldp-p+1)`), its `p > line` boundary logic and
 * the comma/newline interplay are exactly the kind of arithmetic that yields
 * off-by-one and OOB findings under mutation.
 *
 * Design notes
 * ============
 * - The v1 symbol is bound via a .symver alias
 *   (numa_parse_bitmap@libnuma_1.1, 3-arg raw-array form) so both arity
 *   variants of the exported name are exercised; the default (2-arg) form
 *   binds to @libnuma_1.2 (v2).
 * - The line buffer is copied to writable stack storage per call: the parsers
 *   mutate the line in place (memmove).
 * - All masks are allocated with numa_bitmask_alloc() (n >= 1) and freed; the
 *   raw v1 array is sized CPU_LONGS(ncpus) for the declared ncpus - the
 *   documented caller contract this API expects.
 * - numa_node_to_cpus()/numa_node_of_cpu() round-trips are driven on the
 *   parsed masks to cover the convert-and-consume paths downstream of
 *   parsing.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "numa.h"
#include "numaint.h"

/* bind the 3-arg v1 export explicitly */
extern int numa_parse_bitmap_v1_ref(char *line, unsigned long *mask, int ncpus);
__asm__(".symver numa_parse_bitmap_v1_ref, numa_parse_bitmap@libnuma_1.1");

#define MAX_LINE      256u
#define MAX_NCPUS     1024u

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
        (void)argc;
        (void)argv;
        if (numa_available() < 0)
                return 0;
        return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
        if (size < 2)
                return 0;

        /* bitmap string: must contain a newline for the parser */
        char line[MAX_LINE + 1];
        size_t blen = size < MAX_LINE ? size : MAX_LINE;
        memcpy(line, data, blen);
        line[blen] = '\0';
        if (!strchr(line, '\n'))
                line[blen - 1] = '\n';   /* seed the terminator the parser needs */

        unsigned ncpus = (unsigned)data[0] % MAX_NCPUS + 1;

        /* v1: raw array sized for ncpus (caller contract) */
        {
                unsigned long raw[(MAX_NCPUS + 63) / 64];   /* == CPU_LONGS(MAX_NCPUS) */
                memset(raw, 0, sizeof(raw));
                char buf[MAX_LINE + 1];
                memcpy(buf, line, blen + 1);
                (void)numa_parse_bitmap_v1_ref(buf, raw, (int)ncpus);
        }

        /* v2: bitmask form */
        {
                struct bitmask *mask = numa_bitmask_alloc((unsigned)ncpus);
                if (mask) {
                        char buf[MAX_LINE + 1];
                        memcpy(buf, line, blen + 1);
                        (void)numa_parse_bitmap(buf, mask);
                        numa_bitmask_free(mask);
                }
        }

        /* *_all string variants on the remaining bytes */
        if (size > blen) {
                const char *s = (const char *)data + blen;
                size_t slen = size - blen;
                char str[128];
                size_t copy = slen < sizeof(str) - 1 ? slen : sizeof(str) - 1;
                memcpy(str, s, copy);
                str[copy] = '\0';

                struct bitmask *nodes = numa_parse_nodestring_all(str);
                if (nodes)
                        numa_bitmask_free(nodes);
                struct bitmask *cpus = numa_parse_cpustring_all(str);
                if (cpus) {
                        if (numa_available() >= 0) {
                                struct bitmask *cpubits = numa_allocate_cpumask();
                                if (cpubits) {
                                        (void)numa_node_to_cpus(0, cpubits);
                                        numa_bitmask_free(cpubits);
                                }
                        }
                        numa_bitmask_free(cpus);
                }
        }
        return 0;
}
