/* Simple NUMA library.

   All calls are undefined when numa_available returns an error.

   It is strongly recommended to only link this as shared library because
   the kernel API is subject to change.

   Copyright 2003 Andi Kleen, SuSE Labs.   */
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

#include "numaif.h"
#include "numa.h"
#include "cpumask.h"
#include "numaint.h"

#define WEAK __attribute__((weak))

nodemask_t numa_no_nodes;
nodemask_t numa_all_nodes;

static int bind_policy = MPOL_BIND; 

int numa_exit_on_error = 0;

/* Can be overwritten by the application for different error handling */
WEAK void numa_error(char *where) 
{ 
	perror(where); 
	if (numa_exit_on_error)
		exit(1); 
} 

WEAK void numa_warn(int num, char *fmt, ...) 
{ 
	static unsigned warned;
	va_list ap;
	
	/* Give each warning only once */
	if ((1<<num) & warned)
		return; 
	warned |= (1<<num); 

	va_start(ap,fmt);
	fprintf(stderr, "libnuma: Warning: "); 
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
} 

static void setpol(int policy, nodemask_t mask) 
{ 
	if (set_mempolicy(policy, mask.n, NUMA_NUM_NODES+1) < 0) 
		numa_error("set_mempolicy");
} 

static void getpol(int *oldpolicy, nodemask_t *oldmask)
{ 
	if (get_mempolicy(oldpolicy, oldmask->n, NUMA_NUM_NODES+1, 0, 0) < 0) 
		numa_error("get_mempolicy");
} 

static void dombind(void *mem, size_t size, int pol, nodemask_t *nodes)
{ 
	if (mbind(mem, size, pol, nodes->n, nodes ? NUMA_NUM_NODES+1 : 0, 0) < 0) 
		numa_error("mbind"); 
} 

/* (undocumented) */
int numa_pagesize(void)
{ 
	static int pagesize;
	if (pagesize > 0) 
		return pagesize;
	pagesize = getpagesize();
	return pagesize;
} 

static int maxnode = -1; 

static int fallback_max_node(void)
{ 
	char *line = NULL;
	size_t len = 0;
	char *s;
	FILE *f;
	int cpu;
	
	numa_warn(1, "/sys not mounted. Assuming one node per CPU: %s",
		  strerror(errno));

	f = fopen("/proc/cpuinfo","r"); 
	if (!f) {
		numa_warn(2, "/proc not mounted. Assuming zero nodes: %s", 
			  strerror(errno)); 
		return 0;
	}
	maxnode = 0;
	while (getdelim(&line, &len, '\n', f) > 0) { 
		if (strncmp(line,"processor",9))
			continue;
		s = line + strcspn(line, "0123456789"); 
		if (sscanf(s, "%d", &cpu) == 1 && cpu > maxnode) 
			maxnode = cpu;
	} 
	free(line);
	fclose(f); 
	return maxnode;
} 

int numa_max_node(void)
{
	DIR *d;
	struct dirent *de;
	int found;

	/* No hotplug yet. */
	if (maxnode >= 0) 
		return maxnode;
	d = opendir("/sys/devices/system/node"); 
	if (!d)
		return fallback_max_node(); 
	found = 0;
	while ((de = readdir(d)) != NULL) { 
		int nd;
		if (strncmp(de->d_name, "node", 4))
			continue;
		found++;
		nd = strtoul(de->d_name+4, NULL, 0); 
		if (maxnode < nd) 
			maxnode = nd; 
	} 
	closedir(d); 
	if (found == 0) 
		return fallback_max_node();
	return maxnode;
} 

/* (undocumented). FIXME */
/* (cache the result?) */
long numa_node_size(int node, long *freep)
{ 
	size_t len = 0;
	char *line = NULL;
	long size = -1;
	FILE *f; 
	char fn[64];
	int ok = 0;

	if (freep) 
		*freep = -1; 
	sprintf(fn,"/sys/devices/system/node/node%d", node); 
	f = fopen(fn, "r");
	if (!f)
		return -1; 
	while (getdelim(&line, &len, '\n', f) > 0) { 
		char *s = line + strcspn(line, "0123456789");
		if (*s == '\0')
			continue;
		if (!strstr(line, "kB") && !strstr(line, "KB"))
			continue;
		if (strstr(line, "MemTotal")) { 
			ok++;
			size = strtoul(s,NULL,0) << 10; 
		}
		if (strstr(line, "MemFree")) { 
			ok++;
			if (freep) 
				*freep = strtoul(s,NULL,0) << 10; 
		}
	} 
	fclose(f); 
	free(line);
	if (ok != 2)
		numa_warn(3,"Cannot parse sysfs meminfo");
	return size;
}

int numa_available(void)
{
	int max,i;
	if (get_mempolicy(NULL, NULL, 0, 0, 0) < 0 && errno == ENOSYS) 
		return -1; 
	max = numa_max_node();
	for (i = 0; i <= max; i++) 
		nodemask_set(&numa_all_nodes, i); 
	return 0;
} 

void numa_interleave_memory(void *mem, size_t size, nodemask_t *mask)
{ 
	dombind(mem, size, MPOL_INTERLEAVE, mask);
} 

void numa_tonode_memory(void *mem, size_t size, int node)
{
	nodemask_t nodes;
	nodemask_zero(&nodes); 
	nodemask_set(&nodes, node); 
	dombind(mem, size,  bind_policy, &nodes); 
}

void numa_setlocal_memory(void *mem, size_t size)
{
	dombind(mem, size, MPOL_DEFAULT, NULL);
}

void numa_police_memory(void *mem, size_t size)
{
	int pagesize = numa_pagesize(); 
	unsigned long i; 
	for (i = 0; i < size; i += pagesize)
		((unsigned char *)mem)[i] = 0;
}

void *numa_alloc(size_t size)
{
	char *mem;
	mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
		   0, 0); 
	if (mem == (char *)-1)
		return NULL;
	numa_police_memory(mem, size);
	return mem;
} 

void *numa_alloc_interleaved_subset(size_t size, nodemask_t *mask) 
{ 
	char *mem;	

	mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
		   0, 0); 
	if (mem == (char *)-1) 
		return NULL;
	dombind(mem, size, MPOL_INTERLEAVE, mask);
	return mem;
} 

void *numa_alloc_interleaved(size_t size)
{ 
	return numa_alloc_interleaved_subset(size, &numa_all_nodes); 
} 

void numa_set_interleave_mask(nodemask_t *mask)
{ 
	if (nodemask_equal(mask, &numa_no_nodes))
		setpol(MPOL_DEFAULT, *mask); 
	else
		setpol(MPOL_INTERLEAVE, *mask);
} 

nodemask_t numa_get_interleave_mask(void)
{ 
	int oldpolicy;
	nodemask_t mask; 
	getpol(&oldpolicy, &mask); 
	if (oldpolicy == MPOL_INTERLEAVE)
		return mask;
	return numa_no_nodes; 
} 

/* (undocumented) */
int numa_get_interleave_node(void)
{ 
	int nd;
	if (get_mempolicy(&nd, NULL, 0, 0, MPOL_F_ILNODE) == 0)
		return nd;
	return 0;	
} 

void *numa_alloc_onnode(size_t size, int node) 
{ 
	char *mem; 
	nodemask_t nodes;
	nodemask_zero(&nodes); 
	nodemask_set(&nodes, node); 
	mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
		   0, 0);  
	if (mem == (char *)-1)
		return NULL;		
	dombind(mem, size, bind_policy, &nodes); 
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

/* ??? not threadsafe. Do that per thread? */ 
void numa_set_bind_policy(int strict) 
{ 
	if (strict) 
		bind_policy = MPOL_BIND; 
	else
		bind_policy = MPOL_PREFERED;
} 

void numa_set_membind(nodemask_t *mask) 
{ 
	setpol(MPOL_BIND, *mask);
} 

nodemask_t numa_get_membind(void)
{
	int oldpolicy;
	nodemask_t nodes;
	getpol(&oldpolicy, &nodes);
	if (oldpolicy == MPOL_BIND)
		return nodes;
	return numa_all_nodes;	
} 

void numa_free(void *mem, size_t size)
{ 
	munmap(mem, size); 
} 

/* Ugliest function due to braindead kernel output format for cpumaps */
static cpumask_t *node_to_cpu(int node) 
{
	char fn[64];
	FILE *f; 
	char *line = NULL; 
	size_t len = 0; 
	char *s;
	int n;
	static cpumask_t cpus[NUMA_NUM_NODES];  
	if (!cpumask_empty(&cpus[node]))
		return &cpus[node];
	/* Thread races are harmless */
	sprintf(fn, "/sys/devices/system/node/node%d/cpumap", node); 
	f = fopen(fn, "r"); 
	if (!f || getdelim(&line, &len, '\n', f) < 1) { 
		numa_warn(4,"/sys not mounted or invalid. Assuming nodes equal CPU: %s",
			  strerror(errno)); 
		goto fallback;
	} 
	n = 0;
	s = line; 	
	while (*s) {
 		unsigned short num; 
		int i;
		num = 0;
		for (i = 0; i < 4 && s[i]; i++) { 
			static const char hexdigits[] = "0123456789abcdef";
			char *w = strchr(hexdigits, tolower(s[i]));
			if (!w) { 
				numa_warn(5, "Unknown character in sysfs cpumap");
				goto fallback; 
			}
			num = (num*16) + (w - hexdigits); 
		}		
		if (n > sizeof(cpumask_t)/sizeof(short)) { 
			if (num) { 
				numa_warn(6, "Too many CPUs in cpumap. Recompile libnuma."); 
				goto fallback;
			}
		}
		((unsigned short *)(cpus[node].m))[n] = num;
		n++;
		s += i;
	} 		
	free(line);
	fclose(f);
	return &cpus[node]; 

 fallback:
	cpumask_set(&cpus[node], node);
	return &cpus[node];		
} 

int numa_run_on_node_mask(nodemask_t *mask)
{ 	
	int i;
	cpumask_t cpus;
	cpumask_zero(&cpus); 
	for (i = 0; i < NUMA_NUM_NODES; i++) { 
		if (nodemask_isset(mask, i))
			cpumask_or(&cpus, &cpus, node_to_cpu(i)); 
	}
	return numa_sched_setaffinity(getpid(), sizeof(cpumask_t), cpus.m);
} 

int numa_run_on_node(int node)
{ 
	cpumask_t cpus; 
	if (node == -1) 
		cpumask_fill(&cpus); 
	else if (node < NUMA_NUM_NODES) {
		cpumask_zero(&cpus);
		cpumask_or(&cpus, &cpus, node_to_cpu(node)); 
	} else
		return -EINVAL; 
	return numa_sched_setaffinity(getpid(), sizeof(cpumask_t), cpus.m);
} 

int numa_homenode(void)
{ 
	int policy;
	nodemask_t nodes;
	getpol(&policy, &nodes);
	if (policy == MPOL_PREFERED || policy == MPOL_BIND) { 
		int i;
		int max = NUMA_NUM_NODES;
		for (i = 0; i < max ; i++) 
			if (nodemask_isset(&nodes, i))
				return i; 
	}
	return 0; /* or random one? */
}

void numa_set_homenode(int node)
{ 
	nodemask_t n;
	nodemask_zero(&n);
	nodemask_set(&n, node); 
	setpol(MPOL_PREFERED, n);
} 

void numa_set_localalloc(int flag) 
{
	nodemask_t empty;
	nodemask_zero(&empty);
	setpol(MPOL_DEFAULT, empty);
} 

void numa_bind(nodemask_t *nodemask)
{
	numa_run_on_node_mask(nodemask); 
	numa_set_membind(nodemask);
}
