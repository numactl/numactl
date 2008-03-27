/* Copyright (C) 2003,2004 Andi Kleen, SuSE Labs.

   libnuma is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; version
   2.1.

   libnuma is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should find a copy of v2.1 of the GNU Lesser General Public License
   somewhere on your Linux system; if not, write to the Free Software 
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

#ifndef _NUMA_H
#define _NUMA_H 1

/* Simple NUMA poliy library */

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bitmask {
	unsigned long size; /* number of bits in the map */
	unsigned long *maskp;
};

#define howmany(x,y) (((x)+((y)-1))/(y))
#define bitsperlong (8 * sizeof(unsigned long))
#define longsperbits(n) howmany(n, bitsperlong)
#define bytesperbits(x) ((x+7)/8)

int bitmask_isbitset(const struct bitmask *, unsigned int);
struct bitmask *bitmask_clearall(struct bitmask *);
struct bitmask *bitmask_setbit(struct bitmask *, unsigned int);
struct bitmask *bitmask_clearbit(struct bitmask *, unsigned int);
unsigned int bitmask_nbytes(struct bitmask *);
struct bitmask *bitmask_alloc(unsigned int);
void bitmask_free(struct bitmask *);
int bitmask_equal(const struct bitmask *, const struct bitmask *);

static inline void nodemask_zero(struct bitmask *mask)
{ 
	bitmask_clearall(mask);
} 

static inline void nodemask_set(struct bitmask *mask, int node)
{
	bitmask_setbit(mask, node);
} 

static inline void nodemask_clr(struct bitmask *mask, int node)
{
	bitmask_clearbit(mask, node);
}

static inline int nodemask_isset(struct bitmask *mask, int node)
{
	return bitmask_isbitset(mask, node);
}
static inline int nodemask_equal(struct bitmask *a, struct bitmask *b)
{ 
	return bitmask_equal(a, b);
} 

/* NUMA support available. If this returns a negative value all other function
   in this library are undefined. */
int numa_available(void);

/* Basic NUMA state */

/* Get max available node */
int numa_max_node(void);
/* Return preferred node */
int numa_preferred(void);

/* Return node size and free memory */
long long numa_node_size64(int node, long long *freep);
long numa_node_size(int node, long *freep);

int numa_pagesize(void); 

/* Set with all nodes. Only valid after numa_available. */
extern struct bitmask *numa_all_nodes;

/* Set with no nodes */
extern struct bitmask *numa_no_nodes;

/* Only run and allocate memory from a specific set of nodes. */
void numa_bind(struct bitmask *nodes);

/* Set the NUMA node interleaving mask. 0 to turn off interleaving */
void numa_set_interleave_mask(struct bitmask *nodemask);

/* Return the current interleaving mask */
struct bitmask *numa_get_interleave_mask(void);

/* allocate a bitmask big enough for all nodes */
struct bitmask *allocate_nodemask(void);

/* Some node to preferably allocate memory from for thread. */
void numa_set_preferred(int node);

/* Set local memory allocation policy for thread */
void numa_set_localalloc(void);

/* Only allocate memory from the nodes set in mask. 0 to turn off */
void numa_set_membind(struct bitmask *nodemask);

/* Return current membind */ 
struct bitmask *numa_get_membind(void);

int numa_get_interleave_node(void);

/* NUMA memory allocation. These functions always round to page size
   and are relatively slow. */

/* Alloc memory page interleaved on nodes in mask */ 
void *numa_alloc_interleaved_subset(size_t size, struct bitmask *nodemask);
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
void numa_interleave_memory(void *mem, size_t size, struct bitmask *mask);

/* Allocate a memory area on a specific node. */
void numa_tonode_memory(void *start, size_t size, int node);

/* Allocate memory on a mask of nodes. */
void numa_tonodemask_memory(void *mem, size_t size, struct bitmask *mask);

/* Allocate a memory area on the current node. */
void numa_setlocal_memory(void *start, size_t size);

/* Allocate memory area with current memory policy */
void numa_police_memory(void *start, size_t size);

/* Run current thread only on nodes in mask */
int numa_run_on_node_mask(struct bitmask *mask);
/* Run current thread only on node */
int numa_run_on_node(int node);
/* Return current mask of nodes the thread can run on */
struct bitmask * numa_get_run_node_mask(void);

/* When strict fail allocation when memory cannot be allocated in target node(s). */
void numa_set_bind_policy(int strict);  

/* Fail when existing memory has incompatible policy */
void numa_set_strict(int flag);

/* maximum nodes (size of kernel nodemask_t) */
int number_of_possible_nodes();

/* maximum cpus (size of kernel cpumask_t) */
int number_of_possible_cpus();

/* nodes in the system */
int number_of_configured_nodes();

/* maximum cpus */
int number_of_configured_cpus();

/* maximum cpus allowed to current task */
int number_of_task_cpus();

/* maximum nodes allowed to current task */
int number_of_task_nodes();

/* allocate a bitmask the size of the kernel cpumask_t */
struct bitmask *allocate_cpumask();

/* allocate a bitmask the size of the kernel nodemask_t */
struct bitmask *allocate_nodemask();

/* set up to represent the cpus available to the current task */
struct bitmask *numa_all_cpus;

/* Convert node to CPU mask. -1/errno on failure, otherwise 0. */
int numa_node_to_cpus(int node, struct bitmask *buffer);

/* Report distance of node1 from node2. 0 on error.*/
int numa_distance(int node1, int node2);

/* Error handling. */
/* This is an internal function in libnuma that can be overwritten by an user
   program. Default is to print an error to stderr and exit if numa_exit_on_error
   is true. */
void numa_error(char *where);
 
/* When true exit the program when a NUMA system call (except numa_available) 
   fails */ 
extern int numa_exit_on_error;
/* Warning function. Can also be overwritten. Default is to print on stderr
   once. */
void numa_warn(int num, char *fmt, ...);

int numa_migrate_pages(int pid, struct bitmask *from, struct bitmask *to);

int numa_move_pages(int pid, unsigned long count, void **pages,
		const int *nodes, int *status, int flags);


#ifdef __cplusplus
}
#endif

#endif
