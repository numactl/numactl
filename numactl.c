/* Copyright (C) 2003,2004,2005 Andi Kleen, SuSE Labs.
   Command line NUMA policy control.

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
#define _GNU_SOURCE
#include <getopt.h>
#include <errno.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include "numaif.h"
#include "numa.h"
#include "numaint.h"
#include "util.h"
#include "shm.h"

int exitcode;

struct option opts[] = { 
	{"interleave", 1, 0, 'i' },
	{"preferred", 1, 0, 'p' },
	{"cpubind", 1, 0, 'c' }, 
	{"cpunodebind", 1, 0, 'N' }, 
	{"physcpubind", 1, 0, 'C' }, 
	{"membind", 1, 0, 'm'},
	{"show", 0, 0, 's' }, 
	{"localalloc", 0,0, 'l'},
	{"hardware", 0,0,'H' },

	{"shm", 1, 0, 'S'},
	{"file", 1, 0, 'f'},
	{"offset", 1, 0, 'o'},
	{"length", 1, 0, 'L'},
	{"strict", 0, 0, 't'},
	{"shmmode", 1, 0, 'M'},
	{"dump", 0, 0, 'd'},
	{"shmid", 1, 0, 'I'},
	{"huge", 0, 0, 'u'},
	{"touch", 0, 0, 'T'},
	{"verify", 0, 0, 'V'}, /* undocumented - for debugging */
	{ 0 }
};

void usage(void)
{
	fprintf(stderr,
		"usage: numactl [--interleave=nodes] [--preferred=node]\n"
		"               [--physcpubind=cpus] [--cpunodebind=nodes]\n"
		"               [--membind=nodes] [--localalloc] command args ...\n"
		"       numactl [--show]\n"
		"       numactl [--hardware]\n"
		"       numactl [--length length] [--offset offset] [--shmmode shmmode]\n"
		"               [--strict]\n"
		"               --shm shmkeyfile | --file tmpfsfile | --shmid id\n"
		"               [--huge] [--touch]\n" 
		"               memory policy\n"
		"\n"
		"memory policy is --interleave, --preferred, --membind, --localalloc\n"
		"nodes is a comma delimited list of node numbers or A-B ranges or all.\n"
		"cpus is a comma delimited list of cpu numbers or A-B ranges or all\n"
		"all ranges can be inverted with !\n"
		"the old --cpubind argument is deprecated.\n"
		"use --cpunodebind or --physcpubind instead\n"
		"length can have g (GB), m (MB) or k (KB) suffixes\n");
	exit(1);
} 

void usage_msg(char *msg, ...) 
{ 
	va_list ap;
	va_start(ap,msg);
	fprintf(stderr, "numactl: "); 
	vfprintf(stderr, msg, ap);
	putchar('\n');
	usage();
} 

void show_physcpubind(void)
{
	int ncpus = 8192;
	
	for (;;) { 
		int cpubufsize = round_up(ncpus, BITS_PER_LONG) / BYTES_PER_LONG;
		unsigned  long cpubuf[cpubufsize / sizeof(long)];
		
		memset(cpubuf,0,cpubufsize);
		if (numa_sched_getaffinity(0, cpubufsize, cpubuf) < 0) { 
			if (errno == EINVAL && ncpus < 1024*1024) {
				ncpus *= 2; 
				continue;
			}
			err("sched_get_affinity");
		}
		printcpumask("physcpubind", cpubuf, cpubufsize);
		break;
	}
}

void show(void)
{
	unsigned long prefnode;
	nodemask_t membind, interleave, cpubind;
	unsigned long cur;
	int policy;
	
	if (numa_available() < 0) { 
		show_physcpubind();
		printf("No NUMA support available on this system.\n");
		exit(1);
	}

	cpubind = numa_get_run_node_mask();

	prefnode = numa_preferred();
	interleave = numa_get_interleave_mask();
	membind = numa_get_membind();
	cur = numa_get_interleave_node();

	policy = 0;
	if (get_mempolicy(&policy, NULL, 0, 0, 0) < 0) 
		perror("get_mempolicy"); 
	 
	printf("policy: %s\n", policy_name(policy));
		
	printf("preferred node: ");
	switch (policy) { 
	case MPOL_PREFERRED:
		if (prefnode != -1) { 
			printf("%ld\n", prefnode);
			break;
		}
		/*FALL THROUGH*/
	case MPOL_DEFAULT:
		printf("current\n");
		break;
	case MPOL_INTERLEAVE:
		printf("%ld (interleave next)\n",cur); 
		break; 
	case MPOL_BIND:
		printf("%d\n", find_first_bit(&membind, NUMA_NUM_NODES)); 
		break;
	} 
	if (policy == MPOL_INTERLEAVE) {
		printmask("interleavemask", &interleave);
		printf("interleavenode: %ld\n", cur); 
	}
	show_physcpubind();
	printmask("cpubind", &cpubind);  // for compatibility
	printmask("nodebind", &cpubind);
	printmask("membind", &membind);
}

char *fmt_mem(unsigned long long mem, char *buf) 
{ 
	if (mem == -1L)
		sprintf(buf, "<not available>"); 
	else
		sprintf(buf, "%Lu MB", mem >> 20); 
	return buf;
} 

static void print_distances(int maxnode)
{
	int i,k;

	if (numa_distance(maxnode,0) == 0) {
		printf("No distance information available.\n");
		return;
	}
	printf("node distances:\n"); 
	printf("node ");
	for (i = 0; i <= maxnode; i++) 
		printf("% 3d ", i);
	printf("\n");
	for (i = 0; i <= maxnode; i++) {
		printf("% 3d: ", i);
		for (k = 0; k <= maxnode; k++) 
			printf("% 3d ", numa_distance(i,k)); 
		printf("\n");
	}			
}

void print_node_cpus(int node)
{
	int len = 1;
	for (;;) { 
		int i;
		unsigned long cpus[len];
		errno = 0;
		int err = numa_node_to_cpus(node, cpus, len * sizeof(long));
		if (err < 0) {
			if (errno == ERANGE) {
				len *= 2; 
				continue;
			}
			break; 
		}
		for (i = 0; i < len*BITS_PER_LONG; i++) 
			if (test_bit(i, cpus))
				printf(" %d", i);
		break;
	}
	putchar('\n');
}

void hardware(void)
{ 
	int i;
	int maxnode = numa_max_node(); 
	printf("available: %d nodes (0-%d)\n", 1+maxnode, maxnode); 	
	for (i = 0; i <= maxnode; i++) { 
		char buf[64];
		long long fr;
		unsigned long long sz = numa_node_size64(i, &fr); 
		printf("node %d cpus:", i);
		print_node_cpus(i);
		printf("node %d size: %s\n", i, fmt_mem(sz, buf));
		printf("node %d free: %s\n", i, fmt_mem(fr, buf));
	}
	print_distances(maxnode);
} 

void checkerror(char *s)
{
	if (errno) {
		perror(s);
		exit(1);
	}
} 

void checknuma(void)
{ 
	static int numa = -1;
	if (numa < 0) {
		if (numa_available() < 0)
			complain("This system does not support NUMA policy");
	}
	numa = 0;
} 

int set_policy = -1;

void setpolicy(int pol)
{
	if (set_policy != -1)
		usage_msg("Conflicting policies");
	set_policy = pol;
}

void nopolicy(void)
{ 
	if (set_policy >= 0)
		usage_msg("specify policy after --shm/--file");
} 

int did_cpubind = 0;
int did_strict = 0;
int do_shm = 0;
int do_dump = 0;
int shmattached = 0;
char *shmoption;

void check_cpubind(int flag)
{
	if (flag)
		usage_msg("cannot do --cpubind on shared memory\n");
}

void noshm(char *opt)
{ 
	if (shmattached)
		usage_msg("%s must be before shared memory specification", opt);
	shmoption = opt;		
} 

void dontshm(char *opt) 
{
	if (shmoption)
		usage_msg("%s shm option is not allowed before %s", shmoption, opt);
}

void needshm(char *opt)
{ 
	if (!shmattached)
		usage_msg("%s must be after shared memory specification", opt);
} 

void get_short_opts(struct option *o, char *s)
{
	while (o->name) { 
		if (isprint(o->val)) {
			*s++ = o->val;
			if (o->has_arg) 
				*s++ = ':';
		}
		o++;
	}
	*s = '\0';
}

int main(int ac, char **av)
{ 
	int c;
	long arg; 
	char *end;
	char shortopts[array_len(opts)*2 + 1];
	get_short_opts(opts,shortopts);
	while ((c = getopt_long(ac, av, shortopts, opts, NULL)) != -1) { 
		nodemask_t mask;
		switch (c) { 
		case 's': /* --show */
			show();
			exit(0);  
		case 'H': /* --hardware */
			nopolicy();
			hardware();
			exit(0);
		case 'i': /* --interleave */
			checknuma();
			mask = nodemask(optarg);
			errno = 0;
			setpolicy(MPOL_INTERLEAVE);
			if (shmfd >= 0)
				numa_interleave_memory(shmptr, shmlen, &mask);
			else
				numa_set_interleave_mask(&mask);
			checkerror("setting interleave mask");
			break;
		case 'N': /* --cpunodebind */
		case 'c': /* --cpubind */
			dontshm("-c/--cpubind/--cpunodebind");
			checknuma();
			mask = nodemask(optarg);
			errno = 0;
			check_cpubind(do_shm);
			did_cpubind = 1;
			numa_run_on_node_mask(&mask);
			checkerror("sched_setaffinity");
			break;
		case 'C': /* --physcpubind */
		{
			int ncpus;
			unsigned long *cpubuf;
			dontshm("-C/--physcpubind");
			cpubuf = cpumask(optarg, &ncpus);
			errno = 0;
			check_cpubind(do_shm);
			did_cpubind = 1;
			numa_sched_setaffinity(0, CPU_BYTES(ncpus), cpubuf);
			checkerror("sched_setaffinity");
			free(cpubuf);
			break;
		}
		case 'm': /* --membind */
			checknuma();
			setpolicy(MPOL_BIND);
			mask = nodemask(optarg); 
			errno = 0;
			numa_set_bind_policy(1);
			if (shmfd >= 0) { 
				numa_tonodemask_memory(shmptr, shmlen, &mask);
			} else {
				numa_set_membind(&mask);
			}
			numa_set_bind_policy(0);
			checkerror("setting membind");
			break;
		case 'p': /* --preferred */
			checknuma();
			setpolicy(MPOL_PREFERRED);
			arg = strtoul(optarg,&end,0); 
			if (*end || end == optarg || arg < 0 || arg > numa_max_node()) 
				usage();
			errno = 0;
			numa_set_bind_policy(0);
			nodemask_zero(&mask);
			nodemask_set(&mask, arg);
			numa_set_bind_policy(0);
			if (shmfd >= 0) 
				numa_tonode_memory(shmptr, shmlen, arg);
			else
				numa_set_preferred(arg);
			checkerror("setting preferred node");
			break;
		case 'l': /* --local */
			checknuma();
			setpolicy(MPOL_DEFAULT);
			errno = 0;
			if (shmfd >= 0) 
				numa_setlocal_memory(shmptr, shmlen);
			else
				numa_set_localalloc();
			checkerror("local allocation");
			break;
		case 'S': /* --shm */
			check_cpubind(did_cpubind);
			nopolicy();
			attach_sysvshm(optarg); 
			shmattached = 1;
			break;
		case 'f': /* --file */
			check_cpubind(did_cpubind);
			nopolicy();
			attach_shared(optarg);
			shmattached = 1;
			break;
		case 'L': /* --length */
			noshm("--length");
			shmlen = memsize(optarg);
			break;
		case 'M': /* --shmmode */
			noshm("--shmmode");
			shmmode = strtoul(optarg, &end, 8);
			if (end == optarg || *end) 
				usage();
			break;
		case 'd': /* --dump */
			if (shmfd < 0)
				complain("Cannot do --dump without shared memory.\n");
			dump_shm();
			do_dump = 1;
			break;
		case 't': /* --strict */
			did_strict = 1;
			numa_set_strict(1);
			break;
		case 'I': /* --shmid */
			shmid = strtoul(optarg, &end, 0);
			if (end == optarg || *end) 
				usage();
			break;

		case 'u': /* --huge */
			noshm("--huge");
			shmflags |= SHM_HUGETLB;
			break;

		case 'o':  /* --offset */
			noshm("--offset");
			shmoffset = memsize(optarg);
			break;			

		case 'T': /* --touch */
			needshm("--touch");
			numa_police_memory(shmptr, shmlen);
			break;

		case 'V': /* --verify */
			needshm("--verify");
			if (set_policy < 0) 
				complain("Need a policy first to verify");
			numa_police_memory(shmptr, shmlen); 
			verify_shm(set_policy, mask);
			break;

		default:
			usage();
		} 
	}
	
	av += optind;
	ac -= optind; 

	if (shmfd >= 0) { 
		if (*av)
			usage();
		exit(exitcode);
	}

	if (did_strict)
		fprintf(stderr,"numactl: warning. Strict flag for process ignored.\n");

	if (do_dump)
		usage_msg("cannot do --dump for process");

	if (shmoption)
		usage_msg("shm related option %s for process", shmoption);
	
	if (*av == NULL)
		usage();
	execvp(*av, av);
	complain("execution of `%s': %s\n", av[0], strerror(errno)); 
	return 0; /* not reached */
}
