/* Written 2003 by Andi Kleen.
 * 
 * Per Process NUMA policy control.
 * 
 * Currently assumes nodes equal cpus.
 */
#define _GNU_SOURCE
#include <getopt.h>
#include <errno.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>
#include "numaif.h"
#include "numa.h"
#include "numaint.h"
#include "util.h"

#define err(x) perror("numactl: " x),exit(1)

struct option opts[] = { 
	{"interleave", 1, 0, 'i' },
	{"homenode", 1, 0, 'h' },
	{"prefered", 1, 0, 'p' },
	{"nodebind", 1, 0, 'b' }, 
	{"membind", 1, 0, 'm'},
	{"show", 0, 0, 's' }, 
	{"localalloc", 0,0, 'l'},
	{ 0 }
};

void usage(void)
{
	fprintf(stderr,
		"usage: numactl [--interleave=nodes] [--prefered=node]"
#if 0
		" [--homenode=homenode] [--nodebind=nodes]" 
#endif
		"\n         [--membind=nodes] [--localalloc] command args ...\n");
	fprintf(stderr,	"       numactl [--show]\n");
	fprintf(stderr, "nodes is a comma delimited list of node numbers.\n");
	exit(1);
} 

static char *policies[] = { "default", "prefered", "bind", "interleave" }; 

void show(void)
{
	char buf[16];
	unsigned long homenode;
	nodemask_t membind, interleave, cpubind;
	unsigned long cur;
	int maxnode, policy;
	char *p;

	nodemask_zero(&cpubind);
	if (numa_sched_getaffinity(getpid(), sizeof(cpubind), (unsigned long *)&cpubind) < 0) 
		err("get_affinity");
	if (numa_available() < 0) { 
		printf("No homenode scheduling support available on this system.\n");
		exit(1);
	}

	homenode = numa_homenode();
	interleave = numa_get_interleave_mask();
	membind = numa_get_membind();
	cur = numa_get_interleave_node();
	maxnode = numa_max_node(); 

	policy = 0;
	if (get_mempolicy(&policy, NULL, 0, 0, 0) < 0) 
		perror("get_mempolicy"); 
	if (policy > sizeof(policies)/sizeof(char*)) {
		sprintf(buf, "[%d]", policy); 
		p = buf;
	} else
		p = policies[policy]; 

	printf("policy: %s\n", p);

	printf("available: %d nodes (0-%d)\n", 1+maxnode, maxnode); 
		
	printf("prefered node: %ld\n", homenode);
	printmask("interleavemask", &interleave);
	printf("interleavenode: %ld\n", cur); 
	printmask("nodebind", &cpubind);
	printmask("membind", &membind);
	exit(0);
}

enum mode { 
	OTHER_NONE,
	OTHER_LOCAL,
	OTHER_NUMA
} other = OTHER_NONE; 

void setother(char *s, int local)
{ 
#if 0
	if (other == OTHER_LOCAL && !local) 
		fprintf(stderr, "numactl: %s ignored because of local allocation policy.\n", s); 
	else if (other == OTHER_NUMA && local) 
		fprintf(stderr, "numactl: flags ignored because of local allocation policy");  
	
	other = local ? OTHER_LOCAL : OTHER_NUMA; 
#endif
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
		if (numa_available() < 0) {
			fprintf(stderr, "numactl: this system does not support NUMA policy.\n");	
			exit(1);
		}
	}
	numa = 0;
} 

int main(int ac, char **av)
{ 
	int c;
	long arg; 

	while ((c = getopt_long(ac,av,"+i:h:c:sm:l", opts, NULL)) != -1) { 
		nodemask_t mask;
		switch (c) { 
		case 's':
			show();
			exit(0); 
		case 'i':
			checknuma();
			setother("interleaving",0);
			mask = nodemask(optarg);
			errno = 0;
			numa_set_interleave_mask(&mask);
			checkerror("setting interleave mask");
			break;
		case 'b':
			mask = nodemask(optarg);
			errno = 0;
			numa_run_on_node_mask(&mask);
			checkerror("sched_setaffinity");
			break;
		case 'm':
			checknuma();
			mask = nodemask(optarg); 
			errno = 0;
			numa_set_membind(&mask);
			checkerror("setting membind");
			break;
		case 'p':
		case 'h': 
			checknuma();
			setother("prefered", 0); 
			arg = strtol(optarg,NULL,0); 
			errno = 0;
			numa_set_homenode(arg);
			checkerror("setting prefered node");
			break;
		case 'l': 
			checknuma();
			setother("local allocation", 1);
			errno = 0;
			numa_set_localalloc(1);
			checkerror("local allocation");
			break;
		default:
			usage();
		} 
	}
	
	av += optind;
	ac -= optind; 
	if (*av == NULL)
		usage();
	execvp(*av, av);
	complain("execution of `%s': %s\n", av[0], strerror(errno)); 
	return 0; /* not reached */
}
