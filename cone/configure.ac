dnl Process this file with autoconf to produce a configure script.
#
# Copyright 2003-2011, Double Precision Inc.
#
# See COPYING for distribution information.
#
AC_PREREQ(2.59)
AC_INIT(cone,0.91.1.20150228,[courier-cone@lists.sourceforge.net])
>confdefs.h  # Kill PACKAGE_ macros
AC_CONFIG_SRCDIR(cone.spec.in)
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE

ALL_LINGUAS=
LPATH="$PATH:/usr/local/bin"

test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

eval "exec_prefix=$exec_prefix"
eval "bindir=$bindir"
eval "sysconfdir=$sysconfdir"
eval "datarootdir=$datarootdir"
eval "datadir=$datadir"

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_SYSCONFTOOL
AC_USE_SYSTEM_EXTENSIONS
AM_PROG_LIBTOOL
AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)
dnl Checks for libraries.

save_LIBS="$LIBS"
LIBS="-lcourier-unicode"
AC_LINK_IFELSE(
[AC_LANG_PROGRAM([
#include <courier-unicode.h>
],[
	unicode_default_chset();
])],[:],[AC_MSG_ERROR([The Courier Unicode Library 1.2 appears not to be installed. You may need to install a separate development subpackage, in addition to the main package])])
LIBS="$save_LIBS"

dnl Replace `main' with a function in -lefence:

AM_CONDITIONAL(HAVE_SGML, test -d "$srcdir/docbook")

AC_PATH_PROGS(COURIERCONFIG, courier-config, none, $LPATH)

echo "$VERSION" >coneversion
AC_CONFIG_SUBDIRS(libs/unicode libs/numlib libs/liblock libs/soxwrap libs/md5 libs/sha1 libs/libhmac libs/random128 libs/rfc1035 libs/rfc822 libs/rfc2045 libs/gpglib libs/maildir libs/tcpd curses libs/ldapaddressbook libmail libs/rootcerts cone)
AC_OUTPUT(Makefile cone.spec dbobj.config)
