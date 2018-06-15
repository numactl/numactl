#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include "numa.h"
#include "numaif.h"

#define DEFAULT_NR_PAGES	1024

static int parse_int(const char *str)
{
	char	*endptr;
	long	ret = strtol(str, &endptr, 0);
	if (*endptr != '\0') {
		fprintf(stderr, "[error] strtol() failed: parse error: %s\n", endptr);
		exit(1);
	}

	if (errno == ERANGE)
		fprintf(stderr, "[warning] strtol() out of range\n");

	if (ret > INT_MAX || ret < INT_MIN) {
		fprintf(stderr, "[warning] parse_int() out of range\n");
		ret = (ret > 0) ? INT_MAX : INT_MIN;
	}

	return (int) ret;
}

int main(int argc, char **argv)
{
	char	*mem;
	int		page_size = numa_pagesize();
	int		node = 0;
	int		nr_pages = DEFAULT_NR_PAGES;

	if (numa_available() < 0) {
		fprintf(stderr, "numa is not available");
		exit(1);
	}

	if (argc > 1)
		node = parse_int(argv[1]);
	if (argc > 2)
		nr_pages = parse_int(argv[2]);

	mem = numa_alloc_onnode(page_size, node);

	/* Store the policy of the newly allocated area */
	unsigned long	nodemask;
	int				mode;
	int				nr_nodes = numa_num_possible_nodes();
	if (get_mempolicy(&mode, &nodemask, nr_nodes, mem,
					  MPOL_F_NODE | MPOL_F_ADDR) < 0) {
		perror("get_mempolicy() failed");
		exit(1);
	}

	/* Print some info */
	printf("Page size: %d\n", page_size);
	printf("Pages realloc'ed: %d\n", nr_pages);
	printf("Allocate data in node: %d\n", node);

	int i;
	int nr_inplace = 0;
	int nr_moved   = 0;
	for (i = 0; i < nr_pages; i++) {
		/* Enlarge mem with one more page */
		char	*new_mem = numa_realloc(mem, (i+1)*page_size, (i+2)*page_size);
		if (!new_mem) {
			perror("numa_realloc() failed");
			exit(1);
		}

		if (new_mem == mem)
			++nr_inplace;
		else
			++nr_moved;
		mem = new_mem;

		/* Check the policy of the realloc'ed area */
		unsigned long	realloc_nodemask;
		int				realloc_mode;
		if (get_mempolicy(&realloc_mode, &realloc_nodemask,
						  nr_nodes, mem, MPOL_F_NODE | MPOL_F_ADDR) < 0) {
			perror("get_mempolicy() failed");
			exit(1);
		}

		assert(realloc_nodemask == nodemask &&
			   realloc_mode == mode && "policy changed");
	}

	/* Shrink to the original size */
	mem = numa_realloc(mem, (nr_pages + 1)*page_size, page_size);
	if (!mem) {
		perror("numa_realloc() failed");
		exit(1);
	}

	numa_free(mem, page_size);
	printf("In-place reallocs: %d\n", nr_inplace);
	printf("Moved reallocs: %d\n", nr_moved);
	return 0;
}
