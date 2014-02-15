dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 1998 - 2008 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(courier, 0.10, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(Makefile.am)
AM_INIT_AUTOMAKE([foreign no-define])
AC_CONFIG_HEADERS(config.h)

dnl Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_CC
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL

AC_PATH_PROGS(COURIERAUTHCONFIG, courierauthconfig)

if test "$COURIERAUTHCONFIG" = ""
then
	AC_MSG_ERROR(courierauthconfig not found)
fi

dnl Checks for libraries.

dnl Checks for header files.

AC_CHECK_HEADERS(fcntl.h sys/file.h sys/stat.h sys/time.h sys/wait.h sysexits.h unistd.h syslog.h)
AC_HEADER_TIME

dnl Checks for typedefs, structures, and compiler characteristics.

AC_SIZE_T
AC_TYPE_OFF_T
AC_TYPE_SIGNAL
AC_SYS_LARGEFILE

dnl Checks for library functions.

AC_CHECK_FUNCS(ftruncate strerror strcasecmp strncasecmp setpgrp)
AC_CHECK_FUNC(setpgrp,
	[
		AC_FUNC_SETPGRP
	]
)
 

dnl Other options

if test x$GCC = xyes
then
	CFLAGS="-Wall $CFLAGS"
fi

CFLAGS="-I$srcdir/../.. -I../.. -I$srcdir/../../courier -I../../courier -I$srcdir/../../libs -I../../libs -I$srcdir/../../courier/libs $CFLAGS"

CXXFLAGS="-I$srcdir/../.. -I../.. -I$srcdir/../../courier -I../../courier -I$srcdir/../../libs -I../../libs -I$srcdir/../../courier/libs $CXXFLAGS"

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

AC_ARG_ENABLE(local-extensions,
[--disable-local-extensions  Disable user-ext local addresses],
	extensions="$enableval", extensions="Y")

case "$extensions" in
y*|Y*|1)
	extensions=1
	;;
*)
	extensions=0
	;;
esac
AC_DEFINE_UNQUOTED(LOCAL_EXTENSIONS,$extensions,
	[ Whether dot-local files are implemented ])

AC_ARG_WITH(alias,
[--without-alias             Do not search in ~alias for non-existent addresses],
	alias="$withval", alias="Y")

case "$alias" in
y*|Y*|1)
	alias=1
	;;
*)
	alias=0
	;;
esac

AC_DEFINE_UNQUOTED(ALIAS,$alias, [ Alias account name ])

AC_ARG_ENABLE(root,
[--enable-root               Allow mail delivery to root],
[
	case "$enableval" in
	y*|Y*|1)
		AC_DEFINE_UNQUOTED(ALLOW_ROOT,1, [ Allow mail to root ])
		;;
	esac
])

AC_ARG_WITH(testmode, [], testmode="$withval", testmode="N")

case "$testmode" in
y*|y*|1)
	testmode=1
	CFLAGS=`echo "$CFLAGS" | sed 's/-O2//'`
	;;
*)
	testmode=0
	;;
esac
AC_SUBST(testmode)
AC_ARG_WITH(explicitsync, [--with-explicitsync       Explicitly sync cache files
],
	explicitsync="$withval")

case "$explicitsync" in
y*|Y*|1*)
	AC_DEFINE_UNQUOTED(EXPLICITSYNC,1,
	[ Whether message files are explicitly flushed after creation ])
	;;
esac

AC_ARG_WITH(dirsync, [ --with-dirsync           Manually sync queue file directory],
	dirsync="$withval", dirsync="N")

case "$dirsync" in
y*|Y*|1*)
	AC_DEFINE_UNQUOTED(EXPLICITDIRSYNC,1,
	[ Whether to sync the parent directory after delivering to a maildir ])
	;;
esac

AC_DEFINE_UNQUOTED(testmode,$testmode, [ Testing flag ])

echo "$CFLAGS" >cflags
AC_OUTPUT(Makefile)
