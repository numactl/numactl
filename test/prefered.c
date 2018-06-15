/* Test prefer policy */
#include "numa.h"
#include "numaif.h"
#include <sys/mman.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#define err(x) perror(x),exit(1)

int main(void)
{
	int max = numa_max_node();
	int maxmask = numa_num_possible_nodes();
	struct bitmask *nodes, *mask;
	int pagesize = getpagesize();
	int i;
	int pol;
	int node;
	int err = 0;
	nodes = numa_bitmask_alloc(maxmask);
	mask = numa_bitmask_alloc(maxmask);

	for (i = max; i >= 0; --i) {
		char *mem = mmap(NULL, pagesize*(max+1), PROT_READ|PROT_WRITE,
					MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
		char *adr = mem;

		if (mem == (char *)-1)
			err("mmap");

		printf("%d offset %lx\n", i, (long)(adr - mem));

		numa_bitmask_clearall(nodes);
		numa_bitmask_clearall(mask);
		numa_bitmask_setbit(nodes, i);

		if (mbind(adr,  pagesize, MPOL_PREFERRED, nodes->maskp,
							nodes->size, 0) < 0)
			err("mbind");

		++*adr;

		if (get_mempolicy(&pol, mask->maskp, mask->size, adr, MPOL_F_ADDR) < 0)
			err("get_mempolicy");

		assert(pol == MPOL_PREFERRED);
		assert(numa_bitmask_isbitset(mask, i));

		node = 0x123;

		if (get_mempolicy(&node, NULL, 0, adr, MPOL_F_ADDR|MPOL_F_NODE) < 0)
			err("get_mempolicy2");

		printf("got node %d expected %d\n", node, i);

		if (node != i)
			err = 1;
	}
	return err;
}
