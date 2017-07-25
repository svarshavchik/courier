dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 2000-2001 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(courier, 0.14, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(dupfilter.c)
AM_INIT_AUTOMAKE([foreign no-define])
LPATH="$PATH:/usr/local/bin"
AC_CONFIG_HEADERS(config.h)

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AM_PROG_LIBTOOL
AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)

dnl Checks for libraries.

dnl Checks for header files.

AC_CHECK_HEADERS(unistd.h sys/stat.h sys/time.h fcntl.h)
AC_HEADER_TIME
AC_HEADER_DIRENT

dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_TYPE_SIGNAL
AC_SYS_LARGEFILE

dnl Checks for library functions.

USENSL=no
saveLIBS="$LIBS"
AC_CHECK_LIB(socket,socket,result=yes,result=no)
if test $result = yes; then
        NETLIBS="-lsocket"
else
        AC_CHECK_LIB(socket,socket,result=yes,result=no,-lnsl)
        if test $result = yes; then
                NETLIBS = "-lsocket -lnsl"
                USENSL=yes
        else
                AC_CHECK_LIB(socket,connect,result=yes,result=no)
                if test $result = yes; then
                        NETLIBS="-lsocket"
                else
                        AC_CHECK_LIB(socket,connect,result=yes,result=no,-lnsl)
                        if test $result = yes; then
                                NETLIBS="-lsocket -lnsl"
                                USENSL=yes
                        fi
                fi
        fi
fi

if test $USENSL != yes; then
	LIBS="$LIBS $NETLIBS"
	AC_TRY_LINK_FUNC(inet_addr, [ : ],
	[
	        AC_CHECK_LIB(nsl,inet_addr,result=yes,result=no)
	        if test $result = yes; then
	                NETLIBS="$NETLIBS -lnsl"
	        fi
	])
fi

LIBS="$saveLIBS"
AC_SUBST(NETLIBS)

AC_ARG_WITH(piddir, [  --with-piddir         Directory where PID files are located ], piddir="$withval", piddir=/var/run)

AC_SUBST(piddir)

test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

eval "prefix=$prefix"
eval "exec_prefix=$exec_prefix"
eval "datarootdir=$datarootdir"
eval "sysconfdir=$sysconfdir"
eval "localstatedir=$localstatedir"
eval "libexecdir=$libexecdir"
eval "datadir=$datadir"

AC_CHECK_FUNCS(sigsuspend sigpause)

AC_ARG_WITH(mailuid, [], mailuid="$withval")
AC_ARG_WITH(mailgid, [], mailgid="$withval")
AC_ARG_WITH(mailuser, [], mailuser="$withval")
AC_ARG_WITH(mailgroup, [], mailgroup="$withval")
AC_SUBST(mailuid)
AC_SUBST(mailgid)
AC_SUBST(mailuser)
AC_SUBST(mailgroup)

if test x$GCC = xyes
then
	CFLAGS="-Wall $CFLAGS"
	CXXFLAGS="-Wall $CXXFLAGS"
fi
CFLAGS="-I.. -I$srcdir/.. -I$srcdir/../libs -I$srcdir/../.. -I../.. -I$srcdir../../libs -I../../libs -I$srcdir/../../libs $CFLAGS"
CXXFLAGS="-I.. -I$srcdir/.. -I$srcdir/../libs -I$srcdir/../.. -I../.. -I$srcdir../../libs -I../../libs -I$srcdir/../../libs $CXXFLAGS"

INSTALL_SCRIPT='${INSTALL} -m 500'

AC_PATH_PROGS(COURIERAUTHCONFIG, courierauthconfig)

if test "$COURIERAUTHCONFIG" = ""
then
	AC_MSG_ERROR(courierauthconfig not found)
fi

. ../../dbobj.config
dblibrary="../../$dblibrary"
AC_SUBST(dblibrary)

AC_PATH_PROG(SETENV, env, env, $LPATH)

if test "$SETENV" = "env"
then
	AC_MSG_ERROR(env not found.)
fi

AC_SUBST(SETENV)

AC_CONFIG_SUBDIRS(libfilter perlfilter)

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/../../libs/docbook)

AC_OUTPUT(Makefile filterctl courierfilter.start verifysender verifysenderfull)
