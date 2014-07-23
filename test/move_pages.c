/*
 * Test program to test the moving of individual pages in a process.
 *
 * (C) 2006 Silicon Graphics, Inc.
 *		Christoph Lameter <clameter@sgi.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include "numa.h"
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

int main(int argc, char **argv)
{
	int i, rc;

	pagesize = getpagesize();

	nr_nodes = numa_max_node();

	if (nr_nodes < 2) {
		printf("A minimum of 2 nodes is required for this test.\n");
		exit(1);
	}

	setbuf(stdout, NULL);
	printf("move_pages() test ......\n");
	if (argc > 1)
		sscanf(argv[1], "%d", &page_count);

	printf("pages=%d (%s)\n", page_count, argv[1]);

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
		nodes[i] = (i % nr_nodes);
		status[i] = -123;
	}

	printf("\nMoving pages to start node ...\n");
	rc = numa_move_pages(0, page_count, addr, NULL, status, 0);
	if (rc < 0)
		perror("move_pages");

	for (i = 0; i < page_count; i++)
		printf("Page %d vaddr=%p node=%d\n", i, pages + i * pagesize, status[i]);

	printf("\nMoving pages to target nodes ...\n");
	rc = numa_move_pages(0, page_count, addr, nodes, status, 0);

	if (rc < 0) {
		perror("move_pages");
		errors++;
	}

	for (i = 0; i < page_count; i++) {
		if (i != 2) {
			if (pages[ i* pagesize ] != (char) i)
				errors++;
			else if (nodes[i] != (i % nr_nodes))
				errors++;
		}
	}

	for (i = 0; i < page_count; i++) {
		printf("Page %d vaddr=%lx node=%d\n", i,
			(unsigned long)(pages + i * pagesize), status[i]);
	}

	if (!errors)
		printf("Test successful.\n");
	else
		printf("%d errors.\n", errors);

	return errors > 0 ? 1 : 0;
}

