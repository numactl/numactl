/* Somewhat silly multithreaded memcpy */
#include <pthread.h>
#include <numa.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MB (1024*1024)

int max_node; 
long size = 100 * MB;

struct copyargs { 
	char *src; 
	char *dst; 
	int size; 
	int node;
	int blocksize;
	int step;
}; 

volatile unsigned start_lock;
volatile unsigned notrunning; 
volatile unsigned notfinished;

static inline unsigned long rdtsc(void)
{ 
	unsigned low, high; 
	asm volatile("rdtsc" : "=a" (low), "=d" (high));
	return ((unsigned long)high << 32) | low; 
} 

static inline void atomic_dec(volatile unsigned *val) 
{ 
	asm("lock ; decl %0" : "+m" (*val)); 
} 

static inline void do_copy(struct copyargs *args)
{ 
	int n = 0;
	int blocksize = args->blocksize; 
	int step = args->step; 
	char *src = args->src;
	char *dst = args->dst;
	while (n < args->size) { 
		memcpy(dst, src, blocksize);	/* inline would be better */
		src += step;
		dst += step;
		n += blocksize;
	}  
} 

void *copythread(void *argp)
{ 
	struct copyargs *args = argp;

	numa_run_on_node(args->node); 
	atomic_dec(&notrunning);

	/* spin on lock until we can start */ 
	while (start_lock)
		;

	do_copy(args); 	

	atomic_dec(&notfinished); 
	return NULL;
} 

void test(void)
{ 
	void *mem; 
	int i;
	int stride, stride_init; 
	unsigned long start,end;
	int page_size = getpagesize();

	if (numa_available() < 0) { 
		fprintf(stderr, "Your kernel doesn't support NUMA policy.\n");
		exit(1);
	} 
	max_node = numa_max_node(); 
	stride_init = numa_get_interleave_node(); 
	start_lock = 1;

	mem = numa_alloc_interleaved(size); 
	if (!mem) 
		exit(1);

	numa_run_on_node(stride_init); 
	memset(mem, 0xff, size/2); 
	memcpy(mem+size/2, mem, size/2); /* prime the dynamic linker on memcpy */

	pthread_t thr[max_node];
	struct copyargs args[max_node];
	stride = stride_init;
	int offset = 0;
	for (i = 0; i < max_node; i++) {
		args[i].node = stride; 	
		stride = (1+stride)%max_node;
		args[i].step = page_size * max_node; 
		args[i].blocksize = page_size; 
		args[i].src = mem + offset;
		args[i].dst = mem + size/2 + offset;
		args[i].size = (size/2)/max_node;
		offset += page_size;
		if (i != stride_init) {
			notrunning++;
			notfinished++;
			pthread_create(&thr[i], NULL, copythread, &args);		
		}	
	} 

	/* Wait for the threads to startup */ 
	while (notrunning > 0)
		;

	/* Start the copy */
	
	start = rdtsc(); 
	start_lock = 0; 		/* let others start */ 
	do_copy(&args[0]);  
	while (notfinished > 0) /* wait for others */
		; 
	end = rdtsc(); 

	for (i = 0; i < max_node; i++)
		pthread_join(thr[i], NULL); 

	printf("interleaved %ld cycles (%.3f/MB)\n", end-start, 
	       (float)(end-start)/((float)size/MB));

	args[0].src = mem; 
	args[0].dst = mem+size/2; 
	args[0].step = page_size;
	args[0].size = size/2; 
	args[0].blocksize = page_size; 
	start = rdtsc();
	do_copy(&args[0]);
	end = rdtsc(); 
	printf("single threaded %ld cycles (%.3f/MB)\n",
	       end-start,
	       (float)(end-start)/((float)size/MB)); 
	numa_free(mem, size); 

	mem = numa_alloc_local(size); 
	args[0].src = mem; 
	args[0].dst = mem+size/2; 
	args[0].step = page_size;
	args[0].size = size/2; 
	args[0].blocksize = page_size; 
	start = rdtsc();
	do_copy(&args[0]);
	end = rdtsc(); 
	printf("local %ld cycles (%.3f/MB)\n",
	       end-start,
	       (float)(end-start)/((float)size/MB)); 

	start = rdtsc();
	memcpy(mem+size/2, mem, size/2); 	
	end = rdtsc(); 
	printf("optimized local %ld cycles (%.3f/MB)\n",
	       end-start,
	       (float)(end-start)/((float)size/MB)); 

	numa_free(mem, size); 
}

int main(void)
{ 
	long i;
	for (i = 4096*4; i <= 2UL*1024*MB; i<<=1) { 
		printf("\nsize %ld bytes (%.3f MB)\n", i, i/(1024.0*1024.0));
		size = i;
		test();
	} 

	return 0;
} 
