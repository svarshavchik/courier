dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 2004 - 2022 S. Varshavchik.  See COPYING for
dnl distribution information.

AC_INIT([courier-analog],[0.23],[courier-users@lists.sourceforge.net])
AM_INIT_AUTOMAKE(foreign)
LPATH="$PATH:/usr/local/bin"

# Checks for programs.
AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/libs/docbook)
AC_CONFIG_FILES([Makefile
		courier-analog
		courier-analog.spec
		courier-debuild
		])
AC_OUTPUT
