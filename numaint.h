/* Internal interfaces of libnuma */
#include "bitops.h"

extern int numa_sched_setaffinity_v1(pid_t pid, unsigned len, const unsigned long *mask);
extern int numa_sched_getaffinity_v1(pid_t pid, unsigned len, const unsigned long *mask);
extern int numa_sched_setaffinity_v1_int(pid_t pid, unsigned len,const unsigned long *mask);
extern int numa_sched_getaffinity_v1_int(pid_t pid, unsigned len,const unsigned long *mask);
extern int numa_sched_setaffinity_v2(pid_t pid, struct bitmask *mask);
extern int numa_sched_getaffinity_v2(pid_t pid, struct bitmask *mask);
extern int numa_sched_setaffinity_v2_int(pid_t pid, struct bitmask *mask);
extern int numa_sched_getaffinity_v2_int(pid_t pid, struct bitmask *mask);
extern long get_mempolicy(int *policy, const unsigned long *nmask,
                              unsigned long maxnode, void *addr, int flags);
extern long mbind(void *start, unsigned long len, int mode,
	  const unsigned long *nmask, unsigned long maxnode, unsigned flags);
extern long set_mempolicy(int mode, const unsigned long *nmask,
			  unsigned long maxnode);
extern long migrate_pages(int pid, unsigned long maxnode, const unsigned long *frommask,
	const unsigned long *tomask);

extern long move_pages(int pid, unsigned long count,
	void **pages, const int *nodes, int *status, int flags);

#define SHM_HUGETLB     04000   /* segment will use huge TLB pages */

#define CPU_BYTES(x) (round_up(x, BITS_PER_LONG)/8)
#define CPU_LONGS(x) (CPU_BYTES(x) / sizeof(long))

#define make_internal_alias(x) extern __typeof (x) x##_int __attribute((alias(#x), visibility("hidden")))

enum numa_warn { 
	W_nosysfs,
	W_noproc,
	W_badmeminfo,
	W_nosysfs2,
	W_cpumap,
	W_numcpus,
	W_noderunmask,
	W_distance,
	W_memory,
}; 
