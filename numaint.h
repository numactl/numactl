#include "bitops.h"

extern int numa_sched_setaffinity(pid_t pid, unsigned len, unsigned long *mask);
extern int numa_sched_getaffinity(pid_t pid, unsigned len, unsigned long *mask);
extern int numa_sched_setaffinity_int(pid_t pid, unsigned len, unsigned long *mask);
extern int numa_sched_getaffinity_int(pid_t pid, unsigned len, unsigned long *mask);
extern long get_mempolicy_int(int *policy, unsigned long *nmask, unsigned long maxnode, 
                                                    void *addr, int flags);
extern long mbind_int(void *start, unsigned long len, int mode, 
		  unsigned long *nmask, unsigned long maxnode, unsigned flags);
extern long set_mempolicy_int(int mode, unsigned long *nmask, 
			  unsigned long maxnode);
                                                    
#define SHM_HUGETLB     04000   /* segment will use huge TLB pages */

#define CPU_BYTES(x) round_up(x, BITS_PER_LONG)
#define CPU_WORDS(x) (CPU_BYTES(x) / sizeof(long))

#define make_internal_alias(x) extern __typeof (x) x##_int __attribute((alias(#x), visibility("hidden")))
