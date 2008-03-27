/* Test numa_distance */
#include <numa.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	int numnodes, a, b;
	if (numa_available() < 0) {
		printf("no numa support in kernel\n");
		exit(1);
	}

        numnodes = numa_num_configured_nodes();
	for (a = 0; a < numnodes; a++) { 
		printf("%03d: ", a); 
		if (numa_distance(a, a) != 10) { 
			printf("%d: self distance is not 10 (%d)\n", 
			       a, numa_distance(a,a));
			exit(1);
		}
		for (b = 0; b < numnodes; b++) { 
			int d1 = numa_distance(a, b); 
			int d2 = numa_distance(b, a);
			printf("%03d ", d1);
			if (d1 != d2) {
				printf("\n(%d,%d)->(%d,%d) wrong!\n",a,b,d1,d2); 
				exit(1);
			}
		}
		printf("\n");
	} 
	return 0;	
}

