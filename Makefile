# these can (and should) be overridden on the make command line for production
# use
CFLAGS :=  -g -Wall -O0
# these are used for the benchmarks in addition to the normal CFLAGS. 
# Normally no need to overwrite unless you find a new magic flag to make
# STREAM run faster.
BENCH_CFLAGS := -O2 -ffast-math -funroll-loops
# for compatibility with old releases
CFLAGS += ${OPT_CFLAGS}
override CFLAGS += -I.

# find out if compiler supports __thread
THREAD_SUPPORT := $(shell if $(CC) $(CFLAGS) threadtest.c -o threadtest \
			>/dev/null 2>/dev/null ; then echo "yes" ; else echo "no"; fi)
ifeq ($(THREAD_SUPPORT),no)
	override CFLAGS += -D__thread=""
endif

CLEANFILES := numactl.o libnuma.o numactl numademo numademo.o distance.o \
	      memhog libnuma.so libnuma.so.1 numamon numamon.o syscall.o bitops.o \
	      memhog.o util.o stream_main.o stream_lib.o shm.o stream \
	      test/pagesize test/tshared test/mynode.o test/tshared.o mt.o \
	      test/mynode test/ftok test/prefered test/randmap \
	      .depend .depend.X test/nodemap test/distance \
		test/after test/before threadtest migratepages

SOURCES := bitops.c libnuma.c distance.c memhog.c numactl.c numademo.c \
	numamon.c shm.c stream_lib.c stream_main.c syscall.c util.c mt.c \
	test/*.c

prefix := /usr
libdir := ${prefix}/$(shell ./getlibdir)
docdir := ${prefix}/share/doc

all: numactl migratepages libnuma.so numademo numamon memhog test/tshared stream \
     test/mynode test/pagesize test/ftok test/prefered test/randmap \
	 test/nodemap test/distance

numactl: numactl.o util.o shm.o bitops.o libnuma.so

migratepages: migratepages.c util.o bitops.o libnuma.so

util.o: util.c

memhog: util.o memhog.o libnuma.so

numactl.o: numactl.c

numademo: LDFLAGS += -lm
# GNU make 3.80 appends BENCH_CFLAGS twice. Bug? It's harmless though.
numademo: CFLAGS += -DHAVE_STREAM_LIB -DHAVE_MT ${BENCH_CFLAGS} 
stream_lib.o: CFLAGS += ${BENCH_CFLAGS}
mt.o: CFLAGS += ${BENCH_CFLAGS} 
mt.o: mt.c
numademo: numademo.o stream_lib.o mt.o libnuma.so

numademo.o: numademo.c libnuma.so	

numamon: numamon.o

stream: stream_lib.o stream_main.o  libnuma.so util.o
	${CC} -o stream ${CFLAGS} stream_lib.o stream_main.o util.o -L. -lnuma -lm

stream_main.o: stream_main.c

libnuma.so.1: libnuma.o syscall.o distance.o
	${CC} -shared -Wl,-soname=libnuma.so.1 -o libnuma.so.1 $^

libnuma.so: libnuma.so.1
	ln -sf libnuma.so.1 libnuma.so

libnuma.o : CFLAGS += -fPIC

distance.o : CFLAGS += -fPIC

syscall.o : CFLAGS += -fPIC

test/tshared: test/tshared.o libnuma.so

test/mynode: test/mynode.o libnuma.so

test/pagesize: test/pagesize.c libnuma.so

test/prefered: test/prefered.c libnuma.so

test/ftok: test/ftok.c libnuma.so

test/randmap: test/randmap.c libnuma.so

test/nodemap: test/nodemap.c libnuma.so

test/distance: test/distance.c libnuma.so

.PHONY: install all clean html depend

MANLINKS := \
all_nodes alloc alloc_interleaved alloc_interleaved_subset alloc_local \
alloc_onnode available bind error exit_on_error free get_interleave_mask \
get_interleave_node get_membind get_run_node_mask interleave_memory max_node \
no_nodes node_size node_to_cpus police_memory preferred run_on_node \
run_on_node_mask set_bind_policy  set_interleave_mask set_localalloc \
set_membind set_preferred set_strict setlocal_memory tonode_memory \
tonodemask_memory distance

MANPAGES := numa.3 numactl.8 numastat.8 migratepages.8

install: numactl migratepages numademo.c numamon memhog libnuma.so.1 numa.h numaif.h numastat ${MANPAGES}
	cp numactl ${prefix}/bin
	cp migratepages ${prefix}/bin
	cp numademo ${prefix}/bin
	cp memhog ${prefix}/bin
	cp numactl.8 ${prefix}/share/man/man8
	cp migratepages.8 ${prefix}/share/man/man8
	cp numa.3 ${prefix}/share/man/man3
	( cd ${prefix}/share/man/man3 ; for i in ${MANLINKS} ; do ln -sf numa.3 numa_$$i.3 ; done )
	cp numa_maps.5 ${prefix}/share/man/man5
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

distclean: clean
	rm -f .[^.]* */.[^.]*
	rm -f *~ */*~

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
	${CC} -MM -DDEPS_RUN -I. ${SOURCES} > .depend.X && mv .depend.X .depend

include .depend

Makefile: .depend
