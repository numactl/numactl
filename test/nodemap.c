#include "numa.h"
#include "bitops.h"
#include <stdio.h>
#include <stdlib.h>

#define NCPUS 4096

int main(void)
{
	int maxnode, i, k, w;
	if (numa_available() < 0)  {
		printf("no numa\n");
		exit(1);
	}
	maxnode = numa_max_node();
	for (i = 0; i <= maxnode ; i++) {
		unsigned long cpus[NCPUS / sizeof(long)*8];
		if (numa_node_to_cpus(i, cpus, sizeof(cpus)) < 0) {
			printf("node %d failed to convert\n",i); 
		}		
		printf("%d: ", i); 
		w = 0;
		for (k = 0; k < NCPUS; k++)
			if (test_bit(k, cpus)) 
				printf("%s%d", w>0?",":"", k); 
		putchar('\n');		
	}
	return 0;
}
