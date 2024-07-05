/* Copyright (C) 2003,2004,2005 Andi Kleen, SuSE Labs.
   Command line NUMA policy control.

   numactl is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; version
   2.

   numactl is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should find a copy of v2 of the GNU General Public License somewhere
   on your Linux system; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#define _GNU_SOURCE
#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include "numa.h"
#include "numaif.h"
#include "numaint.h"
#include "util.h"
#include "shm.h"

#define CPUSET 0
#define ALL 1

int exitcode;
int cpu_compress;

enum {
	CPU_COMPRESS = 300,
	OPT_VERSION,
};

static struct option opts[] = {
	{"all", 0, 0, 'a'},
	{"interleave", 1, 0, 'i' },
	{"weighted-interleave", 1, 0, 'w' },
	{"preferred", 1, 0, 'p' },
	{"preferred-many", 1, 0, 'P' },
	{"cpubind", 1, 0, 'c' },
	{"cpunodebind", 1, 0, 'N' },
	{"physcpubind", 1, 0, 'C' },
	{"membind", 1, 0, 'm'},
	{"show", 0, 0, 's' },
	{"localalloc", 0,0, 'l'},
	{"balancing", 0, 0, 'b'},
	{"hardware", 0,0,'H' },

	{"shm", 1, 0, 'S'},
	{"file", 1, 0, 'f'},
	{"offset", 1, 0, 'o'},
	{"length", 1, 0, 'L'},
	{"strict", 0, 0, 't'},
	{"shmmode", 1, 0, 'M'},
	{"dump", 0, 0, 'd'},
	{"dump-nodes", 0, 0, 'D'},
	{"shmid", 1, 0, 'I'},
	{"huge", 0, 0, 'u'},
	{"touch", 0, 0, 'T'},
        {"cpu-compress", 0, 0, CPU_COMPRESS },
	{"verify", 0, 0, 'V'}, /* undocumented - for debugging */
	{"version", 0, 0, OPT_VERSION },
	{ 0 }
};

static void usage(void)
{
	fprintf(stderr,
		"usage: numactl [--all | -a] [--balancing | -b]\n"
		"               [--interleave= | -i <nodes>] [--weighted-interleave= | -w <nodes>]\n"
		"               [--preferred= | -p <node>] [--preferred-many= | -P <nodes>]\n"
		"               [--physcpubind= | -C <cpus>] [--cpunodebind= | -N <nodes>]\n"
		"               [--membind= | -m <nodes>] [--localalloc | -l] command args ...\n"
		"               [--localalloc | -l] command args ...\n"
		"       numactl [--show | -s]\n"
		"       numactl [--hardware | -H] [--cpu-compress]\n"
		"       numactl [--version]\n"
		"       numactl [--length | -L <length>] [--offset | -o <offset>] [--shmmode | -M <shmmode>]\n"
		"               [--strict | -t]\n"
		"               [--shmid | -I <id>] --shm | -S <shmkeyfile>\n"
		"               [--shmid | -I <id>] --file | -f <tmpfsfile>\n"
		"               [--huge | -u] [--touch | -T] \n"
		"               memory policy [--dump | -d] [--dump-nodes | -D]\n"
		"\n"
		"memory policy is --preferred | -p, --membind | -m, --localalloc | -l,\n"
		"                 --interleave | -i, --weighted-interleave | -w\n"
		"<nodes> is a comma delimited list of node numbers or A-B ranges or all.\n"
		"Instead of a number a node can also be:\n"
		"  netdev:DEV the node connected to network device DEV\n"
		"  file:PATH  the node the block device of path is connected to\n"
		"  ip:HOST    the node of the network device host routes through\n"
		"  block:PATH the node of block device path\n"
		"  pci:[seg:]bus:dev[:func] The node of a PCI device\n"
		"<cpus> is a comma delimited list of cpu numbers or A-B ranges or all\n"
		"all ranges can be inverted with !\n"
		"all numbers and ranges can be made cpuset-relative with +\n"
		"the old --cpubind argument is deprecated.\n"
		"use --cpunodebind or --physcpubind instead\n"
		"use --balancing | -b to enable Linux kernel NUMA balancing\n"
		"for the process if it is supported by kernel\n"
		"<length> can have g (GB), m (MB) or k (KB) suffixes\n");
	exit(1);
}

static void usage_msg(char *msg, ...)
{
	va_list ap;
	va_start(ap,msg);
	fprintf(stderr, "numactl: ");
	vfprintf(stderr, msg, ap);
	putchar('\n');
	usage();
	va_end(ap);
}

static void show_physcpubind(void)
{
	int ncpus = numa_num_configured_cpus();

	for (;;) {
		struct bitmask *cpubuf;

		cpubuf = numa_bitmask_alloc(ncpus);

		if (numa_sched_getaffinity(0, cpubuf) < 0) {
			if (errno == EINVAL && ncpus < 1024*1024) {
				ncpus *= 2;
				continue;
			}
			err("sched_get_affinity");
		}
		printmask("physcpubind", cpubuf);
		break;
	}
}

static void show(void)
{
	struct bitmask *membind, *interleave, *cpubind, *preferred;
	unsigned long cur;
	int policy;

	if (numa_available() < 0) {
		show_physcpubind();
		printf("No NUMA support available on this system.\n");
		exit(1);
	}

	cpubind = numa_get_run_node_mask();

	preferred = numa_preferred_many();
	interleave = numa_get_interleave_mask();
	membind = numa_get_membind();
	cur = numa_get_interleave_node();

	policy = 0;
	if (get_mempolicy(&policy, NULL, 0, 0, 0) < 0)
		perror("get_mempolicy");

	printf("policy: %s\n", policy_name(policy));

	printf("preferred node: ");
	switch (policy) {
	case MPOL_PREFERRED:
		if (numa_bitmask_weight(preferred))
			printf("%d\n", find_first(preferred));
		else
			printf("%d\n", 0);
		break;
	case MPOL_DEFAULT:
		printf("current\n");
		break;
	case MPOL_INTERLEAVE:
		printf("%ld (interleave next)\n",cur);
		break;
	case MPOL_BIND:
		printf("%d\n", find_first(membind));
		break;
	case MPOL_PREFERRED_MANY:
		printf("%ld (preferred-many)\n",cur);
		break;
	}
	if (policy == MPOL_INTERLEAVE) {
		printmask("interleavemask", interleave);
		printf("interleavenode: %ld\n", cur);
	}
	show_physcpubind();
	printmask("cpubind", cpubind);  // for compatibility
	printmask("nodebind", cpubind);
	printmask("membind", membind);
	printmask("preferred", preferred);
	numa_bitmask_free(preferred);
}

static char *fmt_mem(unsigned long long mem, char *buf)
{
	if (mem == -1L)
		sprintf(buf, "<not available>");
	else
		sprintf(buf, "%llu MB", mem >> 20);
	return buf;
}

static void print_distances(int maxnode)
{
	int i,k;
	int fst = 0;

	for (i = 0; i <= maxnode; i++)
		if (numa_bitmask_isbitset(numa_nodes_ptr, i)) {
			fst = i;
			break;
		}
	if (numa_distance(maxnode,fst) == 0) {
		printf("No distance information available.\n");
		return;
	}
	printf("node distances:\n");
	printf("node  ");
	for (i = 0; i <= maxnode; i++)
		if (numa_bitmask_isbitset(numa_nodes_ptr, i))
			printf("% 4d ", i);
	printf("\n");
	for (i = 0; i <= maxnode; i++) {
		if (!numa_bitmask_isbitset(numa_nodes_ptr, i))
			continue;
		printf("% 4d: ", i);
		for (k = 0; k <= maxnode; k++)
			if (numa_bitmask_isbitset(numa_nodes_ptr, i) &&
			    numa_bitmask_isbitset(numa_nodes_ptr, k))
				printf("% 4d ", numa_distance(i,k));
		printf("\n");
	}
}

static void print_node_cpus(int node)
{
        int i = 0, err, start, segment = 0, count = 0;
        struct bitmask *cpus;

        cpus = numa_allocate_cpumask();
        err = numa_node_to_cpus(node, cpus);
        if (err < 0) {
                goto out;
        }

        while (i < cpus->size) {
                if (!cpu_compress) {
                        if (numa_bitmask_isbitset(cpus, i))
                        printf(" %d", i);
                        i++;
                        continue;
                }

                start = -1;

                // Find the start and end of a range of available CPUs.
                while (i < cpus->size && numa_bitmask_isbitset(cpus, i)) {
                        if (start == -1) start = i;
                        i++;
                }
                if (start == -1) {
                        i++;
                        continue;
                }
                if (segment) {
                        printf(",");
                }

                int end = i - 1;
                count += (end - start) + 1;
                if (start == end) {
                        printf(" %d", start);
                } else {
                        printf(" %d-%d", start, end);
                }
                segment++;
        }

        if (!cpu_compress)
                printf("\n");
        else
                printf(" (%d)\n", count);

out:
        numa_free_cpumask(cpus);
}

static void hardware(void)
{
	int i;
	int numnodes=0;
	int prevnode=-1;
	int skip=0;
	int maxnode = numa_max_node();

	if (numa_available() < 0) {
                printf("No NUMA available on this system\n");
                exit(1);
        }

	for (i=0; i<=maxnode; i++)
		if (numa_bitmask_isbitset(numa_nodes_ptr, i))
			numnodes++;
	printf("available: %d nodes (", numnodes);
	for (i=0; i<=maxnode; i++) {
		if (numa_bitmask_isbitset(numa_nodes_ptr, i)) {
			if (prevnode == -1) {
				printf("%d", i);
				prevnode=i;
				continue;
			}

			if (i > prevnode + 1) {
				if (skip) {
					printf("%d", prevnode);
					skip=0;
				}
				printf(",%d", i);
				prevnode=i;
				continue;
			}

			if (i == prevnode + 1) {
				if (!skip) {
					printf("-");
					skip=1;
				}
				prevnode=i;
			}

			if ((i == maxnode) && skip)
				printf("%d", prevnode);
		}
	}
	printf(")\n");

	for (i = 0; i <= maxnode; i++) {
		char buf[64];
		long long fr;
		unsigned long long sz = numa_node_size64(i, &fr);
		if (!numa_bitmask_isbitset(numa_nodes_ptr, i))
			continue;

		printf("node %d cpus:", i);
		print_node_cpus(i);
		printf("node %d size: %s\n", i, fmt_mem(sz, buf));
		printf("node %d free: %s\n", i, fmt_mem(fr, buf));
	}
	print_distances(maxnode);
}

static void checkerror(char *s)
{
	if (errno) {
		perror(s);
		exit(1);
	}
}

static void checknuma(void)
{
	static int numa = -1;
	if (numa < 0) {
		if (numa_available() < 0)
			complain("This system does not support NUMA policy");
	}
	numa = 0;
}

int set_policy = -1;

static inline void setpolicy(int pol)
{
	if (set_policy != -1)
		usage_msg("Conflicting policies");
	set_policy = pol;
}

static inline void nopolicy(void)
{
	if (set_policy >= 0)
		usage_msg("specify policy after --shm/--file");
}


static int shmattached = 0;
static int did_node_cpu_parse = 0;
static char *shmoption;

static inline void check_cpubind(int flag)
{
	if (flag)
		usage_msg("cannot do --cpubind on shared memory\n");
}

static inline void noshm(char *opt)
{
	if (shmattached)
		usage_msg("%s must be before shared memory specification", opt);
	shmoption = opt;
}

static inline void dontshm(char *opt)
{
	if (shmoption)
		usage_msg("%s shm option is not allowed before %s", shmoption, opt);
}

static inline void needshm(char *opt)
{
	if (!shmattached)
		usage_msg("%s must be after shared memory specification", opt);
}

static inline void check_all_parse(int flag)
{
	if (did_node_cpu_parse)
		usage_msg("--all/-a option must be before all cpu/node specifications");
}

static void get_short_opts(struct option *o, char *s)
{
	*s++ = '+';
	while (o->name) {
		if (isprint(o->val)) {
			*s++ = o->val;
			if (o->has_arg)
				*s++ = ':';
		}
		o++;
	}
	*s = '\0';
}

static void check_shmbeyond(char *msg)
{
	if (shmoffset >= shmlen) {
		fprintf(stderr,
		"numactl: region offset %#llx beyond its length %#llx at %s\n",
				shmoffset, shmlen, msg);
		exit(1);
	}
}

static struct bitmask *numactl_parse_nodestring(char *s, int flag)
{
	static char *last;

	if (s[0] == 's' && !strcmp(s, "same")) {
		if (!last)
			usage_msg("same needs previous node specification");
		s = last;
	} else {
		last = s;
	}

	if (flag == ALL)
		return numa_parse_nodestring_all(s);
	else
		return numa_parse_nodestring(s);
}

int main(int ac, char **av)
{
	int c;
	char *end;
	char shortopts[array_len(opts)*2 + 1];
	struct bitmask *mask = NULL;
	int did_cpubind = 0;
	int did_strict = 0;
	int do_shm = 0;
	int do_dump = 0;
	int parse_all = 0;
	int numa_balancing = 0;
	int do_hardware = 0;
	int weighted_interleave = 0;

	get_short_opts(opts,shortopts);
	while ((c = getopt_long(ac, av, shortopts, opts, NULL)) != -1) {
		switch (c) {
		case 's': /* --show */
			show();
			exit(0);
		case 'H': /* --hardware */
			nopolicy();
			do_hardware = 1;
			break;
		case 'b': /* --balancing  */
			nopolicy();
			numa_balancing = 1;
			break;
		case 'w': /* --weighted-interleave */
			weighted_interleave = 1;
			/* fall-through - logic is the same as interleave */
		case 'i': /* --interleave */
			checknuma();
			if (parse_all)
				mask = numactl_parse_nodestring(optarg, ALL);
			else
				mask = numactl_parse_nodestring(optarg, CPUSET);
			if (!mask) {
				printf ("<%s> is invalid\n", optarg);
				usage();
			}

			errno = 0;
			did_node_cpu_parse = 1;
			if (weighted_interleave)
				setpolicy(MPOL_WEIGHTED_INTERLEAVE);
			else
				setpolicy(MPOL_INTERLEAVE);
			if (shmfd >= 0)
				numa_interleave_memory(shmptr, shmlen, mask);
			else {
				if (weighted_interleave)
					numa_set_weighted_interleave_mask(mask);
				else
					numa_set_interleave_mask(mask);
			}
			checkerror("setting interleave mask");
			break;
		case 'N': /* --cpunodebind */
		case 'c': /* --cpubind */
			dontshm("-c/--cpubind/--cpunodebind");
			checknuma();
			if (parse_all)
				mask = numactl_parse_nodestring(optarg, ALL);
			else
				mask = numactl_parse_nodestring(optarg, CPUSET);
			if (!mask) {
				printf ("<%s> is invalid\n", optarg);
				usage();
			}
			errno = 0;
			check_cpubind(do_shm);
			did_cpubind = 1;
			did_node_cpu_parse = 1;
			numa_run_on_node_mask_all(mask);
			checkerror("sched_setaffinity");
			break;
		case 'C': /* --physcpubind */
		{
			struct bitmask *cpubuf;
			dontshm("-C/--physcpubind");
			if (parse_all)
				cpubuf = numa_parse_cpustring_all(optarg);
			else
				cpubuf = numa_parse_cpustring(optarg);
			if (!cpubuf) {
				printf ("<%s> is invalid\n", optarg);
				usage();
			}
			errno = 0;
			check_cpubind(do_shm);
			did_cpubind = 1;
			did_node_cpu_parse = 1;
			numa_sched_setaffinity(0, cpubuf);
			checkerror("sched_setaffinity");
			numa_bitmask_free(cpubuf);
			break;
		}
		case 'm': /* --membind */
			checknuma();
			setpolicy(MPOL_BIND);
			if (parse_all)
				mask = numactl_parse_nodestring(optarg, ALL);
			else
				mask = numactl_parse_nodestring(optarg, CPUSET);
			if (!mask) {
				printf ("<%s> is invalid\n", optarg);
				usage();
			}
			errno = 0;
			did_node_cpu_parse = 1;
			numa_set_bind_policy(1);
			if (shmfd >= 0) {
				numa_tonodemask_memory(shmptr, shmlen, mask);
			} else if (numa_balancing) {
				numa_set_membind_balancing(mask);
			} else {
				numa_set_membind(mask);
			}
			numa_set_bind_policy(0);
			checkerror("setting membind");
			break;
		case 'P': /* --preferred-many */
			if (!numa_has_preferred_many())
				complain("preferred-many requested without kernel support");
		case 'p': /* --preferred */
			checknuma();
			if (parse_all)
				mask = numactl_parse_nodestring(optarg, ALL);
			else
				mask = numactl_parse_nodestring(optarg, CPUSET);
			if (!mask) {
				printf ("<%s> is invalid\n", optarg);
				usage();
			}
			errno = 0;
			did_node_cpu_parse = 1;
			numa_set_bind_policy(0);
			if (shmfd >= 0) {
				numa_tonode_memory(shmptr, shmlen, find_first(mask));
				/* Correspond to numa_set_bind_policy function */
				if (numa_has_preferred_many()) {
					setpolicy(MPOL_PREFERRED_MANY);
				} else {
					setpolicy(MPOL_PREFERRED);
				}
			} else if (c == 'p') {
				if (numa_bitmask_weight(mask) != 1)
					usage();

				setpolicy(MPOL_PREFERRED);
				numa_set_preferred(find_first(mask));
			} else {
				setpolicy(MPOL_PREFERRED_MANY);
				numa_set_preferred_many(mask);
			}
			checkerror("setting preferred node");
			break;
		case 'l': /* --local */
			checknuma();
			setpolicy(MPOL_LOCAL);
			errno = 0;
			if (shmfd >= 0)
				numa_setlocal_memory(shmptr, shmlen);
			else
				numa_set_localalloc();
			checkerror("local allocation");
			break;
		case 'S': /* --shm */
			check_cpubind(did_cpubind);
			nopolicy();
			attach_sysvshm(optarg, "--shm");
			shmattached = 1;
			break;
		case 'f': /* --file */
			check_cpubind(did_cpubind);
			nopolicy();
			attach_shared(optarg, "--file");
			shmattached = 1;
			break;
		case 'L': /* --length */
			noshm("--length");
			shmlen = memsize(optarg);
			break;
		case 'M': /* --shmmode */
			noshm("--shmmode");
			shmmode = strtoul(optarg, &end, 8);
			if (end == optarg || *end)
				usage();
			break;
		case 'd': /* --dump */
			if (shmfd < 0)
				complain(
				"Cannot do --dump without shared memory.\n");
			dump_shm();
			do_dump = 1;
			break;
		case 'D': /* --dump-nodes */
			if (shmfd < 0)
				complain(
			    "Cannot do --dump-nodes without shared memory.\n");
			dump_shm_nodes();
			do_dump = 1;
			break;
		case 't': /* --strict */
			did_strict = 1;
			numa_set_strict(1);
			break;
		case 'I': /* --shmid */
			shmid = strtoul(optarg, &end, 0);
			if (end == optarg || *end)
				usage();
			break;

		case 'u': /* --huge */
			noshm("--huge");
			shmflags |= SHM_HUGETLB;
			break;

		case 'o':  /* --offset */
			noshm("--offset");
			shmoffset = memsize(optarg);
			break;

		case 'T': /* --touch */
			needshm("--touch");
			check_shmbeyond("--touch");
			numa_police_memory(shmptr, shmlen);
			break;

		case 'V': /* --verify */
			needshm("--verify");
			if (set_policy < 0)
				complain("Need a policy first to verify");
			check_shmbeyond("--verify");
			numa_police_memory(shmptr, shmlen);
			if (!mask)
				complain("Need a mask to verify");
			else
				verify_shm(set_policy, mask);
			break;

		case 'a': /* --all */
			check_all_parse(did_node_cpu_parse);
			parse_all = 1;
			break;
		case CPU_COMPRESS:
			cpu_compress = 1;
			break;
		case OPT_VERSION:
			nopolicy();
			printf("%s\n", VERSION);
			exit(0);
		default:
			usage();
		}
	}

	if (do_hardware) {
		hardware();
		exit(0);
	}

	numa_bitmask_free(mask);

	av += optind;
	ac -= optind;

	if (shmfd >= 0) {
		if (*av)
			usage();
		exit(exitcode);
	}

	if (did_strict)
		fprintf(stderr,
			"numactl: warning. Strict flag for process ignored.\n");

	if (do_dump)
		usage_msg("cannot do --dump|--dump-shm for process");

	if (shmoption)
		usage_msg("shm related option %s for process", shmoption);

	if (*av == NULL)
		usage();
	execvp(*av, av);
	complain("execution of `%s': %s\n", av[0], strerror(errno));
	return 0; /* not reached */
}
