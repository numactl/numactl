extern int numa_sched_setaffinity(pid_t pid, unsigned len, unsigned long *mask);
extern int numa_sched_getaffinity(pid_t pid, unsigned len, unsigned long *mask);

#define SHM_HUGETLB     04000   /* segment will use huge TLB pages */

#define BITS_PER_LONG (sizeof(unsigned long) * 8)

#define test_bit(i,p)  ((p)[(i) / sizeof(unsigned long)] &   (1UL << ((i)%BITS_PER_LONG)))
#define set_bit(i,p)   ((p)[(i) / sizeof(unsigned long)] |=  (1UL << ((i)%BITS_PER_LONG)))
#define clear_bit(i,p) ((p)[(i) / sizeof(unsigned long)] &= ~(1UL << ((i)%BITS_PER_LONG)))

#define array_len(x) (sizeof(x)/sizeof(*(x)))

#define CPU_BYTES(x) (((x) + BITS_PER_LONG - 1) / 8)
#define CPU_WORDS(x) (CPU_BYTES(x) / sizeof(long))
