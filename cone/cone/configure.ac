dnl Process this file with autoconf to produce a configure script.
#
# Copyright 2003-2025, Double Precision Inc.
#
# See COPYING for distribution information.
#
AC_INIT([cone],[1.15],[courier-cone@lists.sourceforge.net])

AC_CONFIG_SRCDIR(cone.C)
AC_CONFIG_AUX_DIR(..)
AC_CANONICAL_TARGET
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE(foreign)

ALL_LINGUAS=
LPATH="$PATH:/usr/local/bin"

test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

eval "exec_prefix=$exec_prefix"
eval "bindir=$bindir"
eval "sysconfdir=$sysconfdir"
eval "datarootdir=$datarootdir"
eval "datadir=$datadir"

htmldatadir="$datadir/cone"
AC_SUBST(htmldatadir)

dnl Checks for programs.

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
LT_INIT
AM_GNU_GETTEXT([external])
PKG_PROG_PKG_CONFIG

AX_COURIER_UNICODE_CXXFLAGS
AC_SUBST(COURIER_UNICODE_CXXFLAGS)

AC_PATH_PROG(IMAGEVIEWER1, eog, /usr/bin/eog, $LPATH)
AC_SUBST(IMAGEVIEWER1)

AC_PATH_PROG(IMAGEVIEWER2, kview, /usr/bin/kview, $LPATH)
AC_SUBST(IMAGEVIEWER2)

AC_PATH_PROG(XPDF, xpdf, /usr/bin/xpdf, $LPATH)
AC_SUBST(XPDF)

AC_PATH_PROGS(XMLCONFIG, xml2-config xml-config, xml-config, $LPATH)

if test "$XMLCONFIG" = "xml-config"
then
	AC_MSG_ERROR([xml-config not found, download libxml from http://xmlsoft.org])
fi

case "`$XMLCONFIG --version`" in
1.*)
	AC_MSG_ERROR([libxml version 2 required])
	;;
esac

CPPFLAGS="`$XMLCONFIG --cflags` -I.. -I${srcdir}/.. $CPPFLAGS -I../libmail -I${srcdir}/../libmail -I../libs -I$srcdir/../libs"
XMLLIBS="`$XMLCONFIG --libs`"
AC_SUBST(XMLLIBS)

. ../libs/tcpd/couriertls.config

if test "$couriertls" != ""
then
	LIBCOURIERTLS="../libs/tcpd/libcouriertls.la"
fi
AC_SUBST(LIBCOURIERTLS)

. ../libs/rootcerts/rootcertsdir.cnf
AC_SUBST(cacerts)

AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)
dnl Checks for libraries.
dnl Replace `main' with a function in -lefence:

dnl Checks for header files.

AC_HEADER_DIRENT
AC_CHECK_HEADERS(fcntl.h strings.h sys/time.h unistd.h langinfo.h utime.h sys/select.h sys/wait.h)
AC_HEADER_SYS_WAIT

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_HEADERS_ONCE([sys/time.h])

AC_STRUCT_TM

dnl Checks for library functions.
AC_SYS_LARGEFILE

dnl Check whether we should use gdbm, or db.

AC_CHECK_FUNCS(nl_langinfo utime utimes)

if test "$GXX" = "yes"
then
	CPPFLAGS="-Wall $CPPFLAGS"
fi

AM_CONDITIONAL(HAVE_SGML, test -d "$srcdir/../libs/docbook")

#
# Spell checker
#

AC_LANG([C++])
AC_CHECK_HEADERS(aspell/config.hh)

PKG_CHECK_MODULES(hunspell,hunspell,[HUNSPELL="yes"],[HUNSPELL="no"])

AC_MSG_CHECKING(for aspell)

save_LIBS="$LIBS"
LIBS="-laspell $LIBS"

AC_LINK_IFELSE([AC_LANG_PROGRAM([[

#if HAVE_ASPELL_CONFIG_HH

/* Old aspell API */

#include "aspell/config.hh"
#include "aspell/manager.hh"
#include "aspell/suggest.hh"

#else

/* New aspell API */

#include <aspell.h>

AspellConfig *config;
AspellCanHaveError *manager;
AspellSpeller *speller;


#endif

]], [[
	{
#if HAVE_ASPELL_CONFIG_HH
		aspell::Config new_config;
#else
		config=new_aspell_config();
		manager=new_aspell_speller(config);
		speller=to_aspell_speller(manager);
#endif
	}
]])],[ASPELL=yes],[ASPELL=no])
AC_LANG([C])

LIBS="$save_LIBS"
AC_MSG_RESULT($ASPELL)

AC_MSG_CHECKING(for pspell)

save_LIBS="$LIBS"
LIBS="-lpspell $LIBS"

AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include "pspell/pspell.h"
PspellConfig *foo;
]], [[
foo=new_pspell_config();
]])],[PSPELL=yes],[PSPELL=no])

LIBS="$save_LIBS"
AC_MSG_RESULT($PSPELL)

AC_ARG_WITH(spellcheck,
[  --with-spellcheck=hunspell    Use hunspell for spell checking
  --with-spellcheck=pspell      Use pspell for spell checking
  --with-spellcheck=aspell      Use aspell for spell checking], [

case "$withval" in
hunspell)
	if test "$HUNSPELL" = "no"
	then
		AC_MSG_ERROR(hunspell library not found)
	fi
	ASPELL="no"
	PSPELL="no"
	;;
pspell)
	if test "$PSPELL" = "no"
	then
		AC_MSG_ERROR(pspell library not found)
	fi
	ASPELL="no"
	HUNSPELL="no"
	;;
aspell)
	if test "$ASPELL" = "no"
	then
		AC_MSG_ERROR(aspell library not found)
	fi
	PSPELL="no"
	HUNSPELL="no"
	;;
*)
	AC_MSG_ERROR(Invalid --with-spellcheck option)
	;;
esac
])

AC_MSG_CHECKING(for iswalpha)

AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <wctype.h>
int w;
]], [[
	w=iswalpha('A');
]])],[
AC_DEFINE_UNQUOTED(HAVE_ISWALPHA, 1, [ whether iswalpha() is available ])
AC_MSG_RESULT(yes)
],[AC_MSG_RESULT(no)])

if test "$HUNSPELL" = "yes"
then
	AC_DEFINE_UNQUOTED(USE_HUNSPELL, 1, [Use hunspell library for spell checking ])
	ASPELL="no"
	PSPELL="no"

	HUNSPELLPROG="`which hunspell`"
	if test "$HUNSPELLPROG" = ""
	then
		AC_MSG_ERROR([hunspell program not found])
	fi
	AC_DEFINE_UNQUOTED(HUNSPELLPROG,["]$HUNSPELLPROG["], [Path to hunspell executable])
	SPELL_LIB=' '
fi

if test "$ASPELL" = "yes"
then
	AC_DEFINE_UNQUOTED(USE_ASPELL, 1, [ Use aspell library for spell checking ])
	SPELL_LIB='-laspell'
	PSPELL="no"
fi

if test "$PSPELL" = "yes"
then
	AC_DEFINE_UNQUOTED(USE_PSPELL, 1, [ Use pspell library for spell checking ])
	SPELL_LIB='-lpspell'
fi

if test "$SPELL_LIB" = ""
then
	AC_DEFINE_UNQUOTED(NO_SPELL, 1, [Use neither aspell, pspell or hunspell for spell checking])
	AC_MSG_WARN(-------------------------------------------------------------------------)
	AC_MSG_WARN([Neither pspell, aspell, or hunspell was found -- disabling spell checking])
	AC_MSG_WARN(-------------------------------------------------------------------------)
	sleep 5
fi

AC_ARG_WITH(devel, [  --with-devel                  Install development libraries],
		   devel="$withval", devel=no)

case "$devel" in
y*|Y*)
	MAN3='$(man3src)'
	LIBMAIL='libmail.la'
	;;
esac
AC_SUBST(MAN3)
AC_SUBST(MAN5)
AC_SUBST(LIBMAIL)

AC_SUBST(SPELL_LIB)

AC_SUBST(target_cpu)
AC_SUBST(target_vendor)
AC_SUBST(target_os)

filterdir="${datadir}/cone"

AC_ARG_WITH(filterdir, [--with-filterdir=dir   Internal debugging option ],
		       [ filterdir="$withval" ])

AC_DEFINE_UNQUOTED(FILTERDIR, "$filterdir",
			[ Global filter directory ])

AC_MSG_CHECKING(for LDAP support)

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[

#include "../libs/ldapaddressbook/config.h"

#if HAVE_OPENLDAP

#else

#error no openLDAP
#endif

]], [[
	;
]])],[
	LDAPSEARCH="../libs/ldapaddressbook/libldapsearch.la"
        AC_MSG_RESULT(yes)
],[
        AC_MSG_RESULT(no)

])

AC_SUBST(LDAPSEARCH)

# This gets built by the make rule, and is not in the git repo

for f in cone.dist.in
do
    if test ! -f "$srcdir/$f"
    then
	cp -p "$srcdir/$f.git" "$srcdir/$f"
	touch "$srcdir/$f.git"
    fi
done

CFLAGS="-I../libs -I${srcdir}/../libs $CFLAGS"
CXXFLAGS="-I../libs -I${srcdir}/../libs $CXXFLAGS"
AC_CONFIG_FILES([Makefile cone.dist cone.sh IMAGE.filter APPLICATION.PDF.filter http.handler po/Makefile.in])
AC_OUTPUT
