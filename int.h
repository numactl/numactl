extern int numa_sched_setaffinity_int(pid_t pid, unsigned len, unsigned long *mask);
extern int numa_sched_getaffinity_int(pid_t pid, unsigned len, unsigned long *mask);
extern long get_mempolicy_int(int *policy, unsigned long *nmask, unsigned long maxnode,
