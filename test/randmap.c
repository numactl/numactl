/* Randomly change policy */
#include <stdio.h>
#include "numa.h"
#include "numaif.h"
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define SIZE (100*1024*1024)
#define PAGES (SIZE/pagesize)

#define perror(x) printf("%s: %s\n", x, strerror(errno))
#define err(x) perror(x),exit(1)

struct page {
	unsigned long mask;
	int policy;
};

struct page *pages;
char *map;
int pagesize;

void setpol(unsigned long offset, unsigned long length, int policy, unsigned long nodes)
{
	long i, end;

	printf("off:%lx length:%lx policy:%d nodes:%lx\n",
	       offset, length, policy, nodes);

	if (mbind(map + offset*pagesize, length*pagesize, policy,
		  &nodes, 8, 0) < 0) {
		printf("mbind: %s offset %lx length %lx policy %d nodes %lx\n",
		       strerror(errno),
		       offset*pagesize, length*pagesize,
		       policy, nodes);
		return;
	}

	for (i = offset; i < offset+length; i++) {
		pages[i].mask = nodes;
		pages[i].policy = policy;
	}

	i = offset - 20;
	if (i < 0)
		i = 0;
	end = offset+length+20;
	if (end > PAGES)
		end = PAGES;
	for (; i < end; i++) {
		int pol2;
		unsigned long nodes2;
		if (get_mempolicy(&pol2, &nodes2, sizeof(long)*8, map+i*pagesize,
				  MPOL_F_ADDR) < 0)
			err("get_mempolicy");
		if (pol2 != pages[i].policy) {
			printf("%lx: got policy %d expected %d, nodes got %lx expected %lx\n",
			       i, pol2, pages[i].policy, nodes2, pages[i].mask);
		}
		if (policy != MPOL_DEFAULT && nodes2 != pages[i].mask) {
			printf("%lx: nodes %lx, expected %lx, policy %d\n",
			       i, nodes2, pages[i].mask, policy);
		}
	}
}

static unsigned char pop4[16] = {
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
};

int popcnt(unsigned long val)
{
	int count = 0;
	while (val) {
		count += pop4[val & 0xf];
		val >>= 4;
	}
	return count;
}

void testmap(void)
{
	pages = calloc(1, PAGES * sizeof(struct page));
	if (!pages)
		exit(100);

	printf("simple tests\n");
#define MB ((1024*1024)/pagesize)
	setpol(0, PAGES, MPOL_INTERLEAVE, 3);
	setpol(0, MB, MPOL_BIND, 1);
	setpol(MB, MB, MPOL_BIND, 1);
	setpol(MB, MB, MPOL_DEFAULT, 0);
	setpol(MB, MB, MPOL_PREFERRED, 2);
	setpol(MB/2, MB, MPOL_DEFAULT, 0);
	setpol(MB+MB/2, MB, MPOL_BIND, 2);
	setpol(MB/2+100, 100, MPOL_PREFERRED, 1);
	setpol(100, 200, MPOL_PREFERRED, 1);
	printf("done\n");

	for (;;) {
		unsigned long offset = random() % PAGES;
		int policy = random() % (MPOL_MAX);
		unsigned long nodes = random() % 4;
		long length = random() % (PAGES - offset);

		/* validate */
		switch (policy) {
		case MPOL_DEFAULT:
			nodes = 0;
			break;
		case MPOL_INTERLEAVE:
		case MPOL_BIND:
			if (nodes == 0)
				continue;
			break;
		case MPOL_PREFERRED:
			if (popcnt(nodes) != 1)
				continue;
			break;
		}

		setpol(offset, length, policy, nodes);

	}
}

int main(int ac, char **av)
{
	unsigned long seed;

	pagesize = getpagesize();

#if 0
	map = mmap(NULL, SIZE, PROT_READ, MAP_ANONYMOUS|MAP_PRIVATE, 0, 0);
	if (map == (char*)-1)
		err("mmap");
#else
	int shmid = shmget(IPC_PRIVATE, SIZE, IPC_CREAT|0666);
	if (shmid < 0) err("shmget");
	map = shmat(shmid, NULL, SHM_RDONLY);
	shmctl(shmid, IPC_RMID, NULL);
	if (map == (char *)-1) err("shmat");
	printf("map %p\n", map);
#endif

	if (av[1]) {
		char *end;
		unsigned long timeout = strtoul(av[1], &end, 0);
		switch (*end) {
		case 'h': timeout *= 3600; break;
		case 'm': timeout *= 60; break;
		}
		printf("running for %lu seconds\n", timeout);
		alarm(timeout);
	} else
		printf("running forever\n");

	if (av[1] && av[2])
		seed = strtoul(av[2], 0, 0);
	else
		seed = time(0);

	printf("random seed %lu\n", seed);
	srandom(seed);

	testmap();
	/* test shm etc. */
	return 0;
}
