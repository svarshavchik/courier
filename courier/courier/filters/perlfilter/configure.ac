dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 2000-2001 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(perlfilter, 0.10, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(perlfilter.c)
AC_CONFIG_AUX_DIR(../../..)
AM_INIT_AUTOMAKE([foreign no-define])
LPATH="$PATH:/usr/local/bin"

AC_CONFIG_HEADERS(config.h)

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL

AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)

AC_CACHE_CHECK([embedded perl interpreter C flags], ac_cv_perl_ccflags,
[
	if test "$PERL" = "perl"
	then
		ac_cv_perl_ccflags=none
	else
		ac_cv_perl_ccflags="`$PERL -MExtUtils::Embed -e ccopts`"
		if test "$ac_cv_perl_ccflags" = ""
		then
			ac_cv_perl_ccflags="none"
		fi
	fi
])

AC_CACHE_CHECK([embedded perl interpreter linker flags], ac_cv_perl_ldflags,
[
	if test "$PERL" = "perl"
	then
		ac_cv_perl_ldflags=none
	else
		ac_cv_perl_ldflags="`$PERL -MExtUtils::Embed -e ldopts`"
		if test "$ac_cv_perl_ldflags" = ""
		then
			ac_cv_perl_ldflags="none"
		fi
	fi
])

if test "$PERL" = "perl"
then
	AC_MSG_WARN(Cannot find perl interpreter.)
else
if test "$ac_cv_perl_ldflags" = "none"
then
	AC_MSG_WARN(Cannot find perl embedded interpreter.)
else

	if test "$ac_cv_perl_ccflags" != "none"
	then
		CFLAGS="$CFLAGS $ac_cv_perl_ccflags"
	fi
	PERL_LDFLAGS="$ac_cv_perl_ldflags"
	perlfilter_prog="perlfilter"
fi
fi

AC_ARG_WITH(perlfilter, [ --without-perlfilter    -- do not compile perlfilter ],
withperlfilter="$withval",
withperlfilter=yes)

if test "$withperlfilter" = "no"
then
	perlfilter_prog=""
fi

AC_SUBST(PERL_LDFLAGS)
AC_SUBST(perlfilter_prog)

AC_ARG_WITH(mailuser, [], mailuser="$withval")
AC_ARG_WITH(mailgroup, [], mailgroup="$withval")
AC_SUBST(mailuser)
AC_SUBST(mailgroup)
AC_SYS_LARGEFILE

dnl Checks for libraries.

dnl Checks for header files.

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.

CFLAGS="-I${srcdir}/.. -I.. -I${srcdir}/../.. -I../.. -I${srcdir}/../../.. -I ../../.. -I${srcdir}/../../../libs -I../../../libs $CFLAGS"

INSTALL_SCRIPT='${INSTALL} -m 500'
AC_OUTPUT(Makefile)
