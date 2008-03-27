/* Simple NUMA library.
   Copyright (C) 2003,2004,2005 Andi Kleen, SuSE Labs.

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 

   All calls are undefined when numa_available returns an error. */
#define _GNU_SOURCE 1
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>

#include <sys/mman.h>
#include <limits.h>

#include "numa.h"
#include "numaif.h"
#include "numaint.h"
#include "util.h"

#define WEAK __attribute__((weak))

struct bitmask *numa_no_nodes = NULL;
struct bitmask *numa_all_nodes = NULL;
struct bitmask *numa_all_cpus = NULL;
struct bitmask **node_cpu_mask;

#ifdef __thread
#warning "not threadsafe"
#endif

static __thread int bind_policy = MPOL_BIND; 
static __thread unsigned int mbind_flags = 0;
static int sizes_set=0;
static int maxconfigurednode = -1;
static int maxconfiguredcpu = -1;
static int maxprocnode = -1;
static int maxproccpu = -1;
static int nodemask_sz = 0;
static int cpumask_sz = 0;

int numa_exit_on_error = 0;

/*
 * The following bitmask declarations, bitmask_*() routines, and associated
 * _setbit() and _getbit() routines are:
 * Copyright (c) 2004 Silicon Graphics, Inc. (SGI) All rights reserved.
 * SGI publishes it under the terms of the GNU General Public License, v2,
 * as published by the Free Software Foundation.
 */
static unsigned int
_getbit(const struct bitmask *bmp, unsigned int n)
{
	if (n < bmp->size)
		return (bmp->maskp[n/bitsperlong] >> (n % bitsperlong)) & 1;
	else
		return 0;
}

static void
_setbit(struct bitmask *bmp, unsigned int n, unsigned int v)
{
	if (n < bmp->size) {
		if (v)
			bmp->maskp[n/bitsperlong] |= 1UL << (n % bitsperlong);
		else
			bmp->maskp[n/bitsperlong] &= ~(1UL << (n % bitsperlong));
	}
}

int
bitmask_isbitset(const struct bitmask *bmp, unsigned int i)
{
	return _getbit(bmp, i);
}

struct bitmask *
bitmask_setall(struct bitmask *bmp)
{
	unsigned int i;
	for (i = 0; i < bmp->size; i++)
		_setbit(bmp, i, 1);
	return bmp;
}

struct bitmask *
bitmask_clearall(struct bitmask *bmp)
{
	unsigned int i;
	for (i = 0; i < bmp->size; i++)
		_setbit(bmp, i, 0);
	return bmp;
}

struct bitmask *
bitmask_setbit(struct bitmask *bmp, unsigned int i)
{
	_setbit(bmp, i, 1);
	return bmp;
}

struct bitmask *
bitmask_clearbit(struct bitmask *bmp, unsigned int i)
{
	_setbit(bmp, i, 0);
	return bmp;
}

unsigned int
bitmask_nbytes(struct bitmask *bmp)
{
	return longsperbits(bmp->size) * sizeof(unsigned long);
}

/* where n is the number of bits in the map */
struct bitmask *
bitmask_alloc(unsigned int n)
{
	struct bitmask *bmp;

	if (n < 1) {
		printf ("request to allocate mask for %d bits; abort\n", n);
		exit(1);
	}
	bmp = malloc(sizeof(*bmp));
	if (bmp == 0)
		return 0;
	bmp->size = n;
	bmp->maskp = calloc(longsperbits(n), sizeof(unsigned long));
	if (bmp->maskp == 0) {
		free(bmp);
		return 0;
	}
	return bmp;
}

void
bitmask_free(struct bitmask *bmp)
{
	if (bmp == 0)
		return;
	free(bmp->maskp);
	bmp->maskp = (unsigned long *)0xdeadcdef;  /* double free tripwire */
	free(bmp);
	return;
}

/* True if two bitmasks are equal */
int
bitmask_equal(const struct bitmask *bmp1, const struct bitmask *bmp2)
{
	unsigned int i;
	for (i = 0; i < bmp1->size || i < bmp2->size; i++)
		if (_getbit(bmp1, i) != _getbit(bmp2, i))
			return 0;
	return 1;
}
/* *****end of bitmask_  routines ************ */

make_internal_alias(numa_exit_on_error);

/* Next two can be overwritten by the application for different error handling */
WEAK void numa_error(char *where) 
{ 
	int olde = errno;
	perror(where); 
	if (numa_exit_on_error_int)
		exit(1); 
	errno = olde;
} 

WEAK void numa_warn(int num, char *fmt, ...) 
{ 
	static unsigned warned;
	va_list ap;
	int olde = errno;
	
	/* Give each warning only once */
	if ((1<<num) & warned)
		return; 
	warned |= (1<<num); 

	va_start(ap,fmt);
	fprintf(stderr, "libnuma: Warning: "); 
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);

	errno = olde;
} 

static void setpol(int policy, struct bitmask *bmp)
{ 
	if (set_mempolicy(policy, bmp->maskp, bmp->size) < 0)
		numa_error("set_mempolicy");
} 

static void getpol(int *oldpolicy, struct bitmask *bmp)
{ 
	if (get_mempolicy(oldpolicy, bmp->maskp, bmp->size, 0, 0) < 0)
		numa_error("get_mempolicy");
} 

static void dombind(void *mem, size_t size, int pol, struct bitmask *bmp)
{ 
	if (mbind(mem, size, pol, bmp->maskp, bmp->size, mbind_flags) < 0)
		numa_error("mbind"); 
} 

/* (undocumented) */
/* gives the wrong answer for hugetlbfs mappings. */
int numa_pagesize(void)
{ 
	static int pagesize;
	if (pagesize > 0) 
		return pagesize;
	pagesize = getpagesize();
	return pagesize;
} 

make_internal_alias(numa_pagesize);

/*
 * Find the highest numbered existing memory node: maxconfigurednode.
 */
void
set_configured_nodes(void)
{
	DIR *d;
	struct dirent *de;

	d = opendir("/sys/devices/system/node");
	if (!d) {
		numa_warn(W_nosysfs,
		   "/sys not mounted or no numa system. Assuming one node: %s",
		  	strerror(errno));
		maxconfigurednode = 0;
	} else {
		while ((de = readdir(d)) != NULL) {
			int nd;
			if (strncmp(de->d_name, "node", 4))
				continue;
			nd = strtoul(de->d_name+4, NULL, 0);
			if (maxconfigurednode < nd)
				maxconfigurednode = nd;
		}
		closedir(d);
	}
}

/*
 * Convert the string length of an ascii hex mask to the number
 * of bits represented by that mask.
 */
static int s2nbits(const char *s)
{
	return strlen(s) * 32 / 9;
}

/*
 * Determine number of bytes in a seekable open file, without
 * assuming that stat(2) on that file has a useful size.
 * Has side affect of leaving the file rewound to the beginnning.
 */
static int filesize(FILE *fp)
{
	int sz = 0;
	rewind(fp);
	while (fgetc(fp) != EOF)
		sz++;
	rewind(fp);
	return sz;
}
/* Is string 'pre' a prefix of string 's'? */
static int strprefix(const char *s, const char *pre)
{
	return strncmp(s, pre, strlen(pre)) == 0;
}

static const char *mask_size_file = "/proc/self/status";
static const char *nodemask_prefix = "Mems_allowed:\t";
/*
 * (do this the way Paul Jackson's libcpuset does it)
 * The nodemask values in /proc/self/status are in an
 * ascii format that uses 9 characters for each 32 bits of mask.
 * (this could also be used to find the cpumask size)
 */
static void set_nodemask_size()
{
	FILE *fp = NULL;
	char *buf = NULL;
	int fsize;

	if ((fp = fopen(mask_size_file, "r")) == NULL)
		goto done;
	fsize = filesize(fp);
	if ((buf = malloc(fsize)) == NULL)
		goto done;

	/*
	 * Beware: mask sizing arithmetic is fussy.
	 * The trailing newline left by fgets() is required.
	 */
	while (fgets(buf, fsize, fp)) {
		if (strprefix(buf, nodemask_prefix)) {
			nodemask_sz = s2nbits(buf + strlen(nodemask_prefix));
			break;
		}
	}
done:
	if (buf != NULL)
		free(buf);
	if (fp != NULL)
		fclose(fp);
	if (nodemask_sz == 0) /* fall back on error */
		nodemask_sz = maxconfigurednode+1;
}

/*
 * Read a mask consisting of a sequence of hexadecimal longs separated by
 * commas. Order them correctly and return the number of the last bit
 * set.
 */
int
read_mask(char *s, struct bitmask *bmp)
{
	char *end = s;
	unsigned int *start = (unsigned int *)bmp->maskp;
	unsigned int *p = start;
	unsigned int *q;
	unsigned int i;
	unsigned int n = 0;

	i = strtoul(s, &end, 16);

	/* Skip leading zeros */
	while (!i && *end++ == ',')
		i = strtoul(end, &end, 16);

	if (!i)
		/* End of string. No mask */
		return -1;

	/* Read sequence of ints */
	do {
		start[n++] = i;
		i = strtoul(end, &end, 16);
	} while (*end++ == ',');
	n--;

	/*
	 * Invert sequence of ints if necessary since the first int
	 * is the highest and we put it first because we read it first.
	 */
	for (q = start + n, p = start; p < q; q--, p++) {
		unsigned int x = *q;

		*q = *p;
		*p = x;
	}

	/* Poor mans fls() */
	for(i = 31; i >= 0; i--)
		if (test_bit(i, start + n))
			break;

	/*
	 * Return the last bit set
	 */
	return sizeof(unsigned int) * n + i;
}

/*
 * Read a processes constraints in terms of nodes and cpus from
 * /proc/pid/status.
 */
void
set_task_constraints(void)
{
	int max_cpus = number_of_possible_cpus();
	int buflen;
	char *buffer;
	FILE *f;
	/*
	 * The maximum line size consists of the string at the beginning plus
	 * a digit for each 4 cpus and a comma for each 64 cpus.
	 */
	buflen = max_cpus / 4 + max_cpus / BITS_PER_LONG + 40;
	buffer = malloc(buflen);

	numa_all_cpus = allocate_cpumask();
	numa_all_nodes = allocate_nodemask();

	sprintf(buffer,"/proc/%d/status", getpid());
	f = fopen(buffer, "r");
	if (!f) {
		numa_warn(W_cpumap, "Cannot parse /proc/%d/status", getpid());
		return;
	}

	while (fgets(buffer, buflen, f)) {

		if (strncmp(buffer,"Cpus_allowed",12) == 0)
			maxproccpu = read_mask(buffer + 14, numa_all_cpus);

		if (strncmp(buffer,"Mems_allowed",12) == 0) {
			maxprocnode =
				read_mask(buffer + 14, numa_all_nodes);
		}
	}
	fclose(f);
	free (buffer);

	if (maxprocnode < 0) {
		numa_warn(W_cpumap, "Cannot parse /proc/%d/status", getpid());
		return;
	}
	return;
}

/*
 * Find the highest cpu number possible (in other words the size
 * of a kernel cpumask_t (in bits) - 1)
 */
void
set_numa_max_cpu(void)
{
	int len = 2048;
	int n;
	int olde = errno;
	struct bitmask *buffer;

	do {
		buffer = bitmask_alloc(len);
		n = numa_sched_getaffinity_int(0, buffer);
		/* on success, returns size of kernel cpumask_t, in bytes */
		if (n < 0 && errno == EINVAL) {
			if (len >= 1024*1024)
				break;
			len *= 2;
			bitmask_free(buffer);
			continue;
		}
	} while (n < 0);
	bitmask_free(buffer);
	errno = olde;
	cpumask_sz = n*8;
}

/*
 * get the total (configured) number of cpus - both online and offline
 */
void
set_configured_cpus()
{
	int		filecount=0;
	char		*dirnamep = "/sys/devices/system/cpu";
	struct dirent	*dirent;
	DIR		*dir;
	dir = opendir(dirnamep);

	if (dir == NULL) {
		fprintf (stderr,
			"cannot open directory %s\n", dirnamep);
		return;
	}
	while ((dirent = readdir(dir)) != 0) {
		if (!strncmp("cpu", dirent->d_name, 3)) {
			filecount++;
		} else {
			continue;
		}
	}
	closedir(dir);
	maxconfiguredcpu = filecount-1; /* high cpu number */
	return;
}

/*
 * Initialize all the sizes.
 */
void
set_sizes()
{
	sizes_set++;
	set_configured_nodes();	/* configured nodes listed in /sys */
	set_nodemask_size();	/* size of nodemask_t */
	set_numa_max_cpu();	/* size of cpumask_t */
	set_configured_cpus();	/* cpus listed in /sys/devices/system/cpu */
	set_task_constraints();	/* cpus and nodes for current task */
}

int
number_of_configured_nodes()
{
	if (!sizes_set)
		set_sizes();
	return maxconfigurednode+1;
}

int
number_of_configured_cpus(void)
{

	if (!sizes_set)
		set_sizes();
	return maxconfiguredcpu+1;
}

int
number_of_possible_nodes()
{
	if (!sizes_set)
		set_sizes();
	return nodemask_sz;
}

int
number_of_possible_cpus()
{
	if (!sizes_set)
		set_sizes();
	return cpumask_sz;
}

int
number_of_task_nodes()
{
	if (!sizes_set)
		set_sizes();
	return maxprocnode+1;
}

int
number_of_task_cpus()
{
	if (!sizes_set)
		set_sizes();
	return maxproccpu+1;
}

/*
 * Allocate a bitmask for cpus, of a size large enough to
 * match the kernel's cpumask_t.
 */
struct bitmask *
allocate_cpumask()
{
	int ncpus = number_of_possible_cpus();

	return bitmask_alloc(ncpus);
}

/*
 * Allocate a bitmask for nodes, of a size large enough to
 * match the kernel's nodemask_t.
 */
struct bitmask *
allocate_nodemask()
{
	int nnodes = numa_max_node()+1;

	return bitmask_alloc(nnodes);
}

/*
 * Return the number of the highest node in the system, in other words
 * the size of a kernel nodemask_t (in bits) - 1).
 */
int
numa_max_node(void)
{
	if (!sizes_set)
		set_sizes();
	return nodemask_sz -1;
}

make_internal_alias(numa_max_node);

/* (cache the result?) */
long long numa_node_size64(int node, long long *freep)
{ 
	size_t len = 0;
	char *line = NULL;
	long long size = -1;
	FILE *f; 
	char fn[64];
	int ok = 0;
	int required = freep ? 2 : 1; 

	if (freep) 
		*freep = -1; 
	sprintf(fn,"/sys/devices/system/node/node%d/meminfo", node); 
	f = fopen(fn, "r");
	if (!f)
		return -1; 
	while (getdelim(&line, &len, '\n', f) > 0) { 
		char *end;
		char *s = strcasestr(line, "kB"); 
		if (!s) 
			continue; 
		--s; 
		while (s > line && isspace(*s))
			--s;
		while (s > line && isdigit(*s))
			--s; 
		if (strstr(line, "MemTotal")) { 
			size = strtoull(s,&end,0) << 10; 
			if (end == s) 
				size = -1;
			else
				ok++; 
		}
		if (freep && strstr(line, "MemFree")) { 
			*freep = strtoull(s,&end,0) << 10; 
			if (end == s) 
				*freep = -1;
			else
				ok++; 
		}
	} 
	fclose(f); 
	free(line);
	if (ok != required)
		numa_warn(W_badmeminfo, "Cannot parse sysfs meminfo (%d)", ok);
	return size;
}

make_internal_alias(numa_node_size64);

long numa_node_size(int node, long *freep)
{	
	long long f2;	
	long sz = numa_node_size64_int(node, &f2);
	if (freep) 
		*freep = f2; 
	return sz;	
}

int numa_available(void)
{
	if (get_mempolicy(NULL, NULL, 0, 0, 0) < 0 && errno == ENOSYS)
		return -1; 
	numa_max_node_int();
	return 0;
} 

void numa_interleave_memory(void *mem, size_t size, struct bitmask *bmp)
{ 
	dombind(mem, size, MPOL_INTERLEAVE, bmp);
} 

void numa_tonode_memory(void *mem, size_t size, int node)
{
	struct bitmask *nodes;

	nodes = allocate_nodemask();
	bitmask_setbit(nodes, node);
	dombind(mem, size, bind_policy, nodes);
}

void numa_tonodemask_memory(void *mem, size_t size, struct bitmask *bmp)
{
	dombind(mem, size,  bind_policy, bmp);
}

void numa_setlocal_memory(void *mem, size_t size)
{
	dombind(mem, size, MPOL_PREFERRED, NULL);
}

void numa_police_memory(void *mem, size_t size)
{
	int pagesize = numa_pagesize_int();
	unsigned long i; 
	for (i = 0; i < size; i += pagesize)
		asm volatile("" :: "r" (((volatile unsigned char *)mem)[i]));
}

make_internal_alias(numa_police_memory);

void *numa_alloc(size_t size)
{
	char *mem;
	mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
		   0, 0); 
	if (mem == (char *)-1)
		return NULL;
	numa_police_memory_int(mem, size);
	return mem;
} 

void *numa_alloc_interleaved_subset(size_t size, struct bitmask *bmp)
{ 
	char *mem;	

	mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
		   0, 0); 
	if (mem == (char *)-1) 
		return NULL;
	dombind(mem, size, MPOL_INTERLEAVE, bmp);
	return mem;
} 

make_internal_alias(numa_alloc_interleaved_subset);

void *numa_alloc_interleaved(size_t size)
{ 
	return numa_alloc_interleaved_subset_int(size, numa_all_nodes);
} 

void numa_set_interleave_mask(struct bitmask *bmp)
{ 
	if (!numa_no_nodes)
		numa_no_nodes = allocate_nodemask();

	if (bitmask_equal(bmp, numa_no_nodes))
		setpol(MPOL_DEFAULT, bmp);
	else
		setpol(MPOL_INTERLEAVE, bmp);
} 

struct bitmask *
numa_get_interleave_mask(void)
{ 
	int oldpolicy;
	struct bitmask *bmp;

	bmp = allocate_nodemask();
	getpol(&oldpolicy, bmp);
	if (oldpolicy == MPOL_INTERLEAVE)
		return bmp;
	return numa_no_nodes; 
} 

/* (undocumented) */
int numa_get_interleave_node(void)
{ 
	int nd;
	if (get_mempolicy(&nd, NULL, 0, 0, MPOL_F_NODE) == 0)
		return nd;
	return 0;	
} 

void *numa_alloc_onnode(size_t size, int node) 
{ 
	char *mem; 
	struct bitmask *bmp;

	bmp = allocate_nodemask();
	bitmask_setbit(bmp, node);
	mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
		   0, 0);  
	if (mem == (char *)-1)
		return NULL;		
	dombind(mem, size, bind_policy, bmp);
	return mem; 	
} 

void *numa_alloc_local(size_t size) 
{ 
	char *mem; 
	mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
		   0, 0); 
	if (mem == (char *)-1)
		return NULL;
	dombind(mem, size, MPOL_DEFAULT, NULL); 
	return mem; 	
} 

void numa_set_bind_policy(int strict) 
{ 
	if (strict) 
		bind_policy = MPOL_BIND; 
	else
		bind_policy = MPOL_PREFERRED;
} 

void numa_set_membind(struct bitmask *bmp)
{ 
	setpol(MPOL_BIND, bmp);
} 

make_internal_alias(numa_set_membind);

struct bitmask *
numa_get_membind(void)
{
	int oldpolicy;
	struct bitmask *bmp;

	bmp = allocate_nodemask();
	getpol(&oldpolicy, bmp);
	if (oldpolicy == MPOL_BIND)
		return bmp;
	return numa_all_nodes;	
} 

void numa_free(void *mem, size_t size)
{ 
	munmap(mem, size); 
} 

static void bad_cpumap(struct bitmask *mask)
{
	int n;

	for (n = 0; n < mask->size; n++)
		bitmask_setbit(mask, n);
}

int numa_parse_bitmap(char *line, struct bitmask *mask)
{
	int i, ncpus;
	char *p = strchr(line, '\n'); 
	if (!p)
		return -1;
	ncpus = mask->size;

	for (i = 0; p > line;i++) {
		char *oldp, *endp; 
		oldp = p;
		if (*p == ',') 
			--p;
		while (p > line && *p != ',')
			--p;
		/* Eat two 32bit fields at a time to get longs */
		if (p > line && sizeof(unsigned long) == 8) {
			oldp--;
			memmove(p, p+1, oldp-p+1);
			while (p > line && *p != ',')
				--p;
		}
		if (*p == ',')
			p++;
		if (i >= CPU_LONGS(ncpus))
			return -1;
		mask->maskp[i] = strtoul(p, &endp, 16);
		if (endp != oldp)
			return -1;
		p--;
	}
	return 0;
}

void
init_node_cpu_mask()
{
	int nnodes = numa_max_node()+1;
	node_cpu_mask = calloc (nnodes, sizeof(struct bitmask *));
}

/*
 * test whether a node has cpus
 */
/* This would be better with some locking, but I don't want to make libnuma
   dependent on pthreads right now. The races are relatively harmless. */
/*
 * deliver a bitmask of cpus representing the cpus on a given node
 */
int numa_node_to_cpus(int node, struct bitmask *buffer)
{
	int err = 0, bufferlen;
	int nnodes = number_of_configured_nodes();
	char fn[64], *line = NULL;
	FILE *f; 
	size_t len = 0; 
	struct bitmask *mask;

	if (!node_cpu_mask)
		init_node_cpu_mask();

	bufferlen = bitmask_nbytes(buffer);
	if (node > nnodes-1) {
		errno = ERANGE;
		return -1;
	}
	bitmask_clearall(buffer);

	if (node_cpu_mask[node]) { 
		/* have already constructed a mask for this node */
		if (buffer->size != node_cpu_mask[node]->size) {
			printf ("map size mismatch; abort\n");
			exit(1);
		}
		memcpy(buffer->maskp, node_cpu_mask[node]->maskp, bufferlen);
		return 0;
	}

	/* need a new mask for this node */
	mask = allocate_cpumask();

	/* this is a kernel cpumask_t (see node_read_cpumap()) */
	sprintf(fn, "/sys/devices/system/node/node%d/cpumap", node); 
	f = fopen(fn, "r"); 
	if (!f || getdelim(&line, &len, '\n', f) < 1) { 
		numa_warn(W_nosysfs2,
		   "/sys not mounted or invalid. Assuming one node: %s",
			  strerror(errno)); 
		bad_cpumap(mask);
		err = -1;
	} 
	if (f)
		fclose(f);

	if (line && numa_parse_bitmap(line, mask) < 0) {
		numa_warn(W_cpumap, "Cannot parse cpumap. Assuming one node");
		bad_cpumap(mask);
		err = -1;
	}

	free(line);
	memcpy(buffer->maskp, mask->maskp, bufferlen);

	/* slightly racy, see above */ 
	/* save the mask we created */
	if (node_cpu_mask[node]) {
		/* how could this be? */
		if (mask != buffer)
			bitmask_free(mask);
	} else {
		node_cpu_mask[node] = mask; 
	} 
	return err; 
} 

make_internal_alias(numa_node_to_cpus);

/*
 * Given a node mask (size of a kernel nodemask_t) (probably populated by
 * a user argument list) set up a map of cpus (map "cpus") on those nodes.
 * Then set affinity to those cpus.
 */
int numa_run_on_node_mask(struct bitmask *bmp)
{ 	
	int ncpus, i, k, err;
	struct bitmask *cpus, *nodecpus;

	cpus = allocate_cpumask();
	ncpus = cpus->size;
	nodecpus = allocate_cpumask();

	for (i = 0; i < bmp->size; i++) {
		if (bmp->maskp[i / BITS_PER_LONG] == 0)
			continue;
		if (bitmask_isbitset(bmp, i)) {
			if (numa_node_to_cpus_int(i, nodecpus) < 0) {
				numa_warn(W_noderunmask, 
					"Cannot read node cpumask from sysfs");
				continue;
			}
			for (k = 0; k < CPU_LONGS(ncpus); k++)
				cpus->maskp[k] |= nodecpus->maskp[k];
		}	
	}
	err = numa_sched_setaffinity_int(0, cpus);

	/* used to have to consider that this could fail - it shouldn't now */
	if (err < 0) {
		printf ("numa_sched_setaffinity_int() failed; abort\n");
		exit(1);
	}
	bitmask_free(cpus);
	bitmask_free(nodecpus);

	return err;
} 

make_internal_alias(numa_run_on_node_mask);

struct bitmask *
numa_get_run_node_mask(void)
{ 
	int ncpus = number_of_configured_cpus();
	int i, k;
	int max = numa_max_node_int();
	struct bitmask *bmp, *cpus, *nodecpus;

	bmp = allocate_cpumask();
	cpus = allocate_cpumask();
	nodecpus = allocate_cpumask();

	if (numa_sched_getaffinity_int(0, cpus) < 0)
		return numa_no_nodes; 

	for (i = 0; i <= max; i++) {
		if (numa_node_to_cpus_int(i, nodecpus) < 0) {
			/* It's possible for the node to not exist */
			continue;
		}
		for (k = 0; k < CPU_LONGS(ncpus); k++) {
			if (nodecpus->maskp[k] & cpus->maskp[k])
				bitmask_setbit(bmp, i);
		}
	}		
	return bmp;
} 

int
numa_migrate_pages(int pid, struct bitmask *fromnodes, struct bitmask *tonodes)
{
	int numa_num_nodes = number_of_possible_nodes();

	return migrate_pages(pid, numa_num_nodes + 1, fromnodes->maskp,
							tonodes->maskp);
}

int numa_move_pages(int pid, unsigned long count,
	void **pages, const int *nodes, int *status, int flags)
{
	return move_pages(pid, count, pages, nodes, status, flags);
}

int numa_run_on_node(int node)
{ 
	int numa_num_nodes = number_of_possible_nodes();
	struct bitmask *cpus;

	if (node == -1) {
		cpus = allocate_cpumask();
		bitmask_setall(cpus);
	} else if (node < numa_num_nodes) {
		if (numa_node_to_cpus_int(node, cpus) < 0) {
			numa_warn(W_noderunmask,
				"Cannot read node cpumask from sysfs");
			return -1; 
		} 		
	} else { 
		errno = EINVAL;
		return -1; 
	}
	return numa_sched_setaffinity_int(0, cpus);
} 

int numa_preferred(void)
{ 
	int policy;
	struct bitmask *bmp;

	bmp = allocate_nodemask();
	getpol(&policy, bmp);
	if (policy == MPOL_PREFERRED || policy == MPOL_BIND) { 
		int i;
		int max = number_of_possible_nodes();
		for (i = 0; i < max ; i++) 
			if (bitmask_isbitset(bmp, i))
				return i; 
	}
	/* could read the current CPU from /proc/self/status. Probably 
	   not worth it. */
	return 0; /* or random one? */
}

void numa_set_preferred(int node)
{ 
	struct bitmask *bmp;

	bmp = allocate_nodemask();
	if (node >= 0)
		bitmask_setbit(bmp, node);
	setpol(MPOL_PREFERRED, bmp);
	bitmask_free(bmp);
	return;
} 

void numa_set_localalloc(void) 
{	
	struct bitmask *bmp;

	bmp = allocate_nodemask();
	setpol(MPOL_PREFERRED, bmp);
	bitmask_free(bmp);
	return;
} 

void numa_bind(struct bitmask *bmp)
{
	numa_run_on_node_mask_int(bmp);
	numa_set_membind_int(bmp);
}

void numa_set_strict(int flag)
{
	if (flag)
		mbind_flags |= MPOL_MF_STRICT;
	else
		mbind_flags &= ~MPOL_MF_STRICT;
}
