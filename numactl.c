/* Written 2003,2004 by Andi Kleen, SuSE Labs.
 * 
 * Command line NUMA policy control.
 */
#define _GNU_SOURCE
#include <getopt.h>
#include <errno.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
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
		"usage: numactl [--interleave=nodes] [--preferred=node] [--cpubind=nodes]\n" 
		"               [--membind=nodes] [--localalloc] command args ...\n"
		"       numactl [--show]\n"
		"       numactl [--hardware]\n"
		"       numactl [--length length] [--offset offset] [--mode shmmode] [--strict]\n"
		"               --shm shmkeyfile | --file tmpfsfile | --shmid id\n"
		"               [--huge] [--touch]\n" 
		"               memory policy\n"
		"\n"
		"memory policy is --interleave, --preferred, --membind, --localalloc\n"
		"nodes is a comma delimited list of node numbers or none/all.\n"
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

void show(void)
{
	unsigned long prefnode;
	nodemask_t membind, interleave, cpubind;
	unsigned long cur;
	int policy;

	nodemask_zero(&cpubind);
	if (numa_sched_getaffinity(getpid(), sizeof(cpubind), 
				   (unsigned long *)&cpubind) < 0) 
		err("get_affinity");
	if (numa_available() < 0) { 
		printf("No NUMA support available on this system.\n");
		exit(1);
	}

	prefnode = numa_preferred();
	interleave = numa_get_interleave_mask();
	membind = numa_get_membind();
	cur = numa_get_interleave_node();

	policy = 0;
	if (get_mempolicy(&policy, NULL, 0, 0, 0) < 0) 
		perror("get_mempolicy"); 
	 
	printf("policy: %s\n", policy_name(policy));
		
	printf("preferred node: %ld\n", prefnode);
	printmask("interleavemask", &interleave);
	printf("interleavenode: %ld\n", cur); 
	printmask("nodebind", &cpubind);
	printmask("membind", &membind);
}

char *fmt_mem(unsigned long mem, char *buf) 
{ 
	if (mem == -1L)
		sprintf(buf, "<not available>"); 
	else
		sprintf(buf, "%lu MB", mem >> 20); 
	return buf;
} 

void hardware(void)
{ 
	int i;
	int maxnode = numa_max_node(); 
	printf("available: %d nodes (0-%d)\n", 1+maxnode, maxnode); 	
	for (i = 0; i <= maxnode; i++) { 
		char buf[64];
		unsigned long fr;
		unsigned long sz = numa_node_size(i, &fr); 
		printf("node %d size: %s\n", i, fmt_mem(sz, buf));
		printf("node %d free: %s\n", i, fmt_mem(fr, buf));
	}
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

void needshm(char *opt)
{ 
	if (!shmattached)
		usage_msg("%s must be after shared memory specification", opt);
} 

int main(int ac, char **av)
{ 
	int c;
	long arg; 
	char *end;

	while ((c = getopt_long(ac,av,"+i:h:c:p:sm:l", opts, NULL)) != -1) { 
		nodemask_t mask;
		switch (c) { 
		case 's': /* --show */
			show();
			exit(0);  
		case 'H': /* --hardware */
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
		case 'c': /* --cpubind */
			mask = nodemask(optarg);
			errno = 0;
			check_cpubind(do_shm);
			did_cpubind = 1;
			numa_run_on_node_mask(&mask);
			checkerror("sched_setaffinity");
			break;
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
			arg = strtoul(optarg,NULL,0); 
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
			if (end == optarg) 
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
			noshm("--shmid");
			shmid = strtoul(optarg, &end, 0);
			if (end == optarg) 
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
