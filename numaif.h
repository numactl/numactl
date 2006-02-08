#ifndef NUMAIF_H
#define NUMAIF_H 1

#ifdef __cplusplus
extern "C" { 
#endif

/* Kernel interface for NUMA API */

/* System calls */
extern long get_mempolicy(int *policy, 
			  const unsigned long *nmask, unsigned long maxnode,
			  void *addr, int flags);
extern long mbind(void *start, unsigned long len, int mode, 
		  const unsigned long *nmask, unsigned long maxnode, unsigned flags);
extern long set_mempolicy(int mode, const unsigned long *nmask, 
			  unsigned long maxnode);
extern long migratepages(int pid, unsigned long maxnode, unsigned long *fromnode,
			unsigned long *tonode);

/* Policies */
#define MPOL_DEFAULT     0
#define MPOL_PREFERRED    1
#define MPOL_BIND        2
#define MPOL_INTERLEAVE  3

#define MPOL_MAX MPOL_INTERLEAVE

/* Flags for get_mem_policy */
#define MPOL_F_NODE    (1<<0)   /* return next il node or node of address */
				/* Warning: MPOL_F_NODE is unsupported and 
				   subject to change. Don't use. */
#define MPOL_F_ADDR     (1<<1)  /* look up vma using address */

/* Flags for mbind */
#define MPOL_MF_STRICT  (1<<0)  /* Verify existing pages in the mapping */
#define MPOL_MF_MOVE	(1<<1)  /* Move pages owned by this process to conform to mapping */
#define MPOL_MF_MOVE_ALL (1<<2) /* Move every page to conform to mapping */

#ifdef __cplusplus
}
#endif

#endif
