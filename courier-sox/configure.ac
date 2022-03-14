#
# Copyright 2004-2022 Double Precision, Inc.
# See COPYING for distribution information.
#
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.63)
AC_INIT([courier-sox],[0.15.20220416],[courier-users@lists.sourceforge.net])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([sox.c])
AC_CONFIG_HEADERS([courier_socks_config.h])
AC_USE_SYSTEM_EXTENSIONS

LT_CONFIG_LTDL_DIR([libltdl])
LT_INIT(dlopen)

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
LT_INIT(dlopen)
LTDL_INIT
AC_SUBST(LTDLINCL)
AC_SUBST(LIBLTDL)

AC_CHECK_HEADER(ltdl.h, [ : ],
			AC_MSG_ERROR([Unable to find ltdl.h. Please install Libtool's ltdl library]))

AC_CHECK_PROGS(SED, sed, sed)

AC_PATH_PROGS(PERL, perl5 perl, perl)
AC_PATH_PROGS(COURIERAUTHCONFIG, courierauthconfig)

AUTHVER=0.56.20050702

if test "$COURIERAUTHCONFIG" != ""
then
	if test `$COURIERAUTHCONFIG --version=$AUTHVER` != 'yes'
	then
		COURIERAUTHCONFIG=""
	fi
fi

if test "$COURIERAUTHCONFIG" = ""
then
	hash='#'
	AC_MSG_WARN(=== Courier authentication library version $AUTHVER is required.)
	AC_MSG_WARN(=== You need to download and install)
	AC_MSG_WARN([=== http://www.courier-mta.org/download.php${hash}authlib first.])
	AC_MSG_WARN(=== If courier-authlib is installed in a non-default)
	AC_MSG_WARN([=== directory, set the COURIERAUTHCONFIG environment])
	AC_MSG_WARN(=== variable to the full path to the courierauthconfig)
	AC_MSG_WARN(=== binary and rerun this configure script.)
	AC_MSG_WARN([])
	AC_MSG_ERROR(courierauthconfig not found)
fi

$COURIERAUTHCONFIG --configfiles | sed 's/^/auth/' >configtmp
. ./configtmp
courierlogger=$authsbindir/courierlogger
rm -f configtmp
AC_SUBST(courierlogger)

COURIERAUTHINCLUDE="`$COURIERAUTHCONFIG --cppflags`"
COURIERAUTHLINK="`$COURIERAUTHCONFIG --ldflags`"
AC_SUBST(COURIERAUTHINCLUDE)
AC_SUBST(COURIERAUTHLINK)


LIBVERSION_CURRENT=0
LIBVERSION_RELEASE=0
LIBVERSION_AGE=0

LIBVERSION_INFO=$LIBVERSION_CURRENT:$LIBVERSION_RELEASE:$LIBVERSION_AGE
AC_SUBST(LIBVERSION_CURRENT)
AC_SUBST(LIBVERSION_RELEASE)
AC_SUBST(LIBVERSION_AGE)
AC_SUBST(LIBVERSION_INFO)

SOVERSION=0
if test "$LIBVERSION_CURRENT" -ne "$LIBVERSION_AGE"
then
	SOVERSION=`expr $LIBVERSION_CURRENT - $LIBVERSION_AGE`
fi

AC_SUBST(SOVERSION)

# Checks for libraries.
saveLIBS="$LIBS"
NETLIBS=""
USENSL=no

LIBS="$OLDLIBS -lsocket"

AC_TRY_LINK([
#include <sys/types.h>
#include <sys/socket.h>
], [
(void)socket(0, 0, 0);
], [
   NETLIBS="-lsocket"
],
[

LIBS="$saveLIBS -lsocket -lnsl"

AC_TRY_LINK([
#include <sys/types.h>
#include <sys/socket.h>
], [
(void)socket(0, 0, 0);
], [
   NETLIBS="-lsocket -lnsl"
   USENSL=yes
], [

LIBS="$saveLIBS -lsocket"

AC_TRY_LINK([
#include <sys/types.h>
#include <sys/socket.h>
], [
(void)connect(0, 0, 0);
], [
   NETLIBS="-lsocket"
],
[

LIBS="$saveLIBS -lsocket -lnsl"
AC_TRY_LINK([
#include <sys/types.h>
#include <sys/socket.h>
], [
(void)connect(0, 0, 0);
], [
   NETLIBS="-lsocket -lnsl"
   USENSL=yes
],
[
])
])
])
])

if test $USENSL != yes; then
	LIBS="$saveLIBS $NETLIBS"
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

AC_CHECK_LIB(dl,dlopen,DLLIB="-ldl",DLLIB="")
AC_SUBST(DLLIB)

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/select.h sys/poll.h pthread.h])
AC_CHECK_HEADER([sys/select.h], [sys_select_h=1], [sys_select_h=0])
AC_CHECK_HEADER([sys/poll.h], [sys_poll_h]=1, [sys_poll_h=0])
AC_SUBST(sys_select_h)
AC_SUBST(sys_poll_h)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_SYS_LARGEFILE

# Checks for library functions.

AC_CACHE_CHECK([for IPv6 support],
	sox_cv_hasipv6structs,

AC_COMPILE_IFELSE([
AC_LANG_SOURCE( [
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

        ], [
struct in6_addr in6a;
struct sockaddr_in6 sain6;
struct sockaddr_storage soas;
int x=PF_INET6;

	])], sox_cv_hasipv6structs=yes,
                sox_cv_hasipv6structs=no )
)

storage="struct sockaddr"
storage_family="sa_family"

HAVE_IPV6=0
IPV6_NOPROXY=""

if test $sox_cv_hasipv6structs = yes
then
	storage="struct sockaddr_storage"
	storage_family="ss_family"
        HAVE_IPV6=1
	IPV6_NOPROXY='noproxy ::1'
	AC_DEFINE_UNQUOTED(HAVE_IPV6, 1, [ Whether IPv6 support is available ])
fi
AC_SUBST(HAVE_IPV6)
AC_SUBST(IPV6_NOPROXY)

AC_COMPILE_IFELSE([
AC_LANG_SOURCE( [
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern int foobar2(const struct sockaddr *);

int foobar(__CONST_SOCKADDR_ARG addr)
{
	foobar2(addr.__sockaddr__);
}
        ], [
])],
	[ SOCKADDR_CONST_ARG_PTR="__CONST_SOCKADDR_ARG"
	  SOCKADDR_ARG_PTR="__SOCKADDR_ARG"
	  GET_SOCKADDR_PTR="GET_SOCKADDR_UNION"
	  ],
	[ SOCKADDR_CONST_ARG_PTR="const struct sockaddr *"
	  SOCKADDR_ARG_PTR="struct sockaddr *"
	  GET_SOCKADDR_PTR="GET_SOCKADDR_DIRECT" ])

AC_DEFINE_UNQUOTED(SOCKADDR_CONST_ARG_PTR,$SOCKADDR_CONST_ARG_PTR,
	[ Type for a const struct sockaddr *])
AC_DEFINE_UNQUOTED(SOCKADDR_ARG_PTR,$SOCKADDR_ARG_PTR,
	[ Type for a struct sockaddr *])
AC_DEFINE_UNQUOTED(GET_SOCKADDR_PTR,$GET_SOCKADDR_PTR,
	[ How to convert platform-specific sockaddr ptr to a stock ptr ])

AC_DEFINE_UNQUOTED(SOCKADDR_STORAGE, $storage, [ Whether sockaddr_storage is available ])
AC_DEFINE_UNQUOTED(SS_family, $storage_family, [ The address family field in SOCKADDR_STORAGE])

AC_CACHE_CHECK([for socklen_t],
	sox_cv_hassocklen_t,

AC_COMPILE_IFELSE([
AC_LANG_SOURCE( [
#include <sys/types.h>
#include <sys/socket.h>

socklen_t sl_t;
],[
	accept(0, 0, &sl_t);
])],
	sox_cv_hassocklen_t=yes,
	sox_cv_hassocklen_t=no)
)

socklen_t="int"

if test $sox_cv_hassocklen_t = yes
then
	socklen_t="socklen_t"
fi
AC_SUBST(socklen_t)
AC_DEFINE_UNQUOTED(SOCKLEN_T, $socklen_t, [ Whether socklen_t is defined ])

AC_CACHE_CHECK([for nfds_t],
	sox_cv_hasnfds_t,

AC_COMPILE_IFELSE([
AC_LANG_SOURCE( [
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>

nfds_t n;
],[
	n=0;
])],
	sox_cv_hasnfds_t=yes,
	sox_cv_hasnfds_t=no)
)

nfds_t="unsigned int"

if test $sox_cv_hasnfds_t = yes
then
	nfds_t="nfds_t"
fi
AC_SUBST(nfds_t)
AC_DEFINE_UNQUOTED(NFDS_T, $nfds_t, [ Whether nfds_t is defined ])



AC_CACHE_CHECK([for sa_family_t],
	sox_cv_hassa_family_t,

AC_COMPILE_IFELSE([
AC_LANG_SOURCE( [
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

sa_family_t af;
],[
	af=0;
])],
	sox_cv_hassa_family_t=yes,
	sox_cv_hassa_family_t=no)
)

sa_family_t="int"

if test $sox_cv_hassa_family_t = yes
then
	sa_family_t="sa_family_t"
fi

AC_DEFINE_UNQUOTED(SA_FAMILY_T, $sa_family_t, [ Whether sa_family_t is defined ])

AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strchr strdup getaddrinfo poll inet_pton])

AC_MSG_CHECKING([whether we have figured out all the prototypes correctly.])

AC_COMPILE_IFELSE([
AC_LANG_SOURCE( [
#include "$srcdir/stubfuncs.h"

extern void makeused(void *);

],[
	ptr_connect=connect; makeused(ptr_connect);
	ptr_bind=bind; makeused(ptr_bind);
	ptr_select=select; makeused(ptr_select);
	ptr_getsockopt=getsockopt; makeused(ptr_getsockopt);
	ptr_close=close; makeused(ptr_close);
	ptr_fclose=fclose; makeused(ptr_fclose);
	ptr_listen=listen; makeused(ptr_listen);
	ptr_accept=accept; makeused(ptr_accept);
	ptr_getsockname=getsockname; makeused(ptr_getsockname);
	ptr_getpeername=getpeername; makeused(ptr_getpeername);
	ptr_dup=dup; makeused(ptr_dup);
	ptr_dup2=dup2; makeused(ptr_dup2);
#if HAVE_POLL
	ptr_poll=poll; makeused(ptr_poll)
#endif
])],
	[ AC_MSG_RESULT(yes) ],
	[ AC_MSG_RESULT(no)
	AC_MSG_ERROR(Cannot determine system function prototypes on this platform.)])

PTHREADLIB=""

have_pthreads=no

AC_CHECK_FUNC(pthread_mutex_init, [have_pthreads="yes"],
		[AC_CHECK_LIB(pthread, pthread_mutex_init,
		       [PTHREADLIB="-lpthread"; have_pthreads="yes"],
		       [PTHREADLIB=""])])

if test "$have_pthreads" = "yes"
then
	AC_DEFINE_UNQUOTED(HAVE_PTHREADS, 1,
					  [ Whether pthreads are available ])
fi
AC_SUBST(PTHREADLIB)

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/libs/docbook)

test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
eval "exec_prefix=$exec_prefix"
eval "sysconfdir=$sysconfdir"
eval "bindir=$bindir"
eval "sbindir=$sbindir"

if test "$GCC" = "yes"
then
	CFLAGS="$CFLAGS -Wall -pedantic"
fi
CFLAGS="-Ilibs -I$srcdir/libs $CFLAGS"
AC_ARG_WITH(repository, [], REPOSITORY="$withval")
AC_SUBST(REPOSITORY)

# Turn off socks stuff in soxwrap

AC_ARG_WITH(socks, [], [ : ], [ac_configure_args="$ac_configure_args --without-socks"])

AC_CONFIG_SUBDIRS(libs/soxwrap)
AC_CONFIG_FILES(Makefile
	testcidr.sh
	courier-debuild
	courier-sox.spec
	courier-sox.sysvinit
	courier-sox.service
	socks.h
	system
	sockd.rc
	mkbl4)
AC_OUTPUT
