dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 1998 - 2008 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(module.uucp, 0.11, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(Makefile.am)
AM_INIT_AUTOMAKE([foreign no-define])
LPATH="$PATH:/usr/local/bin"
AC_CONFIG_HEADERS(config.h)


dnl Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
version=`cat ../../version`
AC_SUBST(version)
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL
AC_ARG_WITH(db, [], db="$withval", db="")

. ../../dbobj.config
dblibrary="../../$dblibrary"
AC_SUBST(dblibrary)

AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)

if test "$PERL" = "perl"
then
	AC_MSG_ERROR(Cannot find perl)
fi
AC_PATH_PROG(UUX, uux, /usr/bin/uux, $LPATH)
AC_DEFINE_UNQUOTED(UUX, "$UUX", [ Path to uux ])

dnl Checks for libraries.

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

CFLAGS="-I$srcdir/../.. -I../.. -I$srcdir/../../libs -I../../libs -I$srcdir/../../courier -I../../courier -I$srcdir/../../courier/libs $CFLAGS"
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

test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

eval "prefix=\"$prefix\""
eval "exec_prefix=\"$exec_prefix\""
eval "bindir=\"$bindir\""
eval "sbindir=\"$sbindir\""
eval "sysconfdir=\"$sysconfdir\""

SYSCONFDIR="`echo $sysconfdir | tr '[a-z]' '[A-Z]'`"
AC_SUBST(SYSCONFDIR)

INSTALL_SCRIPT='${INSTALL} -m 755'

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/../../libs/docbook)

AC_OUTPUT(Makefile staticlist.c makeuucpneighbors)
