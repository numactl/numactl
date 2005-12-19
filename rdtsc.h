/* Read time stamp counters on various architectures */
#if defined(__x86_64__) || defined(__i386__)
#define rdtsc(val) do { \
	unsigned low, high; 				\
	asm volatile("rdtsc" : "=a" (low), "=d" (high)); \
	val = (((unsigned long long)high)<<32) | low;	\
} while(0)
#elif defined(__ia64__)
#define rdtsc(val) do { \
	asm volatile("mov %0=ar.itc" : "=r"(val)); \
} while(0)
#elif defined(__ppc__) || defined(__ppc64__)
#define rdtsc(val) asm volatile("mftb %0" : "=r" (val))
#else
#error implement rdtsc for your architecture
#endif
