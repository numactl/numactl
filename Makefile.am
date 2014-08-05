
ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -Wall

bin_PROGRAMS = numactl numastat numademo migratepages migspeed memhog

lib_LTLIBRARIES = libnuma.la

include_HEADERS = numa.h numacompat1.h numaif.h

noinst_HEADERS = numaint.h util.h

dist_man_MANS = move_pages.2 numa.3 numactl.8 numastat.8 migratepages.8 migspeed.8

EXTRA_DIST = README INSTALL CHANGES TODO DESIGN

numactl_SOURCES = numactl.c util.c shm.c shm.h
numactl_LDADD = libnuma.la

numastat_SOURCES = numastat.c
numastat_CFLAGS = $(AM_CFLAGS) -std=gnu99

numademo_SOURCES = numademo.c stream_lib.c stream_lib.h mt.c mt.h clearcache.c clearcache.h
numademo_CPPFLAGS = $(AM_CPPFLAGS) -DHAVE_STREAM_LIB -DHAVE_MT -DHAVE_CLEAR_CACHE
numademo_CFLAGS = -O3 -ffast-math -funroll-loops
if HAVE_TREE_VECTORIZE
numademo_CFLAGS += -ftree-vectorize
endif
numademo_LDADD = libnuma.la -lm

migratepages_SOURCES = migratepages.c util.c
migratepages_LDADD = libnuma.la

migspeed_SOURCES = migspeed.c util.c
migspeed_LDADD = libnuma.la -lrt

memhog_SOURCES = memhog.c util.c
memhog_LDADD = libnuma.la

libnuma_la_SOURCES = libnuma.c syscall.c distance.c affinity.c affinity.h sysfs.c sysfs.h rtnetlink.c rtnetlink.h versions.ldscript
libnuma_la_LDFLAGS = -version-info 1:0:0 -Wl,--version-script,$(srcdir)/versions.ldscript -Wl,-init,numa_init -Wl,-fini,numa_fini

