/* Test node number validation */
#include "numa.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>

int main(void)
{
	int nodes[] = { INT_MIN, -2, -1, INT_MAX };
	unsigned int i;
	int ret, failed = 0;
	struct bitmask *cpus;

	if (numa_available() < 0) {
		printf("no numa support in kernel\n");
		return 77;
	}
	cpus = numa_allocate_cpumask();
	if (!cpus) {
		printf("failed to allocate cpumask\n");
		return 1;
	}

	for (i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++) {
		errno = 0;
		ret = numa_node_to_cpus(nodes[i], cpus);
		if (ret != -1 || errno != ERANGE) {
			printf("numa_node_to_cpus(%d): returned %d, errno %d\n",
			       nodes[i], ret, errno);
			failed = 1;
		}

		errno = 0;
		ret = numa_run_on_node(nodes[i]);
		if (nodes[i] == -1) {
			if (ret != 0) {
				perror("numa_run_on_node(-1)");
				failed = 1;
			}
		} else if (ret != -1 || errno != EINVAL) {
			printf("numa_run_on_node(%d): returned %d, errno %d\n",
			       nodes[i], ret, errno);
			failed = 1;
		}
	}

	numa_free_cpumask(cpus);
	return failed;
}
