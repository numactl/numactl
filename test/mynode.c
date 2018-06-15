#include <numa.h>
#include <numaif.h>
#include <stdio.h>

int main(void)
{
	int nd;
	char *man = numa_alloc(1000);
	*man = 1;
	if (get_mempolicy(&nd, NULL, 0, man, MPOL_F_NODE|MPOL_F_ADDR) < 0)
		perror("get_mempolicy");
	else
		printf("my node %d\n", nd);
	return 0;
}
