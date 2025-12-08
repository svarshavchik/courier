dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 2000 - 2022 S. Varshavchik.  See COPYING for
dnl distribution information.

AC_INIT([sysconftool],[0.21],[courier-users@lists.sourceforge.net])

AC_CONFIG_SRCDIR(sysconftool.in)
AM_INIT_AUTOMAKE(foreign)

dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PATH_PROGS(PERL, perl5 perl, perl)

if test "$PERL" = "perl"
then
	AC_MSG_ERROR(Perl program not found.)
fi

ACDIR=`aclocal --print-ac-dir`

if test "$ACDIR" = ""
then
	AC_MSG_ERROR(aclocal not installed - please install aclocal)
fi

if test "$prefix" != "NONE"
then
	ACDIR='${datadir}/'`basename $ACDIR`   # For make distcheck
fi

AC_SUBST(ACDIR)

dnl Checks for libraries.

dnl Checks for header files.

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/libs/docbook)

env LC_ALL=C perl -e 'use POSIX qw(strftime); print strftime("DATE=\"%a %b %d %Y\"; DATEFULL=\"%a, %d %b %Y %H:%M:%S %z\"", localtime)' >configure.date
. ./configure.date
rm configure.date
AC_SUBST(DATE)
AC_SUBST(DATEFULL)


AC_CONFIG_FILES([Makefile
		sysconftool
		sysconftoolcheck
		sysconftool.spec
		sysconftool.m4
		sysconftoolize.pl
		courier-debuild
])
AC_OUTPUT
