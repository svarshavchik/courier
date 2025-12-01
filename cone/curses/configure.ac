dnl Process this file with autoconf to produce a configure script.

#
# Copyright 2002-2004, Double Precision Inc.
#
# See COPYING for distribution information.
#
AC_INIT([curses],[0.11],[courier-cone@lists.sourceforge.net])
AC_CONFIG_SRCDIR(mycurses.H)
AC_CONFIG_AUX_DIR(..)
AC_CONFIG_HEADERS(curses_config.h)
AM_INIT_AUTOMAKE(foreign)
>confdefs.h

dnl Checks for programs.
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_17
AC_PROG_AWK
AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Checks for libraries.

CURSESLIBRARY=""

for f in ncursesw cursesw ncurses curses
do
	AC_CHECK_LIB($f, wgetch,
				 [if test "$CURSESLIBRARY" = ""
				     then
					CURSESLIBRARY="$f"
				     fi
				     ])
done

if test "$CURSESLIBRARY" = ""
then
	AC_MSG_ERROR([curses library not found.])
fi

CURSESLIB="-l$CURSESLIBRARY"
AC_SUBST(CURSESLIB)

dnl Checks for header files.
AC_CHECK_HEADERS(sys/time.h unistd.h sys/wait.h glob.h)
AC_CHECK_HEADERS(ncursesw/curses.h)

AC_CHECK_HEADERS_ONCE([sys/time.h])

AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT

save_LIBS="$LIBS"
LIBS="$CURSESLIB $LIBS"

AC_MSG_CHECKING(for use_default_colors())

AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <curses.h>
]], [[
	use_default_colors();
]])],[AC_MSG_RESULT(yes)

AC_DEFINE_UNQUOTED(HAS_USE_DEFAULT_COLORS,1,[Whether we have use_default_colors()])
],[AC_MSG_RESULT(no)])

AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#if HAVE_NCURSESW_CURSES_H
#include <ncursesw/curses.h>
#else
#include <curses.h>
#endif

#ifdef KEY_F
#if KEY_F(0)+63 == KEY_F(63)
#define SANEFKEY
#endif
#endif

#ifndef SANEFKEY
#error Not a sane FKEY
#endif
]])],
AC_DEFINE_UNQUOTED(HAS_FUNCTIONKEYS,1,[Whether curses implements function keys with known semantics]))

LIBS="$save_LIBS"

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

dnl Checks for library functions.

AC_SYS_LARGEFILE

AC_CHECK_FUNCS(glob)
if test "$GXX" = "yes"
then
	CPPFLAGS="-Wall $CPPFLAGS"
fi
CFLAGS="-I../libs -I${srcdir}/../libs $CFLAGS"
CXXFLAGS="-I../libs -I${srcdir}/../libs $CXXFLAGS"

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
