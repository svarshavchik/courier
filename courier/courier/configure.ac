dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 1998 - 2017 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(courier, 0.78, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(submit.C)
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([foreign])
LPATH="$PATH:/usr/local/bin"
AC_CONFIG_HEADERS(config.h)

dnl Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL

AC_PATH_PROG(PROCMAIL, procmail, /usr/bin/procmail, $LPATH)
AC_PATH_PROG(CAT, cat, cat, $LPATH)
AC_PATH_PROG(SED, sed, sed, $LPATH)
if test "$SED" = "sed"
then
	AC_MSG_ERROR(sed not found.)
fi
AC_SUBST(SED)

AC_PATH_PROG(SORT, sort, sort, $LPATH)
if test "$SORT" = "sort"
then
	AC_MSG_ERROR(sort not found)
fi
AC_SUBST(SORT)

AC_PATH_PROG(SETENV, env, env, $LPATH)
if test "$SETENV" = "env"
then
	AC_MSG_ERROR(env not found.)
fi
AC_SUBST(SETENV)

AC_PATH_PROGS(COURIERAUTHCONFIG, courierauthconfig)

if test "$COURIERAUTHCONFIG" = ""
then
	AC_MSG_ERROR(courierauthconfig not found)
fi

AX_COURIER_UNICODE_CXXFLAGS

CFLAGS="-I$srcdir/.. -I.. -I../libs -I$srcdir/../libs -I$srcdir/libs $CFLAGS"
CXXFLAGS="-I$srcdir/.. -I.. -I../libs -I$srcdir/../libs -I$srcdir/libs $CXXFLAGS $COURIER_UNICODE_CXXFLAGS"

AC_ARG_WITH(softlinkdir, [  --with-softlinks=dir:link,dir:link...   specify soft links for
                                          installation subdirectories.], ,
	ac_configure_args="$ac_configure_args --with-softlinkdir=$prefix")

version=`cat ../version`

COPYRIGHT="Courier $version Copyright 1999-2017 Double Precision, Inc."
AC_SUBST(COPYRIGHT)

dnl Check whether we should use gdbm, or db.
#
#
# We may need libdl for later, not just shared check.
#

saveLIBS="$LIBS"
AC_CHECK_LIB(dl, dlopen, [ LIBDL="-ldl" ])
LIBS="$saveLIBS"

dnl Checks for transport libraries.

#
# Figure out what goes into make dist.  Search for all the available
# module. subdirectories
#

distdirlist=""

AC_MSG_CHECKING(for available modules)
for f in $srcdir/module.*/courier.config
do
	test -f "$f" || continue
	dir=`echo $f | sed 's:/courier.config$::;s:.*/::;'`
	name=` . $f ; echo $NAME`
	distdirlist="$distdirlist $dir"
	eval "DIR_$name=$dir"
done
AC_MSG_RESULT($distdirlist)

AC_ARG_WITH(transport,
[  --with-transport='local esmtp ...'
                          Specify list of modules to install],
	transport="$withval",
	transport=`
		for f in $distdirlist
		do
			( . $srcdir/$f/courier.config ; echo $PRIORITY $NAME )
		done | sort -n | awk ' { print $2 } '
		`
	transport=`echo $transport`
	ac_configure_args="$ac_configure_args --with-transport='$transport'",
	)

AC_SUBST(transport)

AC_MSG_CHECKING(for modules to compile)

dirlist=""
for module in $transport
do
	name='$DIR_'$module
	eval "dir=\"$name\""
	if test "$dir" = ""
	then
		AC_MSG_ERROR(Driver not available: $module)
	fi
	dirlist="$dirlist $dir"
done

AC_MSG_RESULT($dirlist)
AC_SUBST(dirlist)
AC_SUBST(distdirlist)

AC_ARG_WITH(db, [], db="$withval", db="")

. ../dbobj.config
dblibrary="../$dblibrary"
AC_SUBST(dblibrary)

AC_ARG_WITH(courierlogger, [], courierlogger=$withval,
			   [
			   AC_MSG_ERROR(--with-courierlogger option missing)
			   ])

AC_SUBST(courierlogger)

USENSL=no
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

AC_CHECK_HEADERS(netinet/in.h)

saveLIBS="$LIBS"
LIBS="$NETLIBS $LIBS"
LDAPLIBS=""
AC_CHECK_LIB(lber, ber_scanf,
	[ LDAPLIBS="-llber" LIBS="-llber $LIBS" ])


        AC_MSG_CHECKING(whether -lresolv is needed for res_query)
	AC_LINK_IFELSE([AC_LANG_PROGRAM([
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <resolv.h>

void (*func)()= (void (*)())res_query;
],
			[  (*func)(); ])],
        	AC_MSG_RESULT(no),
		[
        	LIBS="-lresolv $LIBS"

		AC_LINK_IFELSE([AC_LANG_PROGRAM([
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <resolv.h>

void (*func)()= (void (*)())res_query;
],
			[  (*func)(); ])], [
        		LDAPLIBS="-lresolv $LDAPLIBS"
			HAVE_LDAP=1
        		AC_MSG_RESULT(yes)
			],
        		AC_MSG_ERROR(Cannot find function res_query))
			])

AC_CHECK_LIB(ldap, ldap_open,
	[ LDAPLIBS="-lldap $LDAPLIBS" ; LIBS="-lldap $LIBS" ])
AC_CHECK_FUNC(ldap_search_st, HAVE_LDAP=1, HAVE_LDAP=0)
LIBS="$saveLIBS"

AC_ARG_WITH(ldapaliasd, [ --without-ldapaliasd ],
			ldapaliasd="$withval",
			ldapaliasd="yes")

if test "$ldapaliasd" = "no"
then
	LDAPLIBS=""
	HAVE_LDAP=0
fi


AC_SUBST(LDAPLIBS)

if test $HAVE_LDAP = 1
then
	COURIERLDAPALIASD=courierldapaliasd
	LDAPALIASRC=ldapaliasrc.dist
fi

AC_SUBST(COURIERLDAPALIASD)
AC_SUBST(LDAPALIASRC)
AC_SUBST(HAVE_LDAP)

AC_CONFIG_SUBDIRS( libs filters doc webadmin module.dsn module.esmtp module.fax module.local module.uucp )

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADERS(sys/types.h sys/time.h sys/stat.h sys/wait.h sys/file.h sysexits.h unistd.h fcntl.h syslog.h locale.h langinfo.h sys/ioctl.h ldap.h lber.h)
AC_HEADER_TIME

dnl Checks for typedefs, structures, and compiler characteristics.

AC_PID_T
AC_TYPE_UID_T
AC_TYPE_SIGNAL
AC_SYS_LARGEFILE

AC_COMPILE_IFELSE([AC_LANG_SOURCE([

#include <stdio.h>

int main()
{
   setvbuf(stderr, NULL, _IOLBF, BUFSIZ);
}
])],
	AC_DEFINE_UNQUOTED(HAVE_SETVBUF_IOLBF,1,[Whether setvbuf(..._IOLBF) works])
)

AC_CACHE_CHECK([for socklen_t],
	courier_cv_hassocklen_t,

AC_COMPILE_IFELSE([
AC_LANG_SOURCE( [
#include <sys/types.h>
#include <sys/socket.h>

socklen_t sl_t;
],[
	accept(0, 0, &sl_t);
])],
	courier_cv_hassocklen_t=yes,
	courier_cv_hassocklen_t=no)
)

socklen_t="int"

if test $courier_cv_hassocklen_t = yes
then
	:
else
	AC_DEFINE_UNQUOTED(socklen_t, int, [ Default definition for socklen_t ])
fi

dnl Checks for library functions.

AC_CHECK_FUNCS(strcasecmp strncasecmp setpgrp setgroups initgroups mkfifo setlocale)

AC_CHECK_FUNC(setpgrp,
	[
	AC_FUNC_SETPGRP
	]
)

dnl Other options

AC_ARG_WITH(mailuser, [], mailuser="$withval",
	AC_MSG_ERROR(--with-mailuser not set))

AC_ARG_WITH(mailgroup, [], mailgroup="$withval",
	AC_MSG_ERROR(--with-mailgroup not set))

AC_ARG_WITH(mailuid, [], mailuid="$withval",
	AC_MSG_ERROR(--with-mailuid not set))

AC_ARG_WITH(mailgid, [], mailgid="$withval",
	AC_MSG_ERROR(--with-mailgid not set))

AC_SUBST(mailuser)
AC_SUBST(mailgroup)
AC_SUBST(mailuid)
AC_SUBST(mailgid)

AC_ARG_WITH(explicitsync, [  --without-explicitsync  Do not manually sync queue files],
	explicitsync="$withval", explicitsync="Y")

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


AC_ARG_WITH(shellpath, [  --with-shellpath=path   Initialize PATH to path for .courier programs],
	SHELLPATH="$withval",
	SHELLPATH="$prefix/bin:/bin:/usr/bin:/usr/local/bin")

AC_SUBST(SHELLPATH)

#
# Default POP3 configuration
#

for f in $AUTHMODULELIST x
do
	if test "$f" = "authldap"
	then
		AC_ERROR(You must use authdaemon with authldap)
	fi
done

# Automake stupidity

INSTALL_PROGRAM='${INSTALL} -m 700'
INSTALL_SCRIPT='${INSTALL} -m 700'
INSTALL_DATA='${INSTALL} -m 600'

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
AC_LANG_C

AC_TRY_COMPILE( [
#include <signal.h>
#include <string.h>
#include <stdio.h>
],[
struct sigaction sa;

memset(&sa, 0, sizeof(sa));
sa.sa_handler=SIG_IGN;
sa.sa_flags=SA_NOCLDWAIT;
sigaction(SIGCHLD, &sa, NULL);
], AC_DEFINE_UNQUOTED(USE_NOCLDWAIT,1, [ Whether to use SA_NOCLDWAIT ]))

if test x$GCC = xyes
then
	CFLAGS="-Wall $CFLAGS"
fi
if test x$GXX = xyes
then
	CXXFLAGS="-Wall $CXXFLAGS"
fi

echo "mailuser=$mailuser" >uidgid
echo "mailgroup=$mailgroup" >>uidgid

if $SHELL -c 'ulimit -v 0 2>/dev/null'
then
	ULIMIT='ulimit -v'
else
	ULIMIT='ulimit -d'
fi

AC_SUBST(ULIMIT)

for f in /etc/environment /etc/default/locale /etc/sysconfig/i18n /etc/sysconfig/language /etc/locale.conf
do
	if test -f "$f"
	then
		INIT_LOCALE=". $f"
	fi
done

AC_SUBST(INIT_LOCALE)

# Unless you're the maintainer, reset DEBUGLIB just in case.
#
#case `hostname` in
#*.email-scan.com)
#	;;
#*)
	DEBUGLIB=""
#	;;
#esac

AC_SUBST(DEBUGLIB)

test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

eval "exec_prefix=$exec_prefix"
eval "datarootdir=$datarootdir"
eval "bindir=$bindir"
eval "localstatedir=$localstatedir"

MAILDROP="$bindir/maildrop"
AC_SUBST(MAILDROP)

AC_DEFINE_UNQUOTED(MAILDROP,"$MAILDROP",
	[ Whether maildrop is installed ])

. ../libs/tcpd/couriertls.config
AC_SUBST(couriertls)

# Some automake roughage
EXTRABIN='$(extrabin)'
AC_SUBST(EXTRABIN)
EXTRAPKGLIB='$(extrapkglib)'
AC_SUBST(EXTRAPKGLIB)
EXTRASBIN='$(extrasbin)'
AC_SUBST(EXTRASBIN)
SBINSCRIPTS='$(sbinscripts)'
AC_SUBST(SBINSCRIPTS)
EXTRACMDLIB='$(extracmdlib)'
AC_SUBST(EXTRACMDLIB)
EXTRAWEBMAIL='$(extrawebmail)'
AC_SUBST(EXTRAWEBMAIL)

. ../libs/rootcerts/rootcertsdir.cnf
AC_SUBST(cacerts)

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/../libs/docbook)

# This gets built by the make rule, and is not in the git repo

for f in courierd.dist.in webmlmrc.dist.in
do
    if test ! -f "$srcdir/$f"
    then
	cp -p "$srcdir/$f.git" "$srcdir/$f"
	touch "$srcdir/$f.git"
    fi
done

AC_OUTPUT(Makefile courier.h \
	courierctl.start \
	courierd.dist \
	dot-qmail-to-dot-courier.pl \
	pop3d \
	pop3d-ssl \
	imapd.rc \
	imapd-ssl.rc \
	makealiases \
	makehosteddomains \
	makeimapaccess \
	mkmodules.sh perms.sh setperms.sh \
	perftest1 webmaild webmlmrc.dist webmlmd.rc)
