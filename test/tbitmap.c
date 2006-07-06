/* Unit test bitmap parser */
#define _GNU_SOURCE 1
//#include <asm/bitops.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

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
 * @maskp: pointer to bitmap to convert
 * @nmaskbits: size of bitmap, in bits
 *
 * Exactly @nmaskbits bits are displayed.  Hex digits are grouped into
 * comma-separated sets of eight digits per set.
 */
int bitmap_scnprintf(char *buf, unsigned int buflen,
        const unsigned long *maskp, int nmaskbits)
{
        int i, word, bit, len = 0;
        unsigned long val;
        const char *sep = "";
        int chunksz;
        u32 chunkmask;

        chunksz = nmaskbits & (CHUNKSZ - 1);
        if (chunksz == 0)
                chunksz = CHUNKSZ;

        i = ALIGN(nmaskbits, CHUNKSZ) - CHUNKSZ;
        for (; i >= 0; i -= CHUNKSZ) {
                chunkmask = ((1ULL << chunksz) - 1);
                word = i / BITS_PER_LONG;
                bit = i % BITS_PER_LONG;
                val = (maskp[word] >> bit) & chunkmask;
                len += snprintf(buf+len, buflen-len, "%s%0*lx", sep,
                        (chunksz+3)/4, val);
                chunksz = CHUNKSZ;
                sep = ",";
        }
        return len;
}

extern int numa_parse_bitmap(char  *buf,unsigned long *mask, unsigned bits);

int main(void)
{
	char buf[1024];
	unsigned long mask[20], mask2[20];
	int i;
	printf("Testing bitmap functions\n");
	for (i = 0; i < 300; i++) {
		memset(mask, 0, sizeof(mask));
		memset(mask2, 0, sizeof(mask));
		set_bit(i, mask);
		bitmap_scnprintf(buf, sizeof(buf), mask, 300);
		strcat(buf,"\n");
		if (numa_parse_bitmap(buf, mask2, 300) < 0)
			assert(0);
		if (memcmp(mask, mask2, sizeof(mask))) { 
			bitmap_scnprintf(buf, sizeof(buf), mask2, 300);	
			printf("mask2 differs: %s\n", buf);
			assert(0);
		}
	}
	printf("Passed\n");
	return 0;
}

