dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 2004 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(courierlogger, 0.10, [courier-users@lists.sourceforge.net])

>confdefs.h # Kill PACKAGE_ macros.
LPATH="$PATH:/usr/local/bin"
AC_CONFIG_SRCDIR(logger.c)
AC_CONFIG_AUX_DIR(..)
AM_INIT_AUTOMAKE([foreign no-define])
AC_CONFIG_HEADERS(config.h)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL

if test x$GCC = xyes
then
	CFLAGS="-Wall $CFLAGS"
fi
if test x$GXX = xyes
then
	CXXFLAGS="-Wall $CXXFLAGS"
fi

CFLAGS="-I../libs -I$srcdir/../libs $CFLAGS"
CXXFLAGS="-I../libs -I$srcdir/../libs $CXXFLAGS"

AC_C_CONST
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h string.h syslog.h)
AC_TYPE_UID_T
AC_SYS_LARGEFILE
AC_CHECK_FUNCS(strchr)

AC_ARG_WITH(syslog,
	AC_HELP_STRING([--with-syslog=XXX],[syslog facility [[MAIL]]]),
	logdest="LOG_$withval", logdest="LOG_MAIL")

AC_DEFINE_UNQUOTED(LOG_DEST, [$logdest],
	[ syslog facility ])
AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/../libs/docbook)
AC_OUTPUT(Makefile)
