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
	unsigned long nodes, mask;
	int sz = getpagesize();
	char *mem = mmap(NULL, sz*2, PROT_READ|PROT_WRITE, 
					MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
	int off = 0;
	int i;
	int pol;
	int node;

	if (mem == (char *)-1)
		err("mmap");

	for (i = max; i >= 0; i--) { 
		printf("%d\n", i); 

		nodes = 1UL << i;
		if (mbind(mem+off,  sz, MPOL_PREFERRED, &nodes, 2, 0) < 0) 
			err("mbind");
		
		mem[off] = 0;
			
		if (get_mempolicy(&pol, &mask, 64, mem+off, MPOL_F_ADDR) < 0) 
			err("get_mempolicy");
	
		assert(pol == MPOL_PREFERRED);
		assert(mask & (1UL << i)); 


		node = 0x123;
		
		if (get_mempolicy(&node, NULL, 0, mem+off, MPOL_F_ADDR|MPOL_F_NODE) < 0)
			err("get_mempolicy2"); 

		printf("node %d\n", i);

		assert(node == i);	

		off += sz;		
	}
	return 0;
}
