/* Copyright (C) 2003,2004 Andi Kleen, SuSE Labs.

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
#include "numa.h"
#include "numaif.h"
#include "util.h"
#include "bitops.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

void printmask(char *name, struct bitmask *mask)
{ 
	int i;

	printf("%s: ", name); 
	for (i = 0; i <= mask->size; i++)
		if (bitmask_isbitset(mask, i))
			printf("%d ", i); 
	putchar('\n');
} 

/*
 * Extract a node or processor number from the given string.
 * Allow a relative node / processor specification within the allowed
 * set if "relative" is nonzero
 */
unsigned long get_nr(char *s, char **end, struct bitmask *bmp, int relative)
{
	long i, nr;

	if (!relative)
		return strtoul(s, end, 0);

	nr = strtoul(s, end, 0);
	if (s == *end)
		return nr;
	/* Find the nth set bit */
	for (i = 0; nr >= 0 && i <= bmp->size; i++)
		if (bitmask_isbitset(bmp, i))
			nr--;
	return i-1;
}

/*
 * cpumask() is called to create a cpumask_t mask, given
 * an ascii string such as 25 or 12-15 or 1,3,5-7 or +6-10.
 * (the + indicates that the numbers are cpuset-relative)
 *
 * The cpus may be specified as absolute, or relative to the current cpuset.
 * The list of available cpus for this task is in map "numa_all_cpus",
 * which may represent all cpus or the cpus in the current cpuset.
 * (it is set up by read_constraints() from the current task's Cpus_allowed)
 *
 * The caller must free the returned cpubuf buffer.
 */
struct bitmask *
cpumask(char *s, int *ncpus)
{
	int invert = 0, relative=0;
	int conf_cpus = number_of_configured_cpus();
	int task_cpus = number_of_task_cpus();
	char *end; 
	struct bitmask *cpubuf;

	cpubuf = allocate_cpumask();

	if (s[0] == 0) 
		return cpubuf;
	if (*s == '!') { 
		invert = 1;
		s++;
	}
	if (*s == '+') {
		relative++;
		s++;
	}
	do {
		unsigned long arg;
		int i;

		if (!strcmp(s,"all")) { 
			int i;
			for (i = 0; i < task_cpus; i++)
				bitmask_setbit(cpubuf, i);
			s+=4;
			break;
		}
		arg = get_nr(s, &end, numa_all_cpus, relative);
		if (end == s)
			complain("unparseable cpu description `%s'\n", s);
		if (arg >= task_cpus)
			complain("cpu argument %s is out of range\n", s);
		i = arg;
		bitmask_setbit(cpubuf, i);
		s = end; 
		if (*s == '-') {
			char *end2;
			unsigned long arg2;
			int i;
			arg2 = get_nr(++s, &end2, numa_all_cpus, relative);
			if (end2 == s)
				complain("missing cpu argument %s\n", s);
			if (arg2 >= task_cpus)
				complain("cpu argument %s out of range\n", s);
			while (arg <= arg2) {
				i = arg;
				if (bitmask_isbitset(numa_all_cpus, i))
					bitmask_setbit(cpubuf, i);
				arg++;
			}
			s = end2;
		}
	} while (*s++ == ','); 
	if (s[-1] != '\0')
		usage();
	if (invert) { 
		int i;
		for (i = 0; i < conf_cpus; i++) {
			if (bitmask_isbitset(cpubuf, i))
				bitmask_clearbit(cpubuf, i);
			else
				bitmask_setbit(cpubuf, i);
		}
	} 
	*ncpus = cpubuf->size;
	return cpubuf;	
}

void printcpumask(char *name, struct bitmask *mask)
{ 
	int i;
	printf("%s: ", name);
	for (i = 0; i < mask->size; i++) {
		if (bitmask_isbitset(mask, i))
			printf("%d ", i);
	}
	putchar('\n');
} 

/*
 * nodemask() is called to create a node mask, given
 * an ascii string such as 25 or 12-15 or 1,3,5-7 or +6-10.
 * (the + indicates that the numbers are cpuset-relative)
 *
 * The nodes may be specified as absolute, or relative to the current cpuset.
 * The list of available nodes is in map "numa_all_nodes",
 * which may represent all nodes or the nodes in the current cpuset.
 * (it is set up by read_constraints() from the current task's Mems_allowed)
 *
 * The caller must free the returned nodebuf buffer.
 */
struct bitmask *
nodemask(char *s)
{
	int maxnode = numa_max_node();
	int invert = 0, relative = 0;
	int conf_nodes = number_of_configured_nodes();
	int task_nodes = number_of_task_nodes();
	char *end; 
	struct bitmask *nodebuf;

	nodebuf = allocate_nodemask();

	if (s[0] == 0) 
		return numa_no_nodes;
	if (*s == '!') { 
		invert = 1;
		s++;
	}
	if (*s == '+') {
		relative++;
		s++;
	}
	do {
		unsigned long arg;
		int i;
		if (!strcmp(s,"all")) { 
			int i;
			for (i = 0; i < task_nodes; i++)
				bitmask_setbit(nodebuf, i);
			s+=4;
			break;
		}
		arg = get_nr(s, &end, numa_all_nodes, relative);
		if (end == s)
			complain("unparseable node description `%s'\n", s);
		if (arg > maxnode)
			complain("node argument %d is out of range\n", arg);
		i = arg;
		bitmask_setbit(nodebuf, i);
		s = end; 
		if (*s == '-') { 
			char *end2;
			unsigned long arg2;
			arg2 = get_nr(++s, &end2, numa_all_nodes, relative);
			if (end2 == s)
				complain("missing node argument %s\n", s);
			if (arg2 >= task_nodes)
				complain("node argument %d out of range\n", arg2);
			while (arg <= arg2) {
				i = arg;
				if (bitmask_isbitset(numa_all_nodes, i))
					bitmask_setbit(nodebuf, i);
				arg++;
			}
			s = end2;
		}
	} while (*s++ == ','); 
	if (s[-1] != '\0')
		usage();
	if (invert) { 
		int i;
		for (i = 0; i < conf_nodes; i++) {
			if (bitmask_isbitset(nodebuf, i))
				bitmask_clearbit(nodebuf, i);
			else
				bitmask_setbit(nodebuf, i);
		}
	} 
	return nodebuf;
}

void complain(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt); 
	fprintf(stderr, "numactl: ");
	vfprintf(stderr,fmt,ap); 
	putchar('\n');
	va_end(ap); 
	exit(1); 
}

void nerror(char *fmt, ...) 
{ 
	int err = errno;
	va_list ap;
	va_start(ap,fmt); 
	fprintf(stderr, "numactl: ");
	vfprintf(stderr, fmt, ap); 
	va_end(ap);
	if (err) 
		fprintf(stderr,": %s\n", strerror(err));
	else
		fputc('\n', stderr);
	exit(1);
} 

long memsize(char *s) 
{ 
	char *end;
	long length = strtoul(s,&end,0);
	switch (toupper(*end)) { 
	case 'G': length *= 1024;  /*FALL THROUGH*/
	case 'M': length *= 1024;  /*FALL THROUGH*/
	case 'K': length *= 1024; break;
	}
	return length; 
} 


static struct policy { 
	char *name;
	int policy; 
	int noarg; 
} policies[] = { 
	{ "interleave", MPOL_INTERLEAVE, }, 
	{ "membind",    MPOL_BIND, }, 
	{ "preferred",   MPOL_PREFERRED, }, 
	{ "default",    MPOL_DEFAULT, 1 }, 
	{ NULL },
};

static char *policy_names[] = { "default", "preferred", "bind", "interleave" }; 

char *policy_name(int policy)
{
	static char buf[32];
	if (policy >= array_len(policy_names)) {
		sprintf(buf, "[%d]", policy); 
		return buf;
	}
	return policy_names[policy];
}

int parse_policy(char *name, char *arg) 
{ 
	int k;
	struct policy *p = NULL; 
	if (!name) 
		return MPOL_DEFAULT;
	for (k = 0; policies[k].name; k++) { 
		p = &policies[k];
		if (!strcmp(p->name, name))
			break; 
	}
	if (!p || !p->name || (!arg && !p->noarg)) 
		usage(); 
    return p->policy;
} 

void print_policies(void)
{
	int i;
	printf("Policies:");
	for (i = 0; policies[i].name; i++) 
		printf(" %s", policies[i].name);
	printf("\n"); 
}
