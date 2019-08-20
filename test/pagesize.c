#include <stdio.h>
#include <unistd.h>

int main(void)
{
	printf("%d\n", getpagesize());
	return 0;
}
