dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 2004 - 2021 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_PREREQ(2.59)
AC_INIT(courier-analog, 0.18.20210219, [courier-users@lists.sourceforge.net])
AM_INIT_AUTOMAKE
LPATH="$PATH:/usr/local/bin"

# Checks for programs.
AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/libs/docbook)
AC_CONFIG_FILES([Makefile courier-analog courier-analog.spec])
AC_OUTPUT
