/* Clear the CPU cache for benchmark purposes. Pretty simple minded.
 * Might not work in some complex cache topologies.
 * When you switch CPUs it's a good idea to clear the cache after testing
 * too.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "clearcache.h"

static unsigned cache_size(void)
{
	unsigned cs = 0;
#ifdef _SC_LEVEL1_DCACHE_SIZE
	cs += sysconf(_SC_LEVEL1_DCACHE_SIZE);
#endif
#ifdef _SC_LEVEL2_CACHE_SIZE
	cs += sysconf(_SC_LEVEL2_CACHE_SIZE);
#endif
#ifdef _SC_LEVEL3_CACHE_SIZE
	cs += sysconf(_SC_LEVEL3_CACHE_SIZE);
#endif
#ifdef _SC_LEVEL4_CACHE_SIZE
	cs += sysconf(_SC_LEVEL4_CACHE_SIZE);
#endif
	if (cs == 0) {
		static int warned;
		if (!warned) {
			printf("Cannot determine CPU cache size\n");
			warned = 1;
		}
		cs = 64*1024*1024;
	}
	cs *= 2; /* safety factor */

	return cs;
}

static void fallback_clearcache(void)
{
	static unsigned char *clearmem;
	unsigned cs = cache_size();
	unsigned i;

	if (!clearmem)
		clearmem = malloc(cs);
	if (!clearmem) {
		printf("Warning: cannot allocate %u bytes of clear cache buffer\n", cs);
		return;
	}
	for (i = 0; i < cs; i += 32)
		clearmem[i] = 1;
}

void clearcache(unsigned char *mem, unsigned size)
{
     __builtin___clear_cache(mem, (mem + (size-1)));
}
