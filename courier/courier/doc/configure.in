dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 1998 - 2001 Double Precision, Inc.  See COPYING for
dnl distribution information.
AC_INIT(courier-doc, 0.10, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(index.html)
AM_INIT_AUTOMAKE([foreign no-define])
LPATH="$PATH:/usr/local/bin"


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

version=`cat ../../version`
AC_SUBST(version)

AC_ARG_WITH(extension,
[--with-extension=prefix     Read $HOME/.prefix and .prefix-foo
                            for delivery instructions ],
	extension="$withval", extension="courier")
AC_SUBST(extension)

dnl Checks for programs.

dnl I need perl only to build man pages

AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)
AC_PROG_LN_S

dnl Checks for libraries.

dnl Checks for header files.

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.

dnl Evaluate actual directory location.

test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

eval "prefix=\"$prefix\""
eval "exec_prefix=\"$exec_prefix\""
eval "bindir=\"$bindir\""
eval "sbindir=\"$sbindir\""
eval "sysconfdir=\"$sysconfdir\""
eval "datarootdir=\"$datarootdir\""
eval "datadir=\"$datadir\""
eval "localstatedir=\"$localstatedir\""
eval "libexecdir=\"$libexecdir\""

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/../../libs/docbook)

AC_OUTPUT( Makefile )
