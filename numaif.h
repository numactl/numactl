#ifndef NUMAIF_H
#define NUMAIF_H 1

/* Kernel interface for NUMA library */
#include <sys/types.h>

/* System calls */
extern long get_mempolicy(int *policy, 
			  unsigned long *nmask, unsigned long maxnode,
			  void *addr, int flags);
extern long mbind(void *start, unsigned long len, int mode, 
		  unsigned long *nmask, unsigned long maxnode, unsigned flags);
extern long set_mempolicy(int mode, unsigned long *nmask, unsigned long maxnode);

/* Policies */
#define MPOL_DEFAULT     0
#define MPOL_PREFERED    1
#define MPOL_BIND        2
#define MPOL_INTERLEAVE  3

#define MPOL_MAX MPOL_INTERLEAVE

/* Flags for get_mem_policy */
#define MPOL_F_ILNODE   (1<<0)  /* return next IL mode instead of node mask */
#define MPOL_F_ADDR     (1<<1)  /* look up vma using address */

#endif
