CFLAGS := -g -Wall -I. -O2

CLEANFILES := libnuma.a numactl.o libnuma.o numactl numademo numademo.o \
	      memhog libnuma.so libnuma.so.1 numamon numamon.o syscall.o bitops.o \
	      memhog.o stream util.o stream_main.o stream_lib.o shm.o \
	      test/pagesize test/tshared test/mynode.o test/tshared.o \
	      test/tshm test/mynode test/ftok test/prefered test/randmap \
		  .depend .depend.X test/nodemap

SOURCES := bitops.c libnuma.c memhog.c numactl.c numademo.c \
	numamon.c shm.c stream_lib.c stream_main.c syscall.c util.c \
	test/*.c

prefix := /usr
libdir := ${prefix}$(shell if [ -d /usr/lib64 ] ; then echo "/lib64" ; else echo "/lib"  ; fi)
docdir := ${prefix}/share/doc

all: numactl libnuma.so numademo numamon memhog stream test/tshared \
     test/mynode test/pagesize test/ftok test/prefered test/randmap \
	 test/nodemap

numactl: numactl.o util.o shm.o bitops.o libnuma.so

util.o: util.c util.h numa.h

shm.o: shm.h util.h bitops.h

memhog: util.o memhog.o libnuma.so

stream: util.o stream_lib.o stream_main.o libnuma.so

numactl.o: numactl.c numa.h numaif.h util.h

numademo: LDFLAGS += -lm

numademo: numademo.o util.o stream_lib.o libnuma.so

numademo.o: numa.h numademo.c libnuma.so	

numamon: numamon.o

libnuma.so.1: libnuma.o syscall.o
	${CC} -shared -Wl,-soname=libnuma.so.1 -o libnuma.so.1 $^

libnuma.so: libnuma.so.1
	ln -sf libnuma.so.1 libnuma.so

libnuma.o : CFLAGS += -fPIC

syscall.o : CFLAGS += -fPIC

test/tshared: test/tshared.o libnuma.so

test/mynode: test/mynode.o libnuma.so

test/pagesize: test/pagesize.c libnuma.so

test/prefered: test/prefered.c libnuma.so

test/ftok: test/ftok.c libnuma.so

test/randmap: test/randmap.c libnuma.so

test/nodemap: test/nodemap.c libnuma.so

.PHONY: install all clean html depend

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
	cp memhog ${prefix}/bin
	cp set_mempolicy.2 ${prefix}/share/man/man2
	cp mbind.2 ${prefix}/share/man/man2
	cp numactl.8 ${prefix}/share/man/man8
	cp numa.3 ${prefix}/share/man/man3
	( cd ${prefix}/share/man/man3 ; for i in ${MANLINKS} ; do ln -sf numa.3 numa_$$i.3 ; done )
	cp libnuma.so.1 ${libdir}
	cd ${libdir} ; ln -sf libnuma.so.1 libnuma.so
	cp numa.h numaif.h ${prefix}/include
	cp numastat ${prefix}/bin
	if [ -d ${docdir} ] ; then \
		mkdir -p ${docdir}/numactl/examples ; \
		cp numademo.c ${docdir}/numactl/examples ; \
	fi	

HTML := html/numactl.html html/numa.html html/mbind.html html/get_mempolicy.html html/set_mempolicy.html

clean: 
	rm -f ${CLEANFILES}
	@rm -rf html

html: ${HTML} 

html/numactl.html: numactl.8
	if [ ! -d html ] ; then mkdir html ; fi
	groff -Thtml -man numactl.8 > html/numactl.html

html/mbind.html: mbind.2
	if [ ! -d html ] ; then mkdir html ; fi
	groff -Thtml -man mbind.2 > html/mbind.html

html/numa.html: numa.3
	if [ ! -d html ] ; then mkdir html ; fi
	groff -Thtml -man numa.3 > html/numa.html

html/get_mempolicy.html: get_mempolicy.2
	if [ ! -d html ] ; then mkdir html ; fi
	groff -Thtml -man get_mempolicy.2 > html/get_mempolicy.html

html/set_mempolicy.html: set_mempolicy.2
	if [ ! -d html ] ; then mkdir html ; fi
	groff -Thtml -man set_mempolicy.2 > html/set_mempolicy.html

depend: .depend

.depend:
	${CC} -MM -I. ${SOURCES} > .depend.X && mv .depend.X .depend

include .depend

Makefile: .depend
