#include <numa.h>
#include <numaif.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define err(x) perror(x),exit(1)

enum SZ {
	MEMSZ = 100<<20,
	NTHR = 10,
};

/* test if shared interleaving state works. */
int main(void)
{
	int i, k;
	char *mem;
	int pagesz = getpagesize();
	int max_node;

	if (numa_available() < 0) {
		printf("no NUMA API available\n");
		exit(1);
	}
	max_node = numa_max_node();
	mem = numa_alloc_interleaved(MEMSZ);
	for (i = 0; i < NTHR; i++) {
		if (fork() == 0) {
			for (k = i*pagesz; k < MEMSZ; k += pagesz * NTHR) {
				mem[k] = 1;
			}
			_exit(0);
		}
	}
	for (i = 0; i < NTHR; i++)
		wait(NULL);
	k = 0;
	for (i = 0; i < MEMSZ; i += pagesz) {
		int nd;
		if (get_mempolicy(&nd, NULL, 0, mem + i, MPOL_F_NODE|MPOL_F_ADDR) < 0)
			err("get_mempolicy");
		if (nd != k)
			printf("offset %d node %d expected %d\n", i, nd, k);
		k = (k+1)%(max_node+1);
	}

	return 0;
}
