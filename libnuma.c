/* Simple NUMA library.

   All calls are undefined when numa_available returns an error.

   Copyright 2003,2004 Andi Kleen, SuSE Labs.   */
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
#include "numaint.h"

#define WEAK __attribute__((weak))

nodemask_t numa_no_nodes;
nodemask_t numa_all_nodes;

#if __GNUC__ < 3 || (__GNUC__ == 3 && __GNUC_MINOR__ < 3)
#warning "not threadsafe"
#define __thread
#endif

static __thread int bind_policy = MPOL_BIND; 
static __thread int mbind_flags = 0;

int numa_exit_on_error = 0;

enum numa_warn { 
	W_nosysfs,
	W_noproc,
	W_badmeminfo,
	W_nosysfs2,
	W_cpumap,
	W_numcpus,
	W_noderunmask,
}; 

/* Next two can be overwritten by the application for different error handling */
WEAK void numa_error(char *where) 
{ 
	int olde = errno;
	perror(where); 
	if (numa_exit_on_error)
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

static void setpol(int policy, nodemask_t mask) 
{ 
	if (set_mempolicy(policy, &mask.n[0], NUMA_NUM_NODES+1) < 0) 
		numa_error("set_mempolicy");
} 

static void getpol(int *oldpolicy, nodemask_t *oldmask)
{ 
	if (get_mempolicy(oldpolicy, oldmask->n, NUMA_NUM_NODES+1, 0, 0) < 0) 
		numa_error("get_mempolicy");
} 

static void dombind(void *mem, size_t size, int pol, nodemask_t *nodes)
{ 
	if (mbind(mem, size, pol, nodes->n, nodes ? NUMA_NUM_NODES+1 : 0, mbind_flags) 
	    < 0) 
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

static int maxnode = -1; 
static int maxcpus = -1; 

static int number_of_cpus(void)
{ 
	char *line = NULL;
	size_t len = 0;
	char *s;
	FILE *f;
	int cpu;
	
	if (maxcpus >= 0) 
		return maxcpus;

	f = fopen("/proc/cpuinfo","r"); 
	if (!f) {
		int n;
		unsigned long buffer[CPU_WORDS(8192)];
		memset(buffer, 0, sizeof(buffer));
		n = numa_sched_getaffinity(getpid(), sizeof(buffer), buffer);
		if (n >= 0) { 
			int i, k;
			for (i = 0; i < n / sizeof(long); i++) {
				if (!buffer[i])
					continue;
				for (k = 0; k< 8; k++) 
					if (buffer[i] & (1<<k))
						maxcpus = i+k;
			}
			return maxcpus;
		}
		numa_warn(W_noproc, "/proc not mounted. Assuming zero nodes: %s", 
			  strerror(errno)); 
		return 0;
	}
	maxcpus = 0;
	while (getdelim(&line, &len, '\n', f) > 0) { 
		if (strncmp(line,"processor",9))
			continue;
		s = line + strcspn(line, "0123456789"); 
		if (sscanf(s, "%d", &cpu) == 1 && cpu > maxcpus) 
			maxcpus = cpu;
	} 
	free(line);
	fclose(f); 
	return maxcpus;
} 

static int fallback_max_node(void)
{
	numa_warn(W_nosysfs, "/sys not mounted. Assuming one node per CPU: %s",
		  strerror(errno));
	maxnode = number_of_cpus();	
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

/* (cache the result?) */
long numa_node_size(int node, long *freep)
{ 
	size_t len = 0;
	char *line = NULL;
	long size = -1;
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
			size = strtoul(s,&end,0) << 10; 
			if (end == s) 
				size = -1;
			else
				ok++; 
		}
		if (freep && strstr(line, "MemFree")) { 
			*freep = strtoul(s,&end,0) << 10; 
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

void numa_tonodemask_memory(void *mem, size_t size, nodemask_t *mask)
{
	dombind(mem, size,  bind_policy, mask); 
}

void numa_setlocal_memory(void *mem, size_t size)
{
	dombind(mem, size, MPOL_PREFERRED, NULL);
}

void numa_police_memory(void *mem, size_t size)
{
	int pagesize = numa_pagesize();
	unsigned long i; 
	for (i = 0; i < size; i += pagesize)
		asm volatile("" :: "r" (((volatile unsigned char *)mem)[i]));
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
	if (get_mempolicy(&nd, NULL, 0, 0, MPOL_F_NODE) == 0)
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

void numa_set_bind_policy(int strict) 
{ 
	if (strict) 
		bind_policy = MPOL_BIND; 
	else
		bind_policy = MPOL_PREFERRED;
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

static unsigned long *node_cpu_mask[NUMA_NUM_NODES];  

/* This would be better with some locking, but I don't want to make libnuma
   dependent on pthreads right now. The races are relatively harmless. */
int numa_node_to_cpus(int node, unsigned long *buffer, int bufferlen) 
{
	char fn[64];
	FILE *f; 
	char *line = NULL; 
	size_t len = 0; 
	char *s;
	int n;
	int buflen_needed;
	unsigned *mask, prev;
	int ncpus = number_of_cpus();

	buflen_needed = CPU_BYTES(ncpus);
	if ((unsigned)node > maxnode || bufferlen < buflen_needed) { 
		errno = ERANGE;
		return -1;
	}

	if (node_cpu_mask[node]) { 
		if (bufferlen > buflen_needed)
			memset(buffer, 0, bufferlen); 
		memcpy(buffer, node_cpu_mask[node], buflen_needed);
		return 0;
	}

	mask = malloc(buflen_needed);
	if (!mask) 
		mask = (unsigned *)buffer; 
	memset(mask, 0, buflen_needed); 
			
	sprintf(fn, "/sys/devices/system/node/node%d/cpumap", node); 
	f = fopen(fn, "r"); 
	if (!f || getdelim(&line, &len, '\n', f) < 1) { 
		numa_warn(W_nosysfs2,
		   "/sys not mounted or invalid. Assuming nodes equal CPU: %s",
			  strerror(errno)); 
		set_bit(node, (unsigned long *)mask);
		goto out;
	} 
	n = 0;
	s = line;
	prev = 0; 
	while (*s) {
 		unsigned num; 
		int i;
		num = 0;
		for (i = 0; s[i] && s[i] != ','; i++) { 
			static const char hexdigits[] = "0123456789abcdef";
			char *w = strchr(hexdigits, tolower(s[i]));
			if (!w) { 
				if (isspace(s[i]))
					break;
				numa_warn(W_cpumap, 
					  "Unexpected character `%c' in sysfs cpumap", s[i]);
				set_bit(node, mask);
				goto out;
			}
			num = (num*16) + (w - hexdigits); 
		}
		if (i == 0) 
			break; 
		s += i;
		if (*s == ',')
			s++;
		/* skip leading zeros */
		if (num == 0 && prev == 0) 
			continue;
		prev |= num; 
		memmove(mask + 1, mask, buflen_needed - sizeof(unsigned)); 
		mask[0] = num; 
	}
 out:
	free(line);
	fclose(f);
	memcpy(buffer, mask, buflen_needed);

	/* slightly racy, see above */ 
	if (node_cpu_mask[node]) {
		if (mask != (unsigned *)buffer)
			free(mask); 	       
	} else {
		node_cpu_mask[node] = (unsigned long *)mask; 
	} 
	return 0; 
} 

int numa_run_on_node_mask(nodemask_t *mask)
{ 	
	int ncpus = number_of_cpus();
	int i, k;
	unsigned long cpus[CPU_WORDS(ncpus)], nodecpus[CPU_WORDS(ncpus)];
	memset(cpus, 0, CPU_BYTES(ncpus));
	for (i = 0; i < NUMA_NUM_NODES; i++) { 
		if (mask->n[i / BITS_PER_LONG] == 0)
			continue;
		if (nodemask_isset(mask, i)) { 
			if (numa_node_to_cpus(i, nodecpus, CPU_BYTES(ncpus)) < 0) { 
				numa_warn(W_noderunmask, 
					  "Cannot read node cpumask from sysfs");
				continue;
			}
			for (k = 0; k < CPU_WORDS(ncpus); k++)
				cpus[k] |= nodecpus[k];
		}	
	}
	return numa_sched_setaffinity(getpid(), CPU_BYTES(ncpus), cpus);
} 

nodemask_t numa_get_run_node_mask(void)
{ 
	int ncpus = number_of_cpus();
	nodemask_t mask;
	int i, k;
	unsigned long cpus[CPU_WORDS(ncpus)], nodecpus[CPU_WORDS(ncpus)];

	memset(cpus, 0, CPU_BYTES(ncpus));
	nodemask_zero(&mask);
	if (numa_sched_getaffinity(getpid(), CPU_BYTES(ncpus), cpus) < 0) 
		return numa_no_nodes; 
	/* somewhat dumb algorithm */
	for (i = 0; i < NUMA_NUM_NODES; i++) {
		if (numa_node_to_cpus(i, nodecpus, CPU_BYTES(ncpus)) < 0) {
			numa_warn(W_noderunmask, "Cannot read node cpumask from sysfs");
			continue;
		}
		for (k = 0; k < NUMA_NUM_NODES/BITS_PER_LONG; k++) {
			if (nodecpus[k] & cpus[k])
				nodemask_set(&mask, i); 
		}
	}		
	return mask;
} 

int numa_run_on_node(int node)
{ 
	int ncpus = number_of_cpus();
	unsigned long cpus[CPU_WORDS(ncpus)];

	if (node == -1)
		memset(cpus, 0xff, CPU_BYTES(ncpus));
	else if (node < NUMA_NUM_NODES) {
		if (numa_node_to_cpus(node, cpus, CPU_BYTES(ncpus)) < 0) {
			numa_warn(W_noderunmask, "Cannot read node cpumask from sysfs");
			return -1; 
		} 		
	} else { 
		errno = EINVAL;
		return -1; 
	}
	return numa_sched_setaffinity(getpid(), CPU_BYTES(ncpus), cpus);
} 

int numa_preferred(void)
{ 
	int policy;
	nodemask_t nodes;
	getpol(&policy, &nodes);
	if (policy == MPOL_PREFERRED || policy == MPOL_BIND) { 
		int i;
		int max = NUMA_NUM_NODES;
		for (i = 0; i < max ; i++) 
			if (nodemask_isset(&nodes, i))
				return i; 
	}
	/* could read the current CPU from /proc/self/status. Probably 
	   not worth it. */
	return 0; /* or random one? */
}

void numa_set_preferred(int node)
{ 
	nodemask_t n;
	if (node == -1) {
		nodemask_t empty;
		nodemask_zero(&empty);
		setpol(MPOL_DEFAULT, empty);
		return;
	}
	nodemask_zero(&n);
	nodemask_set(&n, node); 
	setpol(MPOL_PREFERRED, n);
} 

void numa_set_localalloc(void) 
{	
	nodemask_t empty;
	nodemask_zero(&empty);
	setpol(MPOL_PREFERRED, empty);
} 

void numa_bind(nodemask_t *nodemask)
{
	numa_run_on_node_mask(nodemask); 
	numa_set_membind(nodemask);
}

void numa_set_strict(int flag)
{
	if (flag)
		mbind_flags |= MPOL_MF_STRICT;
	else
		mbind_flags &= ~MPOL_MF_STRICT;
}
