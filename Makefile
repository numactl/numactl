CFLAGS := -g -Wall -O0

CLEANFILES := libnuma.a numactl.o libnuma.o numactl numademo numademo.o memhog \
		  libnuma.so libnuma.so.1 numamon numamon.o syscall.o memhog.o stream \
		  util.o stream_main.o stream_lib.o

prefix := /usr
libdir := ${prefix}/lib64

all: numactl libnuma.so numademo numamon memhog stream

numactl: numactl.o libnuma.so util.o
	${CC} ${LDFLAGS} -o numactl numactl.o util.o -L. -lnuma

util.o: util.c util.h numa.h

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

.PHONY: install all clean

MANLINKS := available max_node homenode set_interleave_mask \
	get_interleave_mask set_homenode set_localalloc set_membind get_membind \
	alloc_interleaved_subset alloc_interleaved alloc_onnode alloc_local free \
	run_on_node_mask run_on_node interleave_memory tonode_memory \
	setlocal_memory police_memory

MANPAGES := numa.3 numactl.8 mbind.2 set_mempolicy.2	

install: numactl libnuma.so.1 numa.h ${MANPAGES}
	cp numactl ${prefix}/bin
	cp numademo ${prefix}/bin
	cp set_mempolicy.2 ${prefix}/share/man/man2
	cp mbind.2 ${prefix}/share/man/man2
	cp numactl.8 ${prefix}/share/man/man8
	cp numa.3 ${prefix}/share/man/man3
	( cd ${prefix}/share/man/man3 ; for i in ${MANLINKS} ; do ln -s numa.3 numa_$$i.3 ; done )
	cp libnuma.so.1 ${libdir}
	cd ${libdir} ; ln -s libnuma.so.1 libnuma.so
	cp numa.h numaif.h ${prefix}/include
	cp numastat ${prefix}/bin

clean: 
	rm -f ${CLEANFILES} 

