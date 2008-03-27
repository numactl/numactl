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

void printmask(char *name, nodemask_t *mask)
{ 
	int i;
	int max = numa_max_node();
	printf("%s: ", name); 
#if 0
	int full = 1;
	for (i = 0; i <= max; i++) { 
		if (nodemask_isset(&numa_all_nodes, i) && !nodemask_isset(mask, i)) {
			full = 0;
			break;
		}		
	} 
	if (full) { 
		printf("all nodes\n"); 
		return;
	}	
#endif
	for (i = 0; i <= max; i++) 
		if (nodemask_isset(mask, i))
			printf("%d ", i); 
	putchar('\n');
} 

/*
 * Extract a node or processor number from the given string.
 * Allow a relative node / processor specification within the allowed
 * set if a + is prepended to the number.
 */
unsigned long get_nr(char *s, char **end, int max, unsigned long *mask)
{
	unsigned long i, nr;

	if (*s != '+')
		return strtoul(s, end, 0);
	s++;
	nr = strtoul(s, end, 0);
	if (s == *end)
		return nr;
	/* Find the nth set bit */
	for (i = 0; nr > 0 && i <= max; i++)
		if (test_bit(i, mask))
			nr--;
	if (nr)
		*end = s;
	return i;

}

int numcpus; 
extern unsigned long numa_all_cpus[];
extern int maxcpus;

/* caller must free buffer */
unsigned long *cpumask(char *s, int *ncpus) 
{
	int invert = 0, relative=0;
	char *end; 

	int cpubufsize = round_up(maxcpus, BITS_PER_LONG) / 8;
	unsigned long *cpubuf = calloc(cpubufsize,1); 
	if (!cpubuf) 
		complain("Out of memory");

	if (s[0] == 0) 
		return cpubuf;
	if (*s == '!') { 
		invert = 1;
		++s;
	}
	do {		
		unsigned long arg;

		if (!strcmp(s,"all")) { 
			int i;
			for (i = 0; i < numcpus; i++)
				set_bit(i, cpubuf);
			break;
		}
		if (*s == '+') relative++;
		arg = get_nr(s, &end, maxcpus, numa_all_cpus);
		if (end == s)
			complain("unparseable node description `%s'\n", s);
		if (arg > maxcpus)
			complain("cpu argument %d is out of range\n", arg);
		set_bit(arg, cpubuf);
		s = end; 
		if (*s == '-') {
			char *end2;
			unsigned long arg2;
			if (relative && *(s+1) != '+') {
				*s = '+';
				arg2 = get_nr(s,&end2,maxcpus,numa_all_cpus);
			} else {
				arg2 = get_nr(++s,&end2,maxcpus,numa_all_cpus);
			}
			if (end2 == s)
				complain("missing cpu argument %s\n", s);
			if (arg2 > maxcpus)
				complain("cpu argument %d out of range\n",arg2);
			while (arg <= arg2) {
				if (test_bit(arg, numa_all_cpus))
					set_bit(arg, cpubuf);
				arg++;
			}
			s = end2;
		}
	} while (*s++ == ','); 
	if (s[-1] != '\0')
		usage();
	if (invert) { 
		int i;
		for (i = 0; i <= maxcpus; i++) {
			if (test_bit(i, cpubuf))
				clear_bit(i, cpubuf);
			else
				set_bit(i, cpubuf);
		}
	} 
	*ncpus = cpubufsize;
	return cpubuf;	
}

void printcpumask(char *name, unsigned long *mask, int size)
{ 
	int i;
	printf("%s: ", name);
	for (i = 0; i < size*8; i++) {
		if (test_bit(i, mask))
			printf("%d ", i);
	}
	putchar('\n');
} 

nodemask_t nodemask(char *s) 
{ 
	int max = numa_max_node();
	nodemask_t mask;
	int invert = 0;
	char *end; 
	nodemask_zero(&mask);
	if (s[0] == 0) 
		return numa_no_nodes; 
	if (*s == '!') { 
		invert = 1;
		++s;
	}
	do {		
		unsigned long arg;

		if (!strcmp(s,"all")) { 
			s += 4;
			mask = numa_all_nodes;
			break;
		}
		arg = get_nr(s, &end, max, (unsigned long *)numa_all_nodes.n);
		if (end == s)
			complain("unparseable node description `%s'\n", s);
		if (arg > max)
			complain("node argument %d is out of range\n", arg);
		nodemask_set(&mask, arg);
		s = end; 
		if (*s == '-') { 
			char *end2;
			unsigned long arg2 = get_nr(++s, &end2, max,
					(unsigned long *)numa_all_nodes.n);
			if (end2 == s)
				complain("missing cpu argument %s\n", s);
			if (arg2 > max)
				complain("node argument %d out of range\n",arg2);
			while (arg <= arg2) {
				if (nodemask_isset(&numa_all_nodes, arg))
					nodemask_set(&mask, arg);
				arg++;
			}
			s = end2;
		}
	} while (*s++ == ','); 
	if (s[-1] != '\0')
		usage();
	if (invert) { 
		int i;
		for (i = 0; i <= max; i++) {
			if (!nodemask_isset(&numa_all_nodes, i))
				continue;
			if (nodemask_isset(&mask, i))
				nodemask_clr(&mask, i);
			else
				nodemask_set(&mask, i); 
		}
	} 
	return mask;
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

