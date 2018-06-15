/* Unit test bitmap parser */
#define _GNU_SOURCE 1
//#include <asm/bitops.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include "numa.h"
#include "util.h"

/* For util.c. Fixme. */
void usage(void)
{
	exit(1);
}

#define ALIGN(x,a) (((x)+(a)-1)&~((a)-1))

#define test_bit(i,p)  ((p)[(i) / BITS_PER_LONG] &   (1UL << ((i)%BITS_PER_LONG)))
#define set_bit(i,p)   ((p)[(i) / BITS_PER_LONG] |=  (1UL << ((i)%BITS_PER_LONG)))
#define clear_bit(i,p) ((p)[(i) / BITS_PER_LONG] &= ~(1UL << ((i)%BITS_PER_LONG)))

typedef unsigned u32;
#define BITS_PER_LONG (sizeof(long)*8)

#define round_up(x,y) (((x) + (y) - 1) & ~((y)-1))

#define CPU_BYTES(x) (round_up(x, BITS_PER_LONG)/8)
#define CPU_LONGS(x) (CPU_BYTES(x) / sizeof(long))

/* Following routine extracted from Linux 2.6.16 */

#define CHUNKSZ                         32
#define nbits_to_hold_value(val)        fls(val)
#define unhex(c)                        (isdigit(c) ? (c - '0') : (toupper(c) - 'A' + 10))
#define BASEDEC 10              /* fancier cpuset lists input in decimal */

/**
 * bitmap_scnprintf - convert bitmap to an ASCII hex string.
 * @buf: byte buffer into which string is placed
 * @buflen: reserved size of @buf, in bytes
 * @mask: pointer to struct bitmask to convert
 *
 * Hex digits are grouped into comma-separated sets of eight digits per set.
 */
int bitmap_scnprintf(char *buf, unsigned int buflen, struct bitmask *mask)
{
        int i, word, bit, len = 0;
        unsigned long val;
        const char *sep = "";
        int chunksz;
        u32 chunkmask;

        chunksz = mask->size & (CHUNKSZ - 1);
        if (chunksz == 0)
                chunksz = CHUNKSZ;

        i = ALIGN(mask->size, CHUNKSZ) - CHUNKSZ;
        for (; i >= 0; i -= CHUNKSZ) {
                chunkmask = ((1ULL << chunksz) - 1);
                word = i / BITS_PER_LONG;
                bit = i % BITS_PER_LONG;
                val = (mask->maskp[word] >> bit) & chunkmask;
                len += snprintf(buf+len, buflen-len, "%s%0*lx", sep,
                        (chunksz+3)/4, val);
                chunksz = CHUNKSZ;
                sep = ",";
        }
        return len;
}

extern int numa_parse_bitmap(char  *buf, struct bitmask *mask);
#define MASKSIZE 300

int main(void)
{
	char buf[1024];
	struct bitmask *mask, *mask2;
	int i;

	mask  = numa_bitmask_alloc(MASKSIZE);
	mask2 = numa_bitmask_alloc(MASKSIZE);

	printf("Testing bitmap functions\n");
	for (i = 0; i < MASKSIZE; i++) {
		numa_bitmask_clearall(mask);
		numa_bitmask_clearall(mask2);
		numa_bitmask_setbit(mask, i);
		assert(find_first(mask) == i);
		bitmap_scnprintf(buf, sizeof(buf), mask);
		strcat(buf,"\n");
		if (numa_parse_bitmap(buf, mask2) < 0)
			assert(0);
		if (memcmp(mask->maskp, mask2->maskp, numa_bitmask_nbytes(mask))) {
			bitmap_scnprintf(buf, sizeof(buf), mask2);
			printf("mask2 differs: %s\n", buf);
			assert(0);
		}
	}
	printf("Passed\n");
	return 0;
}
