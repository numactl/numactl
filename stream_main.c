#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "numa.h"
#include "numaif.h"
#include "util.h"
#include "stream_lib.h"

void usage(void)
{
	exit(1);
}

char *policy = "default";

/* Run STREAM with a numa policy */
int main(int ac, char **av)
{
	struct bitmask *nodes;
	char *map;
	long size;
	int policy;

	policy = parse_policy(av[1], av[2]); 

        nodes = allocate_nodemask();

	if (av[1] && av[2])
		nodes = nodemask(av[2]);
	size = stream_memsize();  
	map = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
		   0, 0); 
	if (map == (char*)-1) exit(1); 
	if (mbind(map, size, policy, nodes->maskp, nodes->size, 0) < 0)
		perror("mbind"), exit(1);
	stream_init(map); 
	stream_test(NULL);
	return 0;
}
