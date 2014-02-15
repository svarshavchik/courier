dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 2000 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(libfilter, 0.10, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(libfilter.c)
AC_CONFIG_AUX_DIR(../../..)
AM_INIT_AUTOMAKE([foreign no-define])
AC_CONFIG_HEADERS(config.h)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

dnl Checks for libraries.

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h sys/time.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_SYS_LARGEFILE

AC_CACHE_CHECK([for socklen_t],
	libfilter_cv_hassocklen_t,
 
AC_COMPILE_IFELSE([
AC_LANG_SOURCE( [
#include <sys/types.h>
#include <sys/socket.h>
 
socklen_t sl_t;
],[
	accept(0, 0, &sl_t);
])],
	libfilter_cv_hassocklen_t=yes,
	libfilter_cv_hassocklen_t=no)
)
 
socklen_t="int"
 
if test $libfilter_cv_hassocklen_t = yes
then
	:
else
	AC_DEFINE_UNQUOTED(socklen_t, int, [ Default definition for socklen_t ])
fi

dnl Checks for library functions.
AC_CHECK_FUNCS(select socket)

CFLAGS="-I../.. -I$srcdir/../.. -I$srcdir/../../.. -I../../.. $CFLAGS"
if test x$GCC = xyes
then
	CFLAGS="-Wall $CFLAGS"
fi

AC_OUTPUT(Makefile)
