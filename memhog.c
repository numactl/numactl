#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include "numa.h"
#include "numaif.h" 
#include "util.h"
#include "stream_int.h"

#define terr(x) perror(x)

enum { 
	UNIT = 10*1024*1024,
}; 


void usage(void)
{ 
	printf("memhog size[kmg] [policy [nodeset]]\n");
	print_policies(); 
	exit(1); 
} 

long length; 

void hog(void *map) 
{ 
	long i;
	for (i = 0;  i < length; i += UNIT) { 
		long left = length - i; 
		if (left > UNIT) 
			left = UNIT; 
		putchar('.'); 
		fflush(stdout); 
		memset(map + i, 0xff, left); 
	} 
	putchar('\n');
}

int main(int ac, char **av) 
{ 
	char *map; 
	nodemask_t nodes, gnodes;
	int policy, gpolicy;
	int ret = 0;
	int loose = 0; 

	nodemask_zero(&nodes); 
	
	if (!av[1]) usage();

	length = memsize(av[1]);
	if (av[2] && numa_available() < 0) {
		printf("Kernel doesn't support NUMA policy\n"); 
		exit(1);
	} else
		loose = 1;
	policy = parse_policy(av[2], av[3]);
	if (policy != MPOL_DEFAULT)
		nodes = nodemask(av[3]);

	map = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
		   0, 0); 
	if (map == (char*)-1) 
		err("mmap");
	
	if (mbind(map, length, policy, nodes.n, NUMA_NUM_NODES + 1, 0) < 0) 
		terr("mbind");
	
	gpolicy = -1; 
	if (get_mempolicy(&gpolicy, gnodes.n, NUMA_NUM_NODES + 1, map,
			  MPOL_F_ADDR) < 0)
		terr("get_mempolicy");
	if (!loose && policy != gpolicy) {
		ret = 1;
		printf("policy %d gpolicy %d\n", policy, gpolicy); 
	}
	if (!loose && !nodemask_equal(&gnodes, &nodes)) { 
		printf("nodes differ %lx, %lx!\n", gnodes.n[0], nodes.n[0]); 
		ret = 1;
	}

	hog(map); 
	exit(ret);
}
