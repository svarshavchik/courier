dnl Process this file with autoconf to produce a configure script.
#
# Copyright 2003-2018, Double Precision Inc.
#
# See COPYING for distribution information.
#
AC_PREREQ(2.59)
AC_INIT(cone,0.96.2.20180801,[courier-cone@lists.sourceforge.net])
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

AC_ARG_WITH(notice,[AS_HELP_STRING([--with-notice=unicode],
[The --with-notice=unicode option is required, and indicates that you
are aware of the instructions in INSTALL for upgrading to this
version])], [notice="$withval"], [])

case "$notice" in
unicode)
	;;
*)
	AC_MSG_ERROR([
************************************************************************
Please rerun configure and add the

      --with-notice=unicode

option.

This indicates that you've read the instructions in INSTALL about
updating from earlier versions; specifically the need to convert any
existing maildirs to a Unicode naming scheme, and the need to update
any other Courier packages together with updating this one.

If you are building RPMs directly using the spec file from the source
tarball, add a

      --define 'notice_option --with-notice=unicode'

option to the rpmbuild command:

$ rpmbuild -ta --define 'notice_option --with-notice=unicode'

This message is only meant to make sure you are aware of the contents
of INSTALL about updating from earlier versions, and the need for
manual steps to update any existing maildirs.

If you are installing for the first time, you can simply rerun configure
with this option.
************************************************************************])
	;;
esac

AX_COURIER_UNICODE_VERSION

dnl Checks for libraries.

PKG_CHECK_MODULES(LIBIDN, libidn >= 0.0.0, [libidn=yes], [libidn=no])
if test "$libidn" != "yes"
then
	AC_MSG_ERROR([libidn not found])
fi

dnl Replace `main' with a function in -lefence:

AM_CONDITIONAL(HAVE_SGML, test -d "$srcdir/docbook")

AC_PATH_PROGS(COURIERCONFIG, courier-config, none, $LPATH)

echo "$VERSION" >coneversion
AC_CONFIG_SUBDIRS(libs/unicode libs/numlib libs/liblock libs/soxwrap libs/md5 libs/sha1 libs/libhmac libs/random128 libs/rfc1035 libs/rfc822 libs/rfc2045 libs/gpglib libs/maildir libs/tcpd curses libs/ldapaddressbook libmail libs/rootcerts cone)
AC_OUTPUT(Makefile cone.spec dbobj.config)
