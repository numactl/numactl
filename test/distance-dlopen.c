/* Exercise the distance cache across libnuma unload/reload cycles.
 * A sanitizer build also checks that dlclose releases the cached table.
 */
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef LIBNUMA_PATH
#define LIBNUMA_PATH "libnuma.so.1"
#endif

static int run_cycle(void)
{
	void *handle = dlopen(LIBNUMA_PATH, RTLD_NOW | RTLD_LOCAL);
	int (*available)(void);
	int (*distance)(int, int);
	int result = 0;

	if (!handle) {
		fprintf(stderr, "dlopen: %s\n", dlerror());
		return 1;
	}
	available = dlsym(handle, "numa_available");
	distance = dlsym(handle, "numa_distance");
	if (!available || !distance) {
		fprintf(stderr, "dlsym: %s\n", dlerror());
		result = 1;
	} else if (available() < 0) {
		result = 77;
	} else if (distance(0, 0) != 10 || distance(-1, 0) != 0) {
		fprintf(stderr, "unexpected NUMA distance\n");
		result = 1;
	}
	if (dlclose(handle)) {
		fprintf(stderr, "dlclose: %s\n", dlerror());
		return 1;
	}
	return result;
}

int main(void)
{
	int i, result;
	int (*check_leaks)(void) = dlsym(RTLD_DEFAULT,
				       "__lsan_do_recoverable_leak_check");

	if (access(LIBNUMA_PATH, F_OK) && errno == ENOENT) {
		puts("shared libnuma not built");
		return 77;
	}
	for (i = 0; i < 10; i++) {
		result = run_cycle();
		if (result)
			return result;
	}
	/* run_cycle() has returned, so no library pointer remains on its stack. */
	if (check_leaks && check_leaks())
		return 1;
	return 0;
}
