#
# Copyright 2002 Double Precision, Inc.  See COPYING for
# distribution information.
#
dnl Process this file with autoconf to produce a configure script.
AC_INIT(module.fax, 0.10, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(faxconvert.c)
AM_INIT_AUTOMAKE([foreign no-define])
LPATH="$PATH:/usr/local/bin"
AC_CONFIG_HEADERS(config.h)


dnl Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL

version=`cat ../../version`
AC_SUBST(version)

AC_PATH_PROG(CUT, cut, cut, $LPATH)
AC_PATH_PROG(SED, sed, sed, $LPATH)

AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)
AC_PATH_PROG(TBL, tbl, /usr/bin/tbl, $LPATH)
AC_PATH_PROG(TROFF, troff, /usr/bin/troff, $LPATH)
AC_PATH_PROG(GROPS, grops, grops, $LPATH)

DPOST_PATH=$LPATH:/usr/lib/lp/postscript

AC_PATH_PROG(DPOST, dpost, none, $DPOST_PATH)

if test "$DPOST" = "none"
then
	if test "$GROPS" = "grops"
	then
		AC_MSG_WARN(----------------------------------------------)
		AC_MSG_WARN(Unable to find troff or groff - assuming groff)
		AC_MSG_WARN(----------------------------------------------)
		sleep 5
		DPOST=/usr/bin/grops
	else
		DPOST="$GROPS"
	fi
fi

AC_PATH_PROG(PDFTOPS, pdftops, none, $LPATH)

AC_PATH_PROG(GS, gs, none, $LPATH)

if test "$PDFTOPS" = "none"
then
	PDFTOPS="/usr/bin/pdftops"
fi

if test "$GS" = "none"
then
	GS="/usr/bin/gs"
	AC_MSG_WARN(-------------------------------------------------------------)
	AC_MSG_WARN(Unable to find ghostscript - courierfax will not be available)
	AC_MSG_WARN(-------------------------------------------------------------)
	sleep 5
fi

AC_SUBST(TBL)
AC_SUBST(TROFF)
AC_SUBST(DPOST)
AC_SUBST(GS)
AC_SUBST(PDFTOPS)

SENDFAX_PATH="$PATH:/usr/sbin:/usr/local/sbin:/usr/local/bin"

AC_PATH_PROG(SENDFAX, sendfax, none, $SENDFAX_PATH)

if test "$SENDFAX" = "none"
then
	SENDFAX="/usr/bin/sendfax"
	GS="/usr/bin/gs"
	AC_MSG_WARN(---------------------------------------------------------)
	AC_MSG_WARN(Unable to find sendfax - courierfax will not be available)
	AC_MSG_WARN(---------------------------------------------------------)
	sleep 5
fi

AC_SUBST(SENDFAX)

AC_PATH_PROG(PNMSCALE, pnmscale, none, $LPATH)

if test "$PNMSCALE" = "none"
then
	PNMSCALE=/usr/bin/pnmscale
	AC_MSG_WARN(------------------------------------------------------)
	AC_MSG_WARN(Unable to find netpbm utilities)
	AC_MSG_WARN(------------------------------------------------------)
	sleep 5
fi
AC_PATH_PROG(GIFTOPNM, giftopnm, /usr/bin/giftopnm, $LPATH)
AC_PATH_PROG(JPEGTOPNM, jpegtopnm, /usr/bin/jpegtopnm, $LPATH)
AC_PATH_PROG(PPMTOPGM, ppmtopgm, /usr/bin/ppmtopgm, $LPATH)
AC_PATH_PROG(PGMTOPBM, pgmtopbm, /usr/bin/pgmtopbm, $LPATH)
AC_PATH_PROG(PBMTOG3, pbmtog3, /usr/bin/pbmtog3, $LPATH)
AC_PATH_PROG(G3TOPBM, g3topbm, /usr/bin/g3topbm, $LPATH)
AC_PATH_PROG(PNGTOPNM, pngtopnm, /usr/bin/pngtopnm, $LPATH)
AC_PATH_PROG(PNMTOPNG, pnmtopng, /usr/bin/pnmtopng, $LPATH)

AC_SUBST(PNMSCALE)
AC_SUBST(GIFTOPNM)
AC_SUBST(JPEGTOPNM)
AC_SUBST(PPMTOPGM)
AC_SUBST(PGMTOPBM)
AC_SUBST(PBMTOG3)
AC_SUBST(G3TOPBM)
AC_SUBST(PNGTOPNM)
AC_SUBST(PNMTOPNG)

dnl Checks for libraries.

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADER(sys/wait.h)
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_SYS_LARGEFILE

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.

AC_CHECK_FUNCS(setpgrp setpgid)
AC_CHECK_FUNC(setpgrp,
        [
        AC_FUNC_SETPGRP
        ]
)


if test "$GCC" = "yes"
then
	CFLAGS="-Wall $CFLAGS"
fi

AC_ARG_WITH(courierdir,
[--with-courierdir         Location of courier source code (main directory)],
	courierdir="$withval", courierdir="../..")

CPPFLAGS="-I$courierdir/courier -I$srcdir/$courierdir/courier -I$courierdir -I$srcdir/$courierdir -I$courierdir/courier/libs -I$srcdir/$courierdir/courier/libs $CPPFLAGS -I$courierdir/libs -I$srcdir/$courierdir/libs"
AC_SUBST(courierdir)

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

test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

eval "exec_prefix=$exec_prefix"
eval "bindir=$bindir"
eval "datadir=$datadir"
eval "sysconfdir=$sysconfdir"
eval "localstatedir=$localstatedir"

filterbindir="$datadir/faxmail"
faxtmpdir="$localstatedir/faxtmp"

AC_SUBST(filterbindir)
AC_SUBST(faxtmpdir)

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/../../libs/docbook)

AC_OUTPUT(Makefile
	init
	application-postscript.filter
	application-pdf.filter
	image-gif.filter
	image-jpeg.filter
	image-png.filter
	text-plain.filter
	coverpage
	new_fax
)
