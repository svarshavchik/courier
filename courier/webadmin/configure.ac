dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 2001 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(webadmin, 0.1, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(webadmin.c)
AC_CONFIG_AUX_DIR(..)
AM_INIT_AUTOMAKE([foreign no-define])

LPATH="$PATH:/usr/local/bin"
AC_CONFIG_HEADERS(config.h)
dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_AWK
AC_PROG_LN_S
AC_PATH_PROGS(PERL, perl5 perl, /usr/bin/perl, $LPATH)
AC_PATH_PROG(PROCMAIL, procmail, /usr/bin/procmail, $LPATH)

AC_PATH_PROGS(COURIERAUTHCONFIG, courierauthconfig)

if test "$COURIERAUTHCONFIG" = ""
then
	AC_MSG_ERROR(courierauthconfig not found)
fi

$COURIERAUTHCONFIG --configfiles | sed 's/^/auth/' >configtmp
. ./configtmp
rm -f configtmp

authdaemond="$authsbindir/authdaemond"
authdaemonrc="$authauthdaemonrc"
authmysqlrc="$authauthmysqlrc"
authpgsqlrc="$authauthpgsqlrc"
authldaprc="$authauthldaprc"

AC_SUBST(authdaemond)
AC_SUBST(authdaemonrc)
AC_SUBST(authmysqlrc)
AC_SUBST(authpgsqlrc)
AC_SUBST(authldaprc)

dnl Checks for libraries.

dnl Checks for header files.
AC_CHECK_HEADERS(unistd.h)

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

AC_DEFINE_UNQUOTED(INSTDIR,"$webadmindir",
	[ Where wedadmin.pl is installed ])
AC_SUBST(PERL)
AC_SUBST(webadmindir)

AC_ARG_WITH(mailuser, [], mailuser="$withval",
	AC_MSG_ERROR(--with-mailuser missing))
AC_ARG_WITH(mailgroup, [], mailgroup="$withval",
	AC_MSG_ERROR(--with-mailgroup missing))
AC_SYS_LARGEFILE

AC_SUBST(mailuser)
AC_SUBST(mailgroup)
AC_DEFINE_UNQUOTED(MAILUSER,"$mailuser", [ Courier mail userid ])
AC_DEFINE_UNQUOTED(MAILGROUP,"$mailgroup", [ Courier mail groupid ])

AC_ARG_WITH(version, [], VERSION="$withval")
AC_ARG_WITH(package, [], PACKAGE="$withval")
AC_SUBST(VERSION)
AC_SUBST(PACKAGE)

INSTALL_SCRIPT='${INSTALL}'

AC_OUTPUT(Makefile webadmin.pl dumpenv.pl)
