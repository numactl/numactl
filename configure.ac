AC_PREREQ([2.64])
AC_INIT([numactl], [2.0.14])

AC_CONFIG_SRCDIR([numactl.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign 1.11 silent-rules subdir-objects parallel-tests])
AM_SILENT_RULES([yes])

LT_PREREQ([2.2])
LT_INIT

AC_PROG_CC

# Override CFLAGS so that we can specify custom CFLAGS for numademo.
AX_AM_OVERRIDE_VAR([CFLAGS])

AX_TLS

AX_CHECK_COMPILE_FLAG([-ftree-vectorize], [tree_vectorize="true"])
AM_CONDITIONAL([HAVE_TREE_VECTORIZE], [test x"${tree_vectorize}" = x"true"])

AC_CONFIG_FILES([Makefile])

# GCC tries to be "helpful" and only issue a warning for unrecognized
# attributes.  So we compile the test with Werror, so that if the
# attribute is not recognized the compilation fails
AC_LANG(C)
AC_LANG_WERROR
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[__attribute__ ((symver ("foo@foo_1"))) void frob (void) { }]])],
                  [AC_DEFINE([HAVE_ATTRIBUTE_SYMVER], [1], [Checking for symver attribute])], [])

AC_OUTPUT
