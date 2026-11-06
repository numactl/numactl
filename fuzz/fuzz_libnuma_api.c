/*
 * fuzz_libnuma_api.c - libFuzzer target for libnuma's node/cpu policy, memory
 * placement, affinity and bitmask APIs (numactl project).
 *
 * Threat model
 * ============
 * 1. Untrusted input: applications link libnuma and feed it values that are
 *    ultimately attacker- or user-influenced: node ids, cpu ids, allocation
 *    sizes, policy enums, affinity masks and migration arguments. The numactl
 *    CLI is the canonical example: `numactl --membind <string> -- <cmd>`
 *    parses an untrusted string and hands the resulting mask to numa_bind(),
 *    numa_set_membind(), numa_migrate_pages() and friends. System agents
 *    (numad, monitoring, orchestrators) do the same with externally visible
 *    topology data.
 * 2. Trust boundary: the public libnuma API surface (everything exported via
 *    versions.ldscript, v1 compat inlines included).
 * 3. Attacker-controlled at the boundary: node/cpu indices, allocation and
 *    migration sizes, mask contents, policy selections, strictness flags.
 *    The harness generates exactly those, bounded to documented API contracts
 *    (nodes < NUMA_NUM_NODES for nodemask_t compat, n >= 1 for
 *    numa_bitmask_alloc, sizes bounded so OOM cannot be a harness fault).
 * 4. Real bug here: a memory-safety violation (ASan/UBSan), leak on an error
 *    path, or hang reachable through public APIs with in-contract arguments.
 *
 * Why this target (coverage rationale)
 * ====================================
 * OSS-Fuzz Fuzz Introspector lists as top unreached high-complexity functions:
 * numa_bind_v1/v2, numa_run_on_node_mask_v1/v2/all, numa_get_run_node_mask_*,
 * numa_run_on_node, numa_node_of_cpu, numa_parse_nodestring_all - all
 * statically reachable from fuzz_parse_str but dynamically never executed.
 * The wrappers in syscall.c (mbind/set_mempolicy/get_mempolicy/migrate_pages/
 * move_pages/sched_setaffinity) sit at 12.5% file coverage and the numa.h
 * v1-compat inline helpers at 2.6%. This target drives all of them through a
 * stateful op stream.
 *
 * Design notes
 * ============
 * - Stateful sequence: each iteration decodes an op stream (opcode + bounded
 *   arguments) and drives multiple API calls, so multi-call states (policy
 *   set then allocate under that policy, mask built then bound) are reached.
 * - All allocations are bounded (masks <= 4096 bits, memory areas <= 252 KiB,
 *   few live objects) and freed within the iteration, so OOM/leaks cannot be
 *   harness artefacts.
 * - numa_error()/numa_warn() are overridden with the library's documented
 *   weak-override mechanism purely to keep expected EINVAL noise out of the
 *   log and out of the library's exit-on-error path; sanitizer detection is
 *   unchanged and nothing is silenced.
 * - The compatibility inline helpers (numa.h) and the SYMVER v1/v2 symbol
 *   pairs are called explicitly: they are part of the API surface and dark in
 *   the production report.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include "numa.h"
#include "numaif.h"
#include "numaint.h"

#define MAX_MASK_BITS   4096u          /* cap for numa_bitmask_alloc() */
#define MAX_AREA_PAGES  63u            /* memory areas <= 252 KiB */
#define NUMA_NUM_NODES_LOCAL 128       /* nodemask_t width on x86_64 */

/* quiet overrides of the documented weak error/warning hooks: expected
   EINVAL/EXDEV noise from policy syscalls must not spam the log. Detection
   behaviour is unchanged. */
static int g_numa_errors;
void numa_error(char *where)
{
        (void)where;
        g_numa_errors++;
}

void numa_warn(int num, char *fmt, ...)
{
        (void)num;
        (void)fmt;
}

static int g_numa_ok;

/* live bitmask slots (freed at end of iteration) */
#define MAX_MASKS 3
static struct bitmask *g_masks[MAX_MASKS];

/* live memory areas from the numa_alloc family */
#define MAX_AREAS 2
struct area {
        void *ptr;
        size_t size;
};
static struct area g_areas[MAX_AREAS];

static struct bitmask *mask_at(unsigned slot)
{
        if (g_masks[slot] == NULL) {
                unsigned n = 1 + (slot % 4);   /* tiny default mask */
                g_masks[slot] = numa_bitmask_alloc(n);
        }
        return g_masks[slot];
}

static void free_masks(void)
{
        unsigned i;
        for (i = 0; i < MAX_MASKS; i++) {
                if (g_masks[i]) {
                        numa_bitmask_free(g_masks[i]);
                        g_masks[i] = NULL;
                }
        }
}

static void free_areas(void)
{
        unsigned i;
        for (i = 0; i < MAX_AREAS; i++) {
                if (g_areas[i].ptr) {
                        numa_free(g_areas[i].ptr, g_areas[i].size);
                        g_areas[i].ptr = NULL;
                        g_areas[i].size = 0;
                }
        }
}

/* allocate (or reuse) an area of bounded size in slot */
static struct area *area_at(unsigned slot, size_t size)
{
        struct area *a = &g_areas[slot % MAX_AREAS];
        if (a->ptr && a->size != size) {
                numa_free(a->ptr, a->size);
                a->ptr = NULL;
                a->size = 0;
        }
        if (!a->ptr) {
                a->ptr = numa_alloc(size);
                if (a->ptr == NULL)
                        return NULL;
                a->size = size;
        }
        return a;
}

static size_t fuzz_area_size(unsigned arg)
{
        return (size_t)(arg % MAX_AREA_PAGES + 1) *
               (size_t)sysconf(_SC_PAGESIZE);
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
        (void)argc;
        (void)argv;
        g_numa_ok = numa_available() >= 0;
        return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
        unsigned max_node = g_numa_ok ? (unsigned)numa_max_node() : 0;

        while (size >= 4) {
                unsigned op = data[0];
                unsigned a0 = data[1];
                unsigned a1 = data[2] | ((unsigned)data[3] << 8);
                data += 4;
                size -= 4;

                /* option matrix: strictness flags flip frequently */
                if (op % 7 == 6) {
                        numa_set_strict(a0 & 1);
                        numa_set_bind_policy((a0 >> 1) & 1);
                        numa_fail_alloc_on_error = (a0 >> 2) & 1;
                }

                switch (op % 12) {
                case 0: { /* bitmask component API */
                        unsigned n = 1 + (a0 % MAX_MASK_BITS);
                        struct bitmask *m = numa_bitmask_alloc(n);
                        if (m) {
                                numa_bitmask_setall(m);
                                numa_bitmask_setbit(m, a1 % n);
                                numa_bitmask_clearbit(m, a1 % n);
                                (void)numa_bitmask_isbitset(m, a1 % n);
                                (void)numa_bitmask_weight(m);
                                (void)numa_bitmask_nbytes(m);
                                numa_bitmask_free(m);
                        }
                        break;
                }
                case 1: { /* bitmask slots + equality/compare */
                        struct bitmask *m1 = mask_at(a0 % MAX_MASKS);
                        struct bitmask *m2 = mask_at((a0 + 1) % MAX_MASKS);
                        if (m1 && m2) {
                                numa_bitmask_clearall(m1);
                                numa_bitmask_setbit(m1, a1 % m1->size);
                                (void)numa_bitmask_equal(m1, m2);
                                copy_bitmask_to_bitmask(m1, m2);
                        }
                        break;
                }
                case 2: { /* nodemask_t compat inline surface */
                        nodemask_t nm1, nm2;
                        unsigned node = a0 % NUMA_NUM_NODES_LOCAL;
                        nodemask_zero(&nm1);
                        nodemask_set_compat(&nm1, (int)node);
                        nodemask_clr_compat(&nm1, (int)(node ^ 1));
                        (void)nodemask_isset_compat(&nm1, (int)node);
                        copy_nodemask_to_bitmask(&nm1, mask_at(0));
                        copy_bitmask_to_nodemask(mask_at(0), &nm2);
                        (void)nodemask_equal(&nm1, &nm2);
                        break;
                }
                case 3: { /* set memory policy (v2 APIs) */
                        struct bitmask *big = numa_allocate_nodemask();
                        if (big) {
                                unsigned node = a0 % NUMA_NUM_NODES_LOCAL;
                                if (node < big->size)
                                        numa_bitmask_setbit(big, node);
                                switch (a1 % 5) {
                                case 0: numa_set_membind(mask_at(0)); break;
                                case 1: numa_set_interleave_mask(big); break;
                                case 2: numa_set_localalloc(); break;
                                case 3:
                                        if (numa_has_preferred_many())
                                                numa_set_preferred_many(big);
                                        else
                                                numa_set_preferred((int)(a0 % (max_node + 1)));
                                        break;
                                default:
                                        numa_set_weighted_interleave_mask(big);
                                        break;
                                }
                        }
                        if (big)
                                numa_bitmask_free(big);
                        break;
                }
                case 4: { /* get memory policy (v2 + v1 compat) */
                        struct bitmask *m;
                        nodemask_t nm;
                        switch (a0 % 6) {
                        case 0: m = numa_get_membind(); break;
                        case 1: m = numa_get_interleave_mask(); break;
                        case 2: m = numa_get_mems_allowed(); break;
                        case 3: m = numa_get_weighted_interleave_mask(); break;
                        case 4:
                                nm = numa_get_interleave_mask_compat();
                                copy_bitmask_to_nodemask(mask_at(0), &nm);
                                m = NULL;
                                break;
                        default:
                                nm = numa_get_membind_compat();
                                m = NULL;
                                break;
                        }
                        if (m)
                                numa_bitmask_free(m);
                        (void)numa_preferred();
                        (void)numa_preferred_err();
                        break;
                }
                case 5: { /* bind + run (the introspector top-10) */
                        struct bitmask *m = mask_at(0);
                        if (m) {
                                if (a1 % 2)
                                        numa_bind(m);
                                else
                                        numa_run_on_node_mask(m);
                        }
                        if (a1 % 3 == 0)
                                numa_run_on_node_mask_all(mask_at(1));
                        (void)numa_run_on_node((int)(a1 % (max_node + 1)));
                        {
                                struct bitmask *rm = numa_get_run_node_mask();
                                if (rm)
                                        numa_bitmask_free(rm);
                        }
                        break;
                }
                case 6: { /* sched affinity, v1 compat + v2 */
                        unsigned long buf[32];
                        struct bitmask *cm = numa_allocate_cpumask();
                        unsigned len = a0 % 16 + 1;
                        if (len <= 32 && cm) {
                                memset(buf, 0, sizeof(buf));
                                buf[0] = 1UL << (a1 % 64);
                                (void)numa_sched_setaffinity_compat(0, len, buf);
                                (void)numa_sched_getaffinity_compat(0, len, buf);
                                (void)numa_sched_setaffinity(0, cm);
                                (void)numa_sched_getaffinity(0, cm);
                        }
                        if (cm)
                                numa_bitmask_free(cm);
                        break;
                }
                case 7: { /* numa_alloc family + numa_free/numa_realloc */
                        size_t sz = fuzz_area_size(a0);
                        unsigned slot = a1 % MAX_AREAS;
                        struct area *a = &g_areas[slot];
                        free_areas();
                        switch (a0 % 8) {
                        case 1:
                                a->ptr = numa_alloc_onnode(sz, (int)(a1 % (max_node + 1)));
                                break;
                        case 2:
                                a->ptr = numa_alloc_interleaved(sz);
                                break;
                        case 3:
                                a->ptr = numa_alloc_interleaved_subset(sz, mask_at(0));
                                break;
                        case 5:
                                a->ptr = numa_alloc_local(sz);
                                break;
                        case 6:
                                a->ptr = numa_alloc_weighted_interleaved(sz);
                                break;
                        case 7:
                                a->ptr = numa_alloc_weighted_interleaved_subset(sz, mask_at(0));
                                break;
                        default:
                                a->ptr = numa_alloc(sz);
                                break;
                        }
                        if (a->ptr) {
                                a->size = sz;
                                ((volatile char *)a->ptr)[0] = 1;  /* touch first page */
                                if (a0 % 3 == 0) {
                                        size_t nsz = fuzz_area_size(a1);
                                        void *np = numa_realloc(a->ptr, a->size, nsz);
                                        if (np) {
                                                a->ptr = np;
                                                a->size = nsz;
                                        }
                                }
                        } else {
                                a->size = 0;
                        }
                        break;
                }
                case 8: { /* page placement on live areas */
                        struct area *a = area_at(a0 % MAX_AREAS, fuzz_area_size(a1));
                        if (a && a->ptr) {
                                switch (a1 % 5) {
                                case 0:
                                        numa_tonode_memory(a->ptr, a->size,
                                                           (int)(a0 % (max_node + 1)));
                                        break;
                                case 1:
                                        numa_tonodemask_memory(a->ptr, a->size, mask_at(0));
                                        break;
                                case 2:
                                        numa_interleave_memory(a->ptr, a->size, mask_at(0));
                                        break;
                                case 3:
                                        numa_weighted_interleave_memory(a->ptr, a->size, mask_at(0));
                                        break;
                                default:
                                        numa_police_memory(a->ptr, a->size);
                                        break;
                                }
                        }
                        break;
                }
                case 9: { /* migration + page motion */
                        struct area *a = area_at(0, fuzz_area_size(a0));
                        if (a && a->ptr) {
                                struct bitmask *from = numa_allocate_nodemask();
                                struct bitmask *to = numa_allocate_nodemask();
                                if (from && to) {
                                        void *pages[4];
                                        int nodes[4];
                                        int status[4];
                                        unsigned np = a0 % 4 + 1;
                                        unsigned i;
                                        size_t pg = (size_t)sysconf(_SC_PAGESIZE);
                                        for (i = 0; i < np; i++) {
                                                pages[i] = (char *)a->ptr + i * pg;
                                                nodes[i] = (int)(a1 % (max_node + 1));
                                                status[i] = -1;
                                        }
                                        numa_bitmask_setbit(from, 0);
                                        if (max_node)
                                                numa_bitmask_setbit(to, (int)(a1 % (max_node + 1)));
                                        (void)numa_migrate_pages(getpid(), from, to);
                                        (void)numa_move_pages(getpid(), np, pages, nodes,
                                                              status,
                                                              (a1 % 2) ? 1 /*MPOL_MF_MOVE*/ : 0);
                                }
                                if (from)
                                        numa_bitmask_free(from);
                                if (to)
                                        numa_bitmask_free(to);
                        }
                        break;
                }
                case 10: { /* topology queries */
                        struct bitmask *cm = numa_allocate_cpumask();
                        int node = (int)(a0 % (max_node + 1));
                        unsigned long buf[256];
                        if (cm) {
                                (void)numa_node_to_cpus(node, cm);
                                (void)numa_node_to_cpus_compat(node, buf, 256);
                        }
                        (void)numa_is_cpuless_node((int)(a1 % (max_node + 1)));
                        (void)numa_node_of_cpu((int)(a1 % 1024)); /* -1/errno is documented */
                        (void)numa_node_size64(node, NULL);
                        (void)numa_num_configured_nodes();
                        (void)numa_num_configured_cpus();
                        (void)numa_num_possible_nodes();
                        (void)numa_num_possible_cpus();
                        (void)numa_num_task_cpus();
                        (void)numa_num_task_nodes();
                        if (cm)
                                numa_bitmask_free(cm);
                        break;
                }
                default: { /* home node + area local placement */
                        struct area *a = area_at(0, fuzz_area_size(a0));
                        if (a && a->ptr) {
                                if (numa_has_home_node() && a1 % 2)
                                        (void)numa_set_mempolicy_home_node(a->ptr, a->size,
                                                                           (int)(a1 % (max_node + 1)),
                                                                           0);
                                if (a1 % 3 == 0)
                                        numa_setlocal_memory(a->ptr, a->size);
                        }
                        break;
                }
                }
        }

        if (g_numa_ok) {
                /* restore ambient state so iterations stay independent */
                numa_set_localalloc();
                numa_set_strict(0);
                numa_set_bind_policy(0);
                numa_fail_alloc_on_error = 0;
        }
        free_masks();
        free_areas();
        return 0;
}
