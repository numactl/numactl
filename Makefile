CFLAGS := -g -Wall -I. -O2

CLEANFILES := libnuma.a numactl.o libnuma.o numactl numademo numademo.o \
	      memhog libnuma.so libnuma.so.1 numamon numamon.o syscall.o \
	      memhog.o stream util.o stream_main.o stream_lib.o shm.o \
	      test/pagesize test/tshared test/mynode.o test/tshared.o \
	      test/tshm test/mynode test/ftok test/prefered test/randmap

prefix := /usr
libdir := ${prefix}$(shell if [ -d /usr/lib64 ] ; then echo "/lib64" ; else echo "/lib"  ; fi)
docdir := ${prefix}/share/doc

all: numactl libnuma.so numademo numamon memhog stream test/tshared \
     test/mynode test/pagesize test/ftok test/prefered test/randmap

numactl: numactl.o libnuma.so util.o shm.o
	${CC} ${LDFLAGS} -o numactl numactl.o shm.o util.o -L. -lnuma

util.o: util.c util.h numa.h

shm.o: shm.h util.h

memhog: util.o memhog.o
	${CC} ${LDFLAGS} -o memhog $^ -L. -lnuma

stream: util.o stream_lib.o stream_main.o libnuma.so
	${CC} ${LDFLAGS} -o stream $^ -lm 

numactl.o: numactl.c numa.h numaif.h util.h

numademo: numademo.o util.o libnuma.so
	${CC} ${LDFLAGS} -o numademo $^

numademo.o: numa.h numademo.c	

numamon: numamon.o 
	${CC} ${LDFLAGS} -o numamon numamon.o

libnuma.so.1: libnuma.o syscall.o
	${CC} -shared -Wl,-soname=libnuma.so.1 -o libnuma.so.1 $^

libnuma.so: libnuma.so.1
	ln -sf libnuma.so.1 libnuma.so

libnuma.o : CFLAGS += -fPIC

syscall.o : CFLAGS += -fPIC

test/tshared: test/tshared.o
	${CC} ${LDFLAGS} -o test/tshared test/tshared.o -L. -lnuma

test/mynode: test/mynode.o
	${CC} ${LDFLAGS} -o test/mynode test/mynode.o -L. -lnuma

test/pagesize: test/pagesize.c
	${CC} ${CFLAGS} -o test/pagesize test/pagesize.c

test/prefered: test/prefered.c
	${CC} ${CFLAGS} -o test/prefered test/prefered.c -I. -L. -lnuma

test/ftok: test/ftok.c
	${CC} ${CFLAGS} -o test/ftok test/ftok.c

test/randmap: test/randmap.c
	${CC} ${CFLAGS} -o test/randmap test/randmap.c -I. -L. -lnuma

.PHONY: install all clean html

MANLINKS := \
all_nodes alloc alloc_interleaved alloc_interleaved_subset alloc_local \
alloc_onnode available bind error exit_on_error free get_interleave_mask \
get_interleave_node get_membind get_run_node_mask interleave_memory max_node \
no_nodes node_size node_to_cpus police_memory preferred run_on_node \
run_on_node_mask set_bind_policy  set_interleave_mask set_localalloc \
set_membind set_preferred set_strict setlocal_memory tonode_memory \
tonodemask_memory 


MANPAGES := numa.3 numactl.8 mbind.2 set_mempolicy.2 get_mempolicy.2	 

install: numactl numademo.c numamon memhog libnuma.so.1 numa.h numaif.h numastat ${MANPAGES}
	cp numactl ${prefix}/bin
	cp numademo ${prefix}/bin
	cp memhog ${prefix}/usr/bin
	cp set_mempolicy.2 ${prefix}/share/man/man2
	cp mbind.2 ${prefix}/share/man/man2
	cp numactl.8 ${prefix}/share/man/man8
	cp numa.3 ${prefix}/share/man/man3
	( cd ${prefix}/share/man/man3 ; for i in ${MANLINKS} ; do ln -s numa.3 numa_$$i.3 ; done )
	cp libnuma.so.1 ${libdir}
	cd ${libdir} ; ln -s libnuma.so.1 libnuma.so
	cp numa.h numaif.h ${prefix}/include
	cp numastat ${prefix}/bin
	if [ -d ${docdir} ] ; then \
		mkdir -p ${docdir}/numactl/examples ; \
		cp numademo.c ${docdir}/examples/numactl ; \
	fi	

clean: 
	rm -f ${CLEANFILES}
	@rm -rf html

# should add proper deps here.
html: 
	if [ ! -d html ] ; then mkdir html ; fi
	groff -Thtml -man numactl.8 > html/numactl.html
	groff -Thtml -man numa.3 > html/numa.html
	groff -Thtml -man mbind.2 > html/mbind.html
	groff -Thtml -man get_mempolicy.2 > html/get_mempolicy.html
	groff -Thtml -man set_mempolicy.2 > html/set_mempolicy.html

