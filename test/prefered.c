/* Test prefer policy */
#include "numaif.h"
#include <sys/mman.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <numa.h>

#define err(x) perror(x),exit(1)

int main(void)
{
	int max = numa_max_node();
	nodemask_t nodes, mask;
	int pagesize = getpagesize();
	int i;
	int pol;
	int node;
	int err = 0;

	for (i = max; i >= 0; --i) { 
		char *mem = mmap(NULL, pagesize*(max+1), PROT_READ|PROT_WRITE, 
					MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
		char *adr = mem;

		if (mem == (char *)-1)
			err("mmap");

		printf("%d offset %lx\n", i, (adr - mem)); 

		nodemask_zero(&nodes);
		nodemask_zero(&mask);
		nodemask_set(&mask, i);

		if (mbind(adr,  pagesize, MPOL_PREFERRED, nodes.n, sizeof(nodemask_t)*8+1, 0) < 0) 
			err("mbind");
		
		++*adr;
			
		if (get_mempolicy(&pol, mask.n, sizeof(nodemask_t)*8+1, adr, MPOL_F_ADDR) < 0) 
			err("get_mempolicy");
	
		assert(pol == MPOL_PREFERRED);
		assert(nodemask_isset(&mask, i));

		node = 0x123;
		
		if (get_mempolicy(&node, NULL, 0, adr, MPOL_F_ADDR|MPOL_F_NODE) < 0)
			err("get_mempolicy2"); 

		printf("got node %d expected %d\n", node, i);

		if (node != i) 
			err = 1;
	}
	return err;
}
