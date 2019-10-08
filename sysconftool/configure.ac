dnl Process this file with autoconf to produce a configure script.
dnl Copyright 2000-2013 Double Precision, Inc.  See COPYING for
dnl distribution information.
AC_INIT(sysconftool, 0.17.20191110, [courier-users@lists.sourceforge.net])

AC_CONFIG_SRCDIR(sysconftool.in)
AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PATH_PROGS(PERL, perl5 perl, perl)

if test "$PERL" = "perl"
then
	AC_ERROR(Perl program not found.)
fi

ACDIR=`aclocal --print-ac-dir`

if test "$ACDIR" = ""
then
	AC_ERROR(aclocal not installed - please install aclocal)
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

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/docbook)

AC_OUTPUT(Makefile sysconftool sysconftoolcheck sysconftool.spec sysconftool.m4 sysconftoolize.pl)
