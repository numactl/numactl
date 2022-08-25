/* Test wrapper for the nodemask parser */
#include <stdio.h>
#include "numa.h"
#include "util.h"

int main(int ac, char **av)
{
	int err = 0;
	while (*++av) {
		struct bitmask *mask = numa_parse_nodestring(*av);
		if (!mask) {
			printf("Failed to convert `%s'\n", *av);
			err |= 1;
			continue;
		}
		printmask("result", mask);
		numa_bitmask_free(mask);
	}
	return err;
}
