#include "bitops.h"

/* extremly dumb */
int find_first_bit(void *m, int max)
{
	unsigned long *mask = m;
	int i;
	for (i = 0; i < max; i++) {
		if (test_bit(i, mask))
			break;			
	}
	return i;
}
