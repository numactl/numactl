/* Internal interfaces of libnuma */
#include "bitops.h"

extern int numa_sched_setaffinity(pid_t pid, unsigned len, const unsigned long *mask);
extern int numa_sched_getaffinity(pid_t pid, unsigned len, const unsigned long *mask);
extern int numa_sched_setaffinity_int(pid_t pid, unsigned len,const unsigned long *mask);
extern int numa_sched_getaffinity_int(pid_t pid, unsigned len,const unsigned long *mask);
extern long get_mempolicy_int(int *policy, const unsigned long *nmask, 
			      unsigned long maxnode, void *addr, int flags);
extern long mbind_int(void *start, unsigned long len, int mode, 
		  const unsigned long *nmask, unsigned long maxnode, unsigned flags);
extern long set_mempolicy_int(int mode, const unsigned long *nmask, 
			  unsigned long maxnode);
                                                    
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
