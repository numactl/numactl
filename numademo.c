/* test/demo program for libnuma */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "numa.h"
#include "util.h"
#include "rdtsc.h"
#include "stream_int.h"

unsigned long msize; 

enum test { 
	MEMSET = 0,
	MEMCPY,
	FORWARD,
	BACKWARD,
	STREAM,
} thistest;

char *delim = " ";
int force;

char *testname[] = { 
	"memset",
	"memcpy",
	"forward",
	"backward",
	"stream",
	NULL,
}; 

void do_stream(char *name, unsigned char *mem)
{
	int i;
	char title[100];
	double res[STREAM_NRESULTS];
	stream_verbose = 0;
	stream_init(mem); 
	stream_test(res);
	sprintf(title, "%s%s%s", name, delim, "STREAM");
	printf("%-30s", title);
	for (i = 0; i < STREAM_NRESULTS; i++) 
		printf("%s%s%s%.2f", delim, stream_names[i], delim, res[i]); 
	printf("%sMB/s\n", delim);
}

#define LOOPS 10

void memtest(char *name, unsigned char *mem)
{ 
	long k;
	unsigned long start, end, max, min, sum; 
	int i;
	char title[128], result[128];

	if (thistest == STREAM) { 
		do_stream(name, mem);
		goto out;
	}
	
	max = 0; 
	min = ~0UL; 
	sum = 0;
	for (i = 0; i < LOOPS; i++) { 
		switch (thistest) { 
		case MEMSET:
			rdtsc(start);  
			memset(mem, 0xff, msize); 
			rdtsc(end); 
		case MEMCPY: 
			rdtsc(start);  
			memcpy(mem, mem + msize/2, msize/2); 
			rdtsc(end); 
			break;

		case FORWARD: 
			/* simple kernel to just fetch cachelines and write them back. 
			   will trigger hardware prefetch */ 
			rdtsc(start);  
			for (k = 0; k < msize; k+=64) 
				mem[k]++;
			rdtsc(end); 
			break;

		case BACKWARD: 
			/* simple kernel to avoid hardware prefetch */ 
			rdtsc(start);  
			for (k = msize-5; k > 0; k-=64) 
				mem[k]--;
			rdtsc(end);
			break;

		default:
			start = end = 0; /* just to quiten gcc */
			break;
		} 
		if ((end-start) > max) max = end-start;
		if ((end-start) < min) min = end-start;
		sum += (end-start); 
	} 
	sprintf(title, "%s%s%s", name, delim, testname[thistest]); 
	sprintf(result, "avg%s%lu%smin%s%lu%smax%s%lu%scycles/KB",
		delim,
		(sum / LOOPS) / (msize/1024), 
		delim,
		delim,
		min / (msize/1024), 
		delim,
		delim,
		max / (msize/1024),
		delim);
	if (!isspace(delim[0]))
		printf("%s%s%s\n", title,delim, result); 
	else
		printf("%-30s%35s\n", title, result); 

 out:
	numa_free(mem, msize); 
} 

int popcnt(unsigned long val)
{ 
	int i = 0, cnt = 0;
	while (val >> i) { 
		if ((1UL << i) & val) 
			cnt++; 
		i++;
	} 
	return cnt;
} 

int max_node;

void test(enum test type)
{ 
	unsigned long mask;
	int i, k;
	char buf[512];
	nodemask_t nodes;

	thistest = type; 

	memtest("memory with no policy", numa_alloc(msize));
	memtest("local memory", numa_alloc_local(msize));

	memtest("memory interleaved on all nodes", numa_alloc_interleaved(msize)); 
	for (i = 0; i <= max_node; i++) { 
		sprintf(buf, "memory on node %d", i); 
		memtest(buf, numa_alloc_onnode(msize, i)); 
	} 
	
	for (mask = 1; mask < (1UL<<(max_node+1)); mask++) { 
		int w;
		char buf2[10];
		if (popcnt(mask) == 1) 
			continue;
		nodemask_zero(&nodes); 
		for (w = 0; mask >> w; w++) { 
			if ((mask >> w) & 1)
				nodemask_set(&nodes, w); 
		} 

		sprintf(buf, "memory interleaved on"); 
		for (k = 0; k <= max_node; k++) 
			if ((1UL<<k) & mask) {
				sprintf(buf2, " %d", k);
				strcat(buf, buf2);
			}
		memtest(buf, numa_alloc_interleaved_subset(msize, &nodes)); 
	}

	for (i = 0; i <= max_node; i++) { 
		printf("setting preferred node to %d\n", i);
		numa_set_preferred(i); 
		memtest("memory without policy", numa_alloc(msize)); 
	} 

	numa_set_interleave_mask(&numa_all_nodes); 
	memtest("manual interleaving to all nodes", numa_alloc(msize)); 

	if (max_node > 0) { 
		nodemask_zero(&nodes); 
		nodemask_set(&nodes, 0);
		nodemask_set(&nodes, 1);
		numa_set_interleave_mask(&nodes); 
		memtest("manual interleaving on node 0/1", numa_alloc(msize)); 
		printf("current interleave node %d\n", numa_get_interleave_node()); 
	} 

	numa_set_interleave_mask(&numa_no_nodes); 

	for (i = 0; i <= max_node; i++) { 
		int oldhn = numa_preferred();

		numa_run_on_node(i); 
		printf("running on node %d, preferred node %d\n",i, oldhn);

		memtest("local memory", numa_alloc_local(msize));

		memtest("memory interleaved on all nodes", 
			numa_alloc_interleaved(msize)); 

		if (max_node >= 1) { 
			nodemask_zero(&nodes);
			nodemask_set(&nodes, 0);
			nodemask_set(&nodes, 1);
			memtest("memory interleaved on node 0/1", 
				numa_alloc_interleaved_subset(msize, &nodes)); 
		} 

		for (k = 0; k <= max_node; k++) { 
			if (k == i) 
				continue;
			sprintf(buf, "alloc on node %d", k);
			nodemask_zero(&nodes);
			nodemask_set(&nodes, k); 
			numa_set_membind(&nodes); 
			memtest(buf, numa_alloc(msize)); 			
			numa_set_membind(&numa_all_nodes);
		}
		
		numa_set_localalloc(); 
		memtest("local allocation", numa_alloc(msize)); 

		numa_set_preferred((i+1) % (1+max_node)); 
		memtest("setting wrong preferred node", numa_alloc(msize)); 
		numa_set_preferred(i); 
		memtest("setting correct preferred node", numa_alloc(msize)); 
		numa_set_preferred(-1); 
		printf("\n\n\n"); 
	} 

	/* numa_run_on_node_mask is not tested */
} 

void usage(void)
{
	int i;
	printf("usage: numademo [-f] [-c] msize[kmg] {tests}\nNo tests means run all.\n"); 
	printf("-c output CSV data. -f run even without NUMA API.\n");  
	printf("valid tests:"); 
	for (i = 0; testname[i]; i++) 
		printf(" %s", testname[i]); 
	putchar('\n');
	exit(1);
}

int main(int ac, char **av)
{
	while (av[1] && av[1][0] == '-') { 
		switch (av[1][1]) { 
		case 'c': 
			delim = ","; 
			break;
		case 'f': 
			force = 1;
			break;
		default:
			usage(); 
			break; 
		} 
		++av; 
	} 

	if (!av[1]) 
		usage(); 

	if (numa_available() < 0) { 
		printf("your system does not support the numa API.\n");
		if (!force)
			exit(1);
	}

	max_node = numa_max_node(); 
	printf("%d nodes available\n", max_node+1); 

	msize = memsize(av[1]); 

	stream_setmem(msize);

	if (av[2] == NULL) { 
		test(MEMSET); 
		test(MEMCPY);
#if 0
		test(FORWARD);
		test(BACKWARD);
#endif
		test(STREAM); 
	} else {
		int k;
		for (k = 2; k < ac; k++) { 
			int i;
			int found = 0;
			for (i = 0; testname[i]; i++) { 
				if (!strcmp(testname[i],av[k])) { 
					test(i); 
					found = 1;
					break;
				}
			} 
			if (!found) { 
				fprintf(stderr,"unknown test `%s'\n", av[k]);
				usage();
			}
		}
	} 
	return 0;
}
