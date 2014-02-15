dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 1998 - 1999 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(afx,1.60, [courier-users@lists.sourceforge.net])
AC_CONFIG_SRCDIR(afx.h)
AM_INIT_AUTOMAKE([foreign no-define])
AC_CONFIG_HEADERS(config.h)
dnl Checks for programs.
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_CPP
AC_PROG_RANLIB
AC_PROG_CXX
AC_PROG_INSTALL

if test x$GXX = xyes
then
	CXXFLAGS="-Wall $CXXFLAGS"
fi

dnl Checks for libraries.

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h strings.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_SYS_LARGEFILE

AC_ARG_WITH(testmode, [ ],
	testmode="$withval", testmode="N")

case "$testmode" in
y*|y*|1)
	testmode=1
	CFLAGS=`echo "$CFLAGS" | sed 's/-O2//'`
	CXXFLAGS=`echo "$CXXFLAGS" | sed 's/-O2//'`
	;;
*)
	testmode=0
	;;
esac

CFLAGS="-I.. -I${srcdir}/.. $CFLAGS"
CXXFLAGS="-I.. -I${srcdir}/.. $CXXFLAGS"

dnl Checks for library functions.
AC_CHECK_FUNCS(regcomp)

AC_LANG_CPLUSPLUS
AC_CACHE_CHECK([if the C++ compiler needs -fhandle-exceptions],
	ac_cv_need_handlexceptions,

AC_TRY_COMPILE([],
[
throw;
], ac_cv_need_handlexceptions=no,
ac_cv_need_handlexceptions=yes)
)

if test "$ac_cv_need_handlexceptions" = "yes"
then
	case "$CXXFLAGS" in
	*handle-exceptions*)
		;;
	*)
		CXXFLAGS="-fhandle-exceptions $CXXFLAGS"
		CXXFLAGS=`echo "$CXXFLAGS" | sed 's/-O2//'`
		;;
	esac
fi

AC_OUTPUT(Makefile)
