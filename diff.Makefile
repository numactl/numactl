diff -ruN numactl-2.0.8-rc5.orig/Makefile numactl-2.0.8-rc5.new/Makefile
--- numactl-2.0.8-rc5.orig/Makefile	2012-08-23 15:50:37.000000000 -0400
+++ numactl-2.0.8-rc5.new/Makefile	2012-09-06 13:58:40.056795146 -0400
@@ -32,7 +32,7 @@
  	      test/mbind_mig_pages test/migrate_pages \
  	      migratepages migspeed migspeed.o libnuma.a \
  	      test/move_pages test/realloc_test sysfs.o affinity.o \
-	      test/node-parse rtnetlink.o test/A
+	      test/node-parse rtnetlink.o test/A numastat
  SOURCES := bitops.c libnuma.c distance.c memhog.c numactl.c numademo.c \
  	numamon.c shm.c stream_lib.c stream_main.c syscall.c util.c mt.c \
  	clearcache.c test/*.c affinity.c sysfs.c rtnetlink.c
@@ -45,10 +45,12 @@
       test/tshared stream test/mynode test/pagesize test/ftok test/prefered \
       test/randmap test/nodemap test/distance test/tbitmap test/move_pages \
       test/mbind_mig_pages test/migrate_pages test/realloc_test libnuma.a \
-     test/node-parse
+     test/node-parse numastat

  numactl: numactl.o util.o shm.o bitops.o libnuma.so

+numastat: CFLAGS += -std=gnu99
+
  migratepages: migratepages.c util.o bitops.o libnuma.so

  migspeed: LDLIBS += -lrt
