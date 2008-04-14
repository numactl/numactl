/* Copyright (C) 2003,2004 Andi Kleen, SuSE Labs.
   Test/demo program for libnuma. This is also a more or less useful benchmark
   of the NUMA characteristics of your machine. It benchmarks most possible 
   NUMA policy memory configurations with various benchmarks.
   Compile standalone with cc -O2 numademo.c -o numademo -lnuma -lm

   numactl is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; version
   2.

   numactl is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should find a copy of v2 of the GNU General Public License somewhere
   on your Linux system; if not, write to the Free Software Foundation, 
   Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#include "numa.h"
#ifdef HAVE_STREAM_LIB
#include "stream_lib.h"
#endif
#ifdef HAVE_MT
#include "mt.h"
#endif

unsigned long msize; 

/* Should get this from cpuinfo, but on !x86 it's not there */
enum { 
	CACHELINESIZE = 64,
}; 

enum test { 
	MEMSET = 0,
	MEMCPY,
	FORWARD,
	BACKWARD,
	STREAM,
	RANDOM,
} thistest;

char *delim = " ";
int force;

char *testname[] = { 
	"memset",
	"memcpy",
	"forward",
	"backward",
#ifdef HAVE_STREAM_LIB
	"stream",
#endif
#ifdef HAVE_MT
	"random",
#endif
	NULL,
}; 

void output(char *title, char *result)
{
	if (!isspace(delim[0]))
		printf("%s%s%s\n", title,delim, result); 
	else
		printf("%-42s%s\n", title, result); 
}


#ifdef HAVE_STREAM_LIB
void do_stream(char *name, unsigned char *mem)
{
	int i;
	char title[100], buf[100];
	double res[STREAM_NRESULTS];
	stream_verbose = 0;
	stream_init(mem); 
	stream_test(res);
	sprintf(title, "%s%s%s", name, delim, "STREAM");
	buf[0] = '\0';
	for (i = 0; i < STREAM_NRESULTS; i++) { 
		if (buf[0])
			strcat(buf,delim);
		sprintf(buf+strlen(buf), "%s%s%.2f%sMB/s", 
			stream_names[i], delim, res[i], delim);
	}
	output(title, buf);
}
#endif


static inline unsigned long long timerfold(struct timeval *tv)
{
	return tv->tv_sec * 1000000ULL + tv->tv_usec;
}

#define LOOPS 10

void memtest(char *name, unsigned char *mem)
{ 
	long k;
#ifdef HAVE_MT
	long w;
#endif
	struct timeval start, end, res;
	unsigned long long max, min, sum, r; 
	int i;
	char title[128], result[128];

#ifdef HAVE_STREAM_LIB
	if (thistest == STREAM) { 
		do_stream(name, mem);
		goto out;
	}
#endif
	
	max = 0; 
	min = ~0UL; 
	sum = 0;
	for (i = 0; i < LOOPS; i++) { 
		switch (thistest) { 
		case MEMSET:
			gettimeofday(&start,NULL);
			memset(mem, 0xff, msize); 
			gettimeofday(&end,NULL);
			break;

		case MEMCPY: 
			gettimeofday(&start,NULL);
			memcpy(mem, mem + msize/2, msize/2); 
			gettimeofday(&end,NULL);
			break;

		case FORWARD: 
			/* simple kernel to just fetch cachelines and write them back. 
			   will trigger hardware prefetch */ 
			gettimeofday(&start,NULL);
			for (k = 0; k < msize; k+=CACHELINESIZE) 
				mem[k]++;
			gettimeofday(&end,NULL);
			break;

		case BACKWARD: 
			gettimeofday(&start,NULL);
			for (k = msize-5; k > 0; k-=CACHELINESIZE) 
				mem[k]--;
			gettimeofday(&end,NULL);
			break;

#ifdef HAVE_MT
		case RANDOM: 
			mt_init(); 
			/* only use power of two msize to avoid division */
			for (w = 1; w < msize; w <<= 1)
				;
			if (w > msize) 
				w >>= 1;
			w--;
			gettimeofday(&start,NULL);
			for (k = msize; k > 0; k -= 8) 
				mem[mt_random() & w]++;
			gettimeofday(&end,NULL);
			break;
#endif

		default:
			break;
		} 

		timersub(&end, &start, &res);
		r = timerfold(&res); 
		if (r > max) max = r;
		if (r < min) min = r;
		sum += r; 
	} 
	sprintf(title, "%s%s%s", name, delim, testname[thistest]); 
#define H(t) (((double)msize) / ((double)t))
#define D3 delim,delim,delim
	sprintf(result, "Avg%s%.2f%sMB/s%sMin%s%.2f%sMB/s%sMax%s%.2f%sMB/s",
		delim,
		H(sum/LOOPS), 
		D3,
		H(min), 
		D3,
		H(max),
		delim);
#undef H
#undef D3
	output(title,result);

#ifdef HAVE_STREAM_LIB
 out:
#endif
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
	struct bitmask *nodes;

	nodes = numa_allocate_nodemask();
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
		numa_bitmask_clearall(nodes);
		for (w = 0; mask >> w; w++) { 
			if ((mask >> w) & 1)
				numa_bitmask_setbit(nodes, w);
		} 

		sprintf(buf, "memory interleaved on"); 
		for (k = 0; k <= max_node; k++) 
			if ((1UL<<k) & mask) {
				sprintf(buf2, " %d", k);
				strcat(buf, buf2);
			}
		memtest(buf, numa_alloc_interleaved_subset(msize, nodes));
	}

	for (i = 0; i <= max_node; i++) { 
		printf("setting preferred node to %d\n", i);
		numa_set_preferred(i); 
		memtest("memory without policy", numa_alloc(msize)); 
	} 

	numa_set_interleave_mask(numa_all_nodes_ptr);
	memtest("manual interleaving to all nodes", numa_alloc(msize)); 

	if (max_node > 0) { 
		numa_bitmask_clearall(nodes);
		numa_bitmask_setbit(nodes, 0);
		numa_bitmask_setbit(nodes, 1);
		numa_set_interleave_mask(nodes);
		memtest("manual interleaving on node 0/1", numa_alloc(msize)); 
		printf("current interleave node %d\n", numa_get_interleave_node()); 
	} 

	numa_set_interleave_mask(numa_no_nodes_ptr);

	nodes = numa_allocate_nodemask();

	for (i = 0; i <= max_node; i++) { 
		int oldhn = numa_preferred();

		numa_run_on_node(i); 
		printf("running on node %d, preferred node %d\n",i, oldhn);

		memtest("local memory", numa_alloc_local(msize));

		memtest("memory interleaved on all nodes", 
			numa_alloc_interleaved(msize)); 

		if (max_node >= 1) { 
			numa_bitmask_clearall(nodes);
			numa_bitmask_setbit(nodes, 0);
			numa_bitmask_setbit(nodes, 1);
			memtest("memory interleaved on node 0/1", 
				numa_alloc_interleaved_subset(msize, nodes));
		} 

		for (k = 0; k <= max_node; k++) { 
			if (k == i) 
				continue;
			sprintf(buf, "alloc on node %d", k);
			numa_bitmask_clearall(nodes);
			numa_bitmask_setbit(nodes, k);
			numa_set_membind(nodes);
			memtest(buf, numa_alloc(msize)); 			
			numa_set_membind(numa_all_nodes_ptr);
		}
		
		numa_set_localalloc(); 
		memtest("local allocation", numa_alloc(msize)); 

		numa_set_preferred((i+1) % (1+max_node)); 
		memtest("setting wrong preferred node", numa_alloc(msize)); 
		numa_set_preferred(i); 
		memtest("setting correct preferred node", numa_alloc(msize)); 
		numa_set_preferred(-1); 
		if (!delim[0])
			printf("\n\n\n"); 
	} 

	/* numa_run_on_node_mask is not tested */
} 

void usage(void)
{
	int i;
	printf("usage: numademo [-S] [-f] [-c] msize[kmg] {tests}\nNo tests means run all.\n"); 
	printf("-c output CSV data. -f run even without NUMA API. -S run stupid tests\n");  
	printf("power of two msizes prefered\n"); 
	printf("valid tests:"); 
	for (i = 0; testname[i]; i++) 
		printf(" %s", testname[i]); 
	putchar('\n');
	exit(1);
}

/* duplicated to make numademo standalone */
long memsize(char *s) 
{ 
	char *end;
	long length = strtoul(s,&end,0);
	switch (toupper(*end)) { 
	case 'G': length *= 1024;  /*FALL THROUGH*/
	case 'M': length *= 1024;  /*FALL THROUGH*/
	case 'K': length *= 1024; break;
	}
	return length; 
} 

int main(int ac, char **av)
{
	int simple_tests = 0;
	
	while (av[1] && av[1][0] == '-') { 
		switch (av[1][1]) { 
		case 'c': 
			delim = ","; 
			break;
		case 'f': 
			force = 1;
			break;
		case 'S':
			simple_tests = 1;
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

	if (!msize)
		usage();

#ifdef HAVE_STREAM_LIB
	stream_setmem(msize);
#endif

	if (av[2] == NULL) { 
		test(MEMSET); 
		test(MEMCPY);
		if (simple_tests) { 
			test(FORWARD);
			test(BACKWARD);
		}
#ifdef HAVE_MT
		test(RANDOM);
#endif
#ifdef HAVE_STREAM_LIB
		test(STREAM); 
#endif
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
