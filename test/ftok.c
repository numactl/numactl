#include <stdio.h>
#include <sys/ipc.h>
int main(int ac, char **av)
{
	while (*++av)
		printf("0x%x\n", ftok(*av, 0));
	return 0;
}
