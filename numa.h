#ifndef _NUMA_H
#define _NUMA_H 1

/* Simple NUMA poliy library */

#include <stddef.h>
#include <string.h>

#if defined(__x86_64__) || defined(__i386__) 
#define NUMA_NUM_NODES 	64
#else
#define NUMA_NUM_NODES	(5*sizeof(unsigned long)*8)
#endif

typedef struct { 
	unsigned long n[NUMA_NUM_NODES/(sizeof(unsigned long)*8)];
} nodemask_t;

static inline void nodemask_zero(nodemask_t *mask)
{ 
	memset(mask->n, 0, sizeof(mask->n)); 
} 

static inline void nodemask_set(nodemask_t *mask, int node)
{
	mask->n[node / (8*sizeof(unsigned long))] |=
		(1UL<<(node%(8*sizeof(unsigned long))));		
} 

static inline void nodemask_clr(nodemask_t *mask, int node)
{
	mask->n[node / (8*sizeof(unsigned long))] &= 
		~(1UL<<(node%(8*sizeof(unsigned long))));	
}
static inline int nodemask_isset(nodemask_t *mask, int node)
{
	if (mask->n[node / (8*sizeof(unsigned long))] & 
		(1UL<<(node%(8*sizeof(unsigned long)))))
		return 1;
	return 0;	
}
static inline int nodemask_equal(nodemask_t *a, nodemask_t *b) 
{ 
	int i;
	for (i = 0; i < NUMA_NUM_NODES/(sizeof(unsigned long)*8); i++) 
		if (a->n[i] != b->n[i]) 
			return 0; 
	return 1;
} 

/* NUMA support available. If this returns a negative value all other function
   in this library are undefined. */
int numa_available(void);

/* Basic NUMA state */

/* Get max available node */
int numa_max_node(void);
/* Get current homenode of thread. */
int numa_homenode(void);
/* Return node size and free memory */
long numa_node_size(int node, long *freep);

int numa_pagesize(void); 

/* Set with all nodes. Only valid after numa_available. */
extern nodemask_t numa_all_nodes;

/* Set with no nodes */
extern nodemask_t numa_no_nodes;

/* Only run and allocate memory from a specific set of nodes. */
void numa_bind(nodemask_t *nodes); 
/* Set the NUMA node interleaving mask. 0 to turn off interleaving */
void numa_set_interleave_mask(nodemask_t *nodemask); 
/* Return the current interleaving mask */
nodemask_t numa_get_interleave_mask(void);
/* Some homenode (node to preferably allocate memory from) for thread. */
void numa_set_homenode(int node);
/* Set local memory allocation policy for thread */
void numa_set_localalloc(int flag);
/* Only allocate memory from the nodes set in mask. 0 to turn off */
void numa_set_membind(nodemask_t *nodemask); 
/* Return current membind */ 
nodemask_t numa_get_membind(void);

int numa_get_interleave_node(void);

/* NUMA memory allocation. These functions always round to page size
   and are relatively slow. */

/* Alloc memory page interleaved on nodes in mask */ 
void *numa_alloc_interleaved_subset(size_t size, nodemask_t *nodemask);
/* Alloc memory page interleaved on all nodes. */
void *numa_alloc_interleaved(size_t size);
/* Alloc memory located on node */
void *numa_alloc_onnode(size_t size, int node);
/* Alloc memory on local node */
void *numa_alloc_local(size_t size);
/* Allocation with current policy */
void *numa_alloc(size_t size);
/* Free memory allocated by the functions above */
void numa_free(void *mem, size_t size);

/* Low level functions, primarily for shared memory. All memory
   processed by these must not be touched yet */

/* Interleave an memory area. */
void numa_interleave_memory(void *mem, size_t size, nodemask_t *mask);

/* Allocate a memory area on a specific node. */
void numa_tonode_memory(void *start, size_t size, int node);

/* Allocate a memory area on the current node. */
void numa_setlocal_memory(void *start, size_t size);

/* Allocate memory area with current memory policy */
void numa_police_memory(void *start, size_t size);

/* Run current thread only on nodes in mask */
int numa_run_on_node_mask(nodemask_t *mask);
/* Run current thread only on node */
int numa_run_on_node(int node);

void numa_set_bind_policy(int strict);  

/* Error handling. */
/* This is an internal function in libnuma that can be overwritten by an user
   program. Default is to print an error to stderr and exit if numa_exit_on_error
   is true. */
void numa_error(char *where); 
/* When true exit the program when a NUMA system call (except numa_available) 
   fails */ 
int numa_exit_on_error;
/* Warning function. Can also be overwritten. Default is to print on stderr
   once. */
void numa_warn(int num, char *fmt, ...);

#endif
