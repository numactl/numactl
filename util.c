#include "numa.h"
#include "numaif.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

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
		arg = strtoul(s, &end, 0); 
		if (end == s)
			complain("unparseable node description `%s'\n", s);
		if (arg > max)
			complain("node argument %d is out of range\n", arg);
		nodemask_set(&mask, arg);
		s = end; 
		if (*s == '-') { 
			char *end2;
			unsigned long arg2 = strtoul(++s, &end2, 0); 
			if (end2 == s)
				complain("missing cpu argument %s\n", s);
			if (arg > max)
				complain("node argument %d out of range\n", arg);
			while (++arg <= arg2)
				nodemask_set(&mask, arg);
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
	case 'G': length *= 1024; 
	case 'M': length *= 1024; 
	case 'K': length *= 1024; 
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

char *policy_names[] = { "default", "preferred", "bind", "interleave" }; 

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

#if 0
/* Not used currently, but can be useful in the future again */
int read_sysctl(char *name) 
{ 
	int ret = -1;
	char *line = NULL; 
	size_t linesz = 0;
	FILE *f = fopen(name, "r"); 
	if (f) { 
		getline(&line, &linesz, f); 
		if (line) { 
			ret = atoi(line); 
			free(line);
		}
		fclose(f);
	}
	return ret;
}
#endif
