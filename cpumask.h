
#ifdef __x86_64__
#define CPU_MAX 64
#else
#define CPU_MAX 256 
#endif

#define CPUMASK_NWORDS (CPU_MAX/(sizeof(long)*8))

typedef struct { 
	unsigned long m[CPUMASK_NWORDS];
} cpumask_t;

static inline int cpumask_empty(cpumask_t *a)
{ 
	int i;
	for (i = 0; i < CPUMASK_NWORDS; i++) 
		if (a->m[i]) return 0;
	return 1;
} 

static inline void cpumask_or(cpumask_t *dst, cpumask_t *a, cpumask_t *b)
{ 
	int i;
	for (i = 0; i < CPUMASK_NWORDS; i++) 
		dst->m[i] = a->m[i] | b->m[i];
} 

static inline void cpumask_set(cpumask_t *dst, int field)
{ 
	dst->m[field / (sizeof(long)*8)] |= (1UL << (field % (sizeof(long)*8)));
} 

static inline void cpumask_zero(cpumask_t *dst)
{ 
	int i;
	for (i = 0; i < CPUMASK_NWORDS; i++) 
		dst->m[i] = 0;

} 

static inline void cpumask_fill(cpumask_t *dst)
{
	int i;
	for (i = 0; i < CPUMASK_NWORDS; i++) 
		dst->m[i] = -1UL;
}



