/* Copyright (C) 2003,2004 Andi Kleen, SuSE Labs.

   libnuma is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; version
   2.1.

   libnuma is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should find a copy of v2.1 of the GNU Lesser General Public License
   somewhere on your Linux system; if not, write to the Free Software 
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#include <unistd.h>
#include <sys/types.h>
#include <asm/unistd.h>
#include <errno.h>
#include "numaif.h"
#include "numaint.h"

#define WEAK __attribute__((weak))

/* Define stub for dummy calls (e.g. for compile testing) */
#ifndef STUB 

#if defined(__x86_64__)

#define __NR_sched_setaffinity    203
#define __NR_sched_getaffinity     204

/* Official allocation */

#define __NR_mbind 237
#define __NR_set_mempolicy 238
#define __NR_get_mempolicy 239

#elif defined __ia64__
#define __NR_sched_setaffinity    1231
#define __NR_sched_getaffinity    1232

/* Official allocation */

#define __NR_mbind 1259
#define __NR_get_mempolicy 1260
#define __NR_set_mempolicy 1261

#elif defined __i386__

/* semi-official allocation (in -mm) */

#define __NR_mbind 274
#define __NR_get_mempolicy 275
#define __NR_set_mempolicy 276

#else
#error "Add syscalls for your architecture"
#endif

#endif

#ifdef __x86_64__
/* 6 argument calls on x86-64 are often buggy in both glibc and
   asm/unistd.h. Add a working version here. */
long syscall6(long call, long a, long b, long c, long d, long e, long f)
{ 
       long res;
       asm volatile ("movq %[d],%%r10 ; movq %[e],%%r8 ; movq %[f],%%r9 ; syscall" 
		     : "=a" (res)
		     : "0" (call),"D" (a),"S" (b), "d" (c), 
		       [d] "g" (d), [e] "g" (e), [f] "g" (f) :
		     "r11","rcx","r8","r10","r9","memory" );
       if (res < 0) { 
	       errno = -res; 
	       res = -1; 
       } 
       return res;
} 
#else
#define syscall6 syscall
#endif

#ifdef STUB 
#define syscall_or_stub(x...) (errno = ENOSYS, -1) 
#define syscall6_or_stub(x...) (errno = ENOSYS, -1)
#else
#define syscall_or_stub(x...) syscall(x)
#define syscall6_or_stub(x...) syscall6(x) 
#endif

long WEAK get_mempolicy(int *policy, 
		   unsigned long *nmask, unsigned long maxnode,
		   void *addr, int flags)          
{
	return syscall_or_stub(__NR_get_mempolicy, policy, nmask, maxnode, addr, flags);
}

long WEAK mbind(void *start, unsigned long len, int mode, 
	   unsigned long *nmask, unsigned long maxnode, unsigned flags) 
{
	return syscall6_or_stub(__NR_mbind, (long)start, len, mode, (long)nmask, maxnode, flags); 
}

long WEAK set_mempolicy(int mode, unsigned long *nmask, 
                                   unsigned long maxnode)
{
	return syscall_or_stub(__NR_set_mempolicy,mode,nmask,maxnode);
}

/* SLES8 glibc doesn't define those */

int numa_sched_setaffinity(pid_t pid, unsigned len, unsigned long *mask)
{
	return syscall(__NR_sched_setaffinity,pid,len,mask);
}

int numa_sched_getaffinity(pid_t pid, unsigned len, unsigned long *mask)
{
	return syscall(__NR_sched_getaffinity,pid,len,mask);

}
