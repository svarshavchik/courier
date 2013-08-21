dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 2001 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(webadmin, 0.1, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(admin-00machine.pl)
AM_INIT_AUTOMAKE([foreign no-define])
LPATH="$PATH:/usr/local/bin"
dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_AWK
AC_PROG_LN_S
AC_PATH_PROGS(PERL, perl5 perl, /usr/bin/perl, $LPATH)
AC_PATH_PROG(PROCMAIL, procmail, /usr/bin/procmail, $LPATH)
AC_SUBST(PROCMAIL)

dnl Checks for libraries.

dnl Checks for header files.

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.

test "x$prefix" = XNONE && prefix="/usr/lib/courier"

test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

eval "exec_prefix=$exec_prefix"
eval "bindir=$bindir"
eval "sysconfdir=$sysconfdir"
eval "datadir=$datadir"

AC_ARG_WITH(webadmindir, [], webadmindir="$withval",
	webadmindir="$datadir/webadmin")

AC_SUBST(PERL)
AC_SUBST(webadmindir)
AC_SYS_LARGEFILE

AC_ARG_WITH(mailuser, [], mailuser="$withval",
	AC_MSG_ERROR(--with-mailuser missing))
AC_ARG_WITH(mailgroup, [], mailgroup="$withval",
	AC_MSG_ERROR(--with-mailgroup missing))

AC_SUBST(mailuser)
AC_SUBST(mailgroup)

AC_ARG_WITH(version, [], VERSION="$withval")
AC_SUBST(VERSION)

INSTALL_SCRIPT='${INSTALL}'
AC_OUTPUT(Makefile admin-05local.pl)
