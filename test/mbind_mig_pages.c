/*
 * Test program to test the moving of pages using mbind.
 *
 * (C) 2006 Silicon Graphics, Inc.
 *		Christoph Lameter <clameter@sgi.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <numa.h>
#include <numaif.h>
#include <unistd.h>
#include <asm/unistd.h>

unsigned int pagesize;
unsigned int page_count = 32;

char *page_base;
char *pages;

void **addr;
int *status;
int *nodes;
int errors;
int nr_nodes;

struct bitmask *old_nodes;
struct bitmask *new_nodes;

int main(int argc, char **argv)
{
	int i, rc;

	pagesize = getpagesize();

	nr_nodes = numa_max_node()+1;

	old_nodes = numa_bitmask_alloc(nr_nodes);
	new_nodes = numa_bitmask_alloc(nr_nodes);
	numa_bitmask_setbit(old_nodes, 0);
	numa_bitmask_setbit(new_nodes, 1);

	if (nr_nodes < 2) {
		printf("A minimum of 2 nodes is required for this test.\n");
		exit(1);
	}

	setbuf(stdout, NULL);
	printf("mbind migration test ......\n");
	if (argc > 1)
		sscanf(argv[1], "%d", &page_count);

	page_base = malloc((pagesize + 1) * page_count);
	addr = malloc(sizeof(char *) * page_count);
	status = malloc(sizeof(int *) * page_count);
	nodes = malloc(sizeof(int *) * page_count);
	if (!page_base || !addr || !status || !nodes) {
		printf("Unable to allocate memory\n");
		exit(1);
	}

	pages = (void *) ((((long)page_base) & ~((long)(pagesize - 1))) + pagesize);

	for (i = 0; i < page_count; i++) {
		if (i != 2)
			/* We leave page 2 unallocated */
			pages[ i * pagesize ] = (char) i;
		addr[i] = pages + i * pagesize;
		nodes[i] = 0;
		status[i] = -123;
	}

	/* Move pages toi node zero */
	numa_move_pages(0, page_count, addr, nodes, status, 0);

	printf("\nPage status before page migration\n");
	printf("---------------------------------\n");
	rc = numa_move_pages(0, page_count, addr, NULL, status, 0);
	if (rc < 0) {
		perror("move_pages");
		exit(1);
	}

	for (i = 0; i < page_count; i++) {
		printf("Page %d vaddr=%p node=%d\n", i, pages + i * pagesize, status[i]);
		if (i != 2 && status[i]) {
			printf("Bad page state. Page %d status %d\n",i, status[i]);
			exit(1);
		}
	}

	/* Move to node zero */
	printf("\nMoving pages via mbind to node 0 ...\n");
	rc = mbind(pages, page_count * pagesize, MPOL_BIND, old_nodes->maskp,
		old_nodes->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT);
	if (rc < 0) {
		perror("mbind");
		errors++;
	}

	printf("\nMoving pages via mbind from node 0 to 1 ...\n");
	rc = mbind(pages, page_count * pagesize, MPOL_BIND, new_nodes->maskp,
		new_nodes->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT);
	if (rc < 0) {
		perror("mbind");
		errors++;
	}

	numa_move_pages(0, page_count, addr, NULL, status, 0);
	for (i = 0; i < page_count; i++) {
		printf("Page %d vaddr=%lx node=%d\n", i,
			(unsigned long)(pages + i * pagesize), status[i]);
		if (i != 2) {
			if (pages[ i* pagesize ] != (char) i) {
				printf("*** Page content corrupted.\n");
				errors++;
			} else if (status[i] != 1) {
				printf("*** Page on wrong node.\n");
				errors++;
			}
		}
	}

	if (!errors)
		printf("Test successful.\n");
	else
		printf("%d errors.\n", errors);

	return errors > 0 ? 1 : 0;
}
