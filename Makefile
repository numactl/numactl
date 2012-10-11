# these can (and should) be overridden on the make command line for production
CFLAGS :=  -g -Wall -O2
# these are used for the benchmarks in addition to the normal CFLAGS. 
# Normally no need to overwrite unless you find a new magic flag to make
# STREAM run faster.
BENCH_CFLAGS := -O3 -ffast-math -funroll-loops
# for compatibility with old releases
CFLAGS += ${OPT_CFLAGS}
override CFLAGS += -I.

# find out if compiler supports __thread
THREAD_SUPPORT := $(shell if $(CC) $(CFLAGS) threadtest.c -o threadtest \
			>/dev/null 2>/dev/null ; then echo "yes" ; else echo "no"; fi)
ifeq ($(THREAD_SUPPORT),no)
	override CFLAGS += -D__thread=""
endif

# find out if compiler supports -ftree-vectorize
THREAD_SUPPORT := $(shell touch empty.c ; if $(CC) $(CFLAGS) -c -ftree-vectorize empty.c -o empty.o \
			>/dev/null 2>/dev/null ; then echo "yes" ; else echo "no"; fi)
ifeq ($(THREAD_SUPPORT),yes)
	BENCH_CFLAGS += -ftree-vectorize
endif

CLEANFILES := numactl.o libnuma.o numactl numademo numademo.o distance.o \
	      memhog libnuma.so libnuma.so.1 numamon numamon.o syscall.o bitops.o \
	      memhog.o util.o stream_main.o stream_lib.o shm.o stream clearcache.o \
	      test/pagesize test/tshared test/mynode.o test/tshared.o mt.o empty.o empty.c \
	      test/mynode test/ftok test/prefered test/randmap \
	      .depend .depend.X test/nodemap test/distance test/tbitmap \
	      test/after test/before threadtest test_move_pages \
	      test/mbind_mig_pages test/migrate_pages \
	      migratepages migspeed migspeed.o libnuma.a \
	      test/move_pages test/realloc_test sysfs.o affinity.o \
	      test/node-parse rtnetlink.o test/A numastat
SOURCES := bitops.c libnuma.c distance.c memhog.c numactl.c numademo.c \
	numamon.c shm.c stream_lib.c stream_main.c syscall.c util.c mt.c \
	clearcache.c test/*.c affinity.c sysfs.c rtnetlink.c

ifeq ($(strip $(PREFIX)),)
prefix := /usr
else
prefix := $(PREFIX)
endif
libdir := ${prefix}/$(shell ./getlibdir)
docdir := ${prefix}/share/doc

all: numactl migratepages migspeed libnuma.so numademo numamon memhog \
     test/tshared stream test/mynode test/pagesize test/ftok test/prefered \
     test/randmap test/nodemap test/distance test/tbitmap test/move_pages \
     test/mbind_mig_pages test/migrate_pages test/realloc_test libnuma.a \
     test/node-parse numastat

numactl: numactl.o util.o shm.o bitops.o libnuma.so

numastat: CFLAGS += -std=gnu99

migratepages: migratepages.c util.o bitops.o libnuma.so

migspeed: LDLIBS += -lrt
migspeed: migspeed.o util.o libnuma.so

util.o: util.c

memhog: util.o memhog.o libnuma.so

numactl.o: numactl.c

numademo: LDLIBS += -lm
# GNU make 3.80 appends BENCH_CFLAGS twice. Bug? It's harmless though.
numademo: CFLAGS += -DHAVE_STREAM_LIB -DHAVE_MT -DHAVE_CLEAR_CACHE ${BENCH_CFLAGS}
stream_lib.o: CFLAGS += ${BENCH_CFLAGS}
mt.o: CFLAGS += ${BENCH_CFLAGS} 
mt.o: mt.c
numademo: numademo.o stream_lib.o mt.o libnuma.so clearcache.o

test_numademo: numademo
	LD_LIBRARY_PATH=$$(pwd) ./numademo -t -e 10M

numademo.o: numademo.c libnuma.so	

numamon: numamon.o

stream: LDLIBS += -lm
stream: stream_lib.o stream_main.o  libnuma.so util.o
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^ ${LDLIBS}

stream_main.o: stream_main.c

libnuma.so.1: versions.ldscript

libnuma.so.1: libnuma.o syscall.o distance.o affinity.o sysfs.o rtnetlink.o
	${CC} ${LDFLAGS} -shared -Wl,-soname=libnuma.so.1 -Wl,--version-script,versions.ldscript -Wl,-init,numa_init -Wl,-fini,numa_fini -o libnuma.so.1 $(filter-out versions.ldscript,$^)

libnuma.so: libnuma.so.1
	ln -sf libnuma.so.1 libnuma.so

libnuma.o : CFLAGS += -fPIC

AR ?= ar
RANLIB ?= ranlib
libnuma.a: libnuma.o syscall.o distance.o sysfs.o affinity.o rtnetlink.o
	$(AR) rc $@ $^
	$(RANLIB) $@

distance.o : CFLAGS += -fPIC

syscall.o : CFLAGS += -fPIC

affinity.o : CFLAGS += -fPIC

sysfs.o : CFLAGS += -fPIC

rtnetlink.o : CFLAGS += -fPIC

test/tshared: test/tshared.o libnuma.so

test/mynode: test/mynode.o libnuma.so

test/pagesize: test/pagesize.c libnuma.so

test/prefered: test/prefered.c libnuma.so

test/ftok: test/ftok.c libnuma.so

test/randmap: test/randmap.c libnuma.so

test/nodemap: test/nodemap.c libnuma.so

test/distance: test/distance.c libnuma.so

test/tbitmap: test/tbitmap.c libnuma.so

test/move_pages: test/move_pages.c libnuma.so

test/mbind_mig_pages: test/mbind_mig_pages.c libnuma.so

test/migrate_pages: test/migrate_pages.c libnuma.so

test/realloc_test: test/realloc_test.c libnuma.so

test/node-parse: test/node-parse.c libnuma.so util.o

.PHONY: install all clean html depend

MANPAGES := numa.3 numactl.8 numastat.8 migratepages.8 migspeed.8

install: numactl migratepages migspeed numademo.c numamon memhog libnuma.so.1 numa.h numaif.h numacompat1.h numastat ${MANPAGES}
	mkdir -p ${prefix}/bin
	install -m 0755 numactl ${prefix}/bin
	install -m 0755 migratepages ${prefix}/bin
	install -m 0755 migspeed ${prefix}/bin
	install -m 0755 numademo ${prefix}/bin
	install -m 0755 memhog ${prefix}/bin
	mkdir -p ${prefix}/share/man/man2 ${prefix}/share/man/man8 ${prefix}/share/man/man3
	install -m 0644 migspeed.8 ${prefix}/share/man/man8
	install -m 0644 migratepages.8 ${prefix}/share/man/man8
	install -m 0644 numactl.8 ${prefix}/share/man/man8
	install -m 0644 numastat.8 ${prefix}/share/man/man8
	install -m 0644 numa.3 ${prefix}/share/man/man3
	( cd ${prefix}/share/man/man3 ; for i in $$(./manlinks) ; do ln -sf numa.3 $$i.3 ; done )
	mkdir -p ${libdir}
	install -m 0755 libnuma.so.1 ${libdir}
	cd ${libdir} ; ln -sf libnuma.so.1 libnuma.so
	install -m 0644 libnuma.a ${libdir}
	mkdir -p ${prefix}/include
	install -m 0644 numa.h numaif.h numacompat1.h ${prefix}/include
	install -m 0755 numastat ${prefix}/bin
	if [ -d ${docdir} ] ; then \
		mkdir -p ${docdir}/numactl/examples ; \
		install -m 0644 numademo.c ${docdir}/numactl/examples ; \
	fi	

HTML := html/numactl.html html/numa.html

clean: 
	rm -f ${CLEANFILES}
	@rm -rf html

distclean: clean
	rm -f .[^.]* */.[^.]*
	rm -f *~ */*~ *.orig */*.orig */*.rej *.rej 

html: ${HTML} 

htmldir:
	if [ ! -d html ] ; then mkdir html ; fi

html/numactl.html: numactl.8 htmldir
	groff -Thtml -man numactl.8 > html/numactl.html

html/numa.html: numa.3 htmldir
	groff -Thtml -man numa.3 > html/numa.html

depend: .depend

.depend:
	${CC} -MM -DDEPS_RUN -I. ${SOURCES} > .depend.X && mv .depend.X .depend

include .depend

Makefile: .depend

.PHONY: test regress1 regress2

regress1: 
	cd test ; ./regress

regress2:
	cd test ; ./regress2

regress3:
	cd test ; ./regress-io

test: all regress1 regress2 test_numademo regress3
