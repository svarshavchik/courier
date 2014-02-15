dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 1998 - 1999 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(module.dsn, 0.11, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(Makefile.am)
AM_INIT_AUTOMAKE([foreign no-define])
AC_CONFIG_HEADERS(config.h)


dnl Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_CC

dnl Checks for libraries.

version=`cat ../../version`
AC_SUBST(version)
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL

dnl Checks for header files.

AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.

AC_SIZE_T
AC_TYPE_SIGNAL
AC_SYS_LARGEFILE

dnl Checks for library functions.

AC_CHECK_FUNCS(strcasecmp strncasecmp)

dnl Other options

if test x$GCC = xyes
then
	CFLAGS="-Wall $CFLAGS"
fi

AC_ARG_WITH(courierdir,
[--with-courierdir         Location of courier source code (main directory)],
	courierdir="$withval", courierdir="../..")

CFLAGS="-I$srcdir/$courierdir/courier -I$courierdir/courier -I$courierdir -I$srcdir/$courierdir -I$srcdir/$courierdir/courier/libs -I$courierdir/libs -I$srcdir/$courierdir/libs $CFLAGS"
CXXFLAGS="-I$srcdir/$courierdir/courier -I$courierdir/courier -I$courierdir -I$srcdir/$courierdir -I$srcdir/$courierdir/courier/libs -I$courierdir/libs -I$srcdir/$courierdir/libs $CXXFLAGS"

AC_SUBST(courierdir)

AC_ARG_WITH(mailuser, [--with-mailuser=user      Specify mail user name (defaults to mail)],
	mailuser="$withval",
	mailuser=mail
	)
AC_ARG_WITH(mailgroup, [--with-mailgroup=group    Specify mail group name (defaults to mail)],
	mailgroup="$withval",
	mailgroup=mail
	)
AC_SUBST(mailuser)
AC_SUBST(mailgroup)

AC_OUTPUT(Makefile dsnheader.txt)
