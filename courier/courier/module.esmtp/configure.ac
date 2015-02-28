dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 1998 - 2008 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(module.esmtp, 0.11, [courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AC_CONFIG_SRCDIR(Makefile.am)
AM_INIT_AUTOMAKE([foreign no-define])
LPATH="$PATH:/usr/local/bin"
AC_CONFIG_HEADERS(config.h)

dnl Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_AWK
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
AC_ARG_WITH(mailuser, [], mailuser="$withval",
	AC_MSG_ERROR(--with-mailuser missing))
AC_ARG_WITH(mailgroup, [], mailgroup="$withval",
	AC_MSG_ERROR(--with-mailgroup missing))
AC_ARG_WITH(piddir, [  --with-piddir         Directory where PID files are located ], piddir="$withval", piddir=/var/run)
AC_SUBST(piddir)
AC_ARG_WITH(courierlogger, [], courierlogger=$withval,
			   [
			   AC_MSG_ERROR(--with-courierlogger option missing)
			   ])
AC_SUBST(courierlogger)

version=`cat ../../version`

AC_SUBST(mailuser)
AC_SUBST(mailgroup)
AC_DEFINE_UNQUOTED(COURIER_VERSION,"$version",[ Courier version ])

AC_PATH_PROG(SED, sed, sed, $LPATH)
if test "$SED" = "sed"
then
	AC_MSG_ERROR(sed not found.)
fi
AC_SUBST(SED)


AC_ARG_WITH(shellpath, [  --with-shellpath=path   Initialize PATH to path for .courier programs],
	SHELLPATH="$withval",
	SHELLPATH="$prefix/bin:/bin:/usr/bin:/usr/local/bin")

AC_SUBST(SHELLPATH)
AC_PATH_PROG(SETENV, env, env, $LPATH)
if test "$SETENV" = "env"
then
	AC_MSG_ERROR(env not found.)
fi
AC_SUBST(SETENV)

. ../../dbobj.config
dblibrary="../../$dblibrary"
AC_SUBST(dblibrary)

saveLIBS="$LIBS"
AC_CHECK_LIB(dl, dlopen, [ LIBDL="-ldl" ])
LIBS="$saveLIBS"

AC_SUBST(LIBDL)

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

dnl Checks for header files.
AC_CHECK_HEADERS(sys/types.h sys/time.h sys/stat.h sys/wait.h netinet/tcp.h sysexits.h unistd.h fcntl.h syslog.h utime.h locale.h)
AC_HEADER_TIME

dnl Checks for typedefs, structures, and compiler characteristics.

AC_SIZE_T
AC_TYPE_SIGNAL
AC_SYS_LARGEFILE

AC_CACHE_CHECK([for socklen_t],
	esmtp_cv_hassocklen_t,

AC_COMPILE_IFELSE([
AC_LANG_SOURCE( [
#include <sys/types.h>
#include <sys/socket.h>

socklen_t sl_t;
],[
	accept(0, 0, &sl_t);
])],
	esmtp_cv_hassocklen_t=yes,
	esmtp_cv_hassocklen_t=no)
)

socklen_t="int"

if test $esmtp_cv_hassocklen_t = yes
then
	:
else
	AC_DEFINE_UNQUOTED(socklen_t, int, [ Default definition for socklen_t ])
fi

AC_CACHE_CHECK([for caddr_t],courier_cv_CADDR_T_DEFINED,

	AC_TRY_COMPILE(
[
#include <sys/types.h>
],[
caddr_t p=0;
],
	courier_cv_CADDR_T_DEFINED=yes,
	courier_cv_CADDR_T_DEFINED=no)
)

if test "$courier_cv_CADDR_T_DEFINED" = "no"
then
	AC_DEFINE_UNQUOTED(caddr_t,my_caddr_t,
		[ Whether caddr_t should be defined ])
fi

dnl Checks for library functions.

AC_CHECK_FUNCS(strerror strcasecmp strncasecmp utime utimes setlocale)

dnl Other options

if test x$GCC = xyes
then
	CFLAGS="-Wall $CFLAGS"
fi

CFLAGS="-I$srcdir/../.. -I../.. -I$srcdir/../../courier -I../../courier -I$srcdir/../../libs -I../../libs -I$srcdir/../../courier/libs $CFLAGS"

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

#
# Some fixed constants.
#

AC_ARG_WITH(paranoid-smtpext,
	[--with-paranoid-smtpext   Be paranoid with negotiating Courier SMTP extensions],
	paranoid="$withval")

if test "$paranoid" = "yes"
then
	AC_DEFINE_UNQUOTED(COURIER_EXTENSIONS,"250-XCOURIEREXTENSIONS\r\n",
	[ Courier extension string ])
	AC_DEFINE_UNQUOTED(IS_COURIER_EXTENSIONS,KEYWORD("XCOURIEREXTENSIONS"),
	[ Courier extension string ])
else
	AC_DEFINE_UNQUOTED(COURIER_EXTENSIONS,"",
	[ Courier extension string ])
	AC_DEFINE_UNQUOTED(IS_COURIER_EXTENSIONS,1,
	[ Courier extension string ])
fi

AC_DEFINE_UNQUOTED(EHLO_VERP_EXTENSION,"250-XVERP=Courier\r\n",
	[ Courier extension string ])
AC_DEFINE_UNQUOTED(EHLO_EXDATA_EXTENSION,"250-XEXDATA\r\n",
	[ Courier extension string ])
AC_DEFINE_UNQUOTED(EHLO_SECURITY_EXTENSION, ["250-XSECURITY=NONE,STARTTLS\r\n"],
	[ Courier extension string ])

AC_DEFINE_UNQUOTED(IS_EXDATA_KEYWORD,KEYWORD("XEXDATA"),
	[ Courier extension string ])
AC_DEFINE_UNQUOTED(IS_VERP_KEYWORD,KEYWORDARG("XVERP="),
	[ Courier extension string ])
AC_DEFINE_UNQUOTED(IS_SECURITY_KEYWORD,KEYWORDARG("XSECURITY="),
	[ Courier extension string ])

AC_SUBST(DEBUGLIB)

INSTALL_SCRIPT='${INSTALL} -m 555'

OPENSSL_PATH=$PATH:/sbin:/usr/sbin:/usr/local/sbin:/usr/local/bin
AC_PATH_PROGS(OPENSSL, openssl, openssl, $OPENSSL_PATH)

if test "$OPENSSL" = ""
then
	OPENSSL=/usr/local/bin/openssl
fi
AC_SUBST(OPENSSL)

AC_PATH_PROGS(CERTTOOL, certtool, , $OPENSSL_PATH)

if test "$CERTTOOL" = ""
then
	CERTTOOL=/usr/local/bin/certtool
fi

AC_ARG_WITH(random, [ --with-random=/dev/urandom - location of the system random file generator],
	RANDOMV="$withval",
RANDOMV="/dev/random"
if test -r /dev/urandom
then
        RANDOMV="/dev/urandom"
fi
	)

AC_SUBST(RANDOMV)

test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
eval "prefix=$prefix"
eval "exec_prefix=$exec_prefix"
eval "datarootdir=$datarootdir"
eval "sysconfdir=$sysconfdir"
eval "sbindir=$sbindir"
eval "bindir=$bindir"
eval "datadir=$datadir"
eval "localstatedir=$localstatedir"

mydatadir="$datadir"
AC_SUBST(mydatadir)   # Quell automake warning


dnl
dnl  Check what libraries are available
dnl

SASLLIBS=""

if test -d ../../libs/libhmac
then
	SASLLIBS="$SASLLIBS ../../libs/libhmac/libhmac.la"
fi

if test -d ../../libs/sha1
then
	SASLLIBS="$SASLLIBS ../../libs/sha1/libsha1.la"
fi

if test -d ../../libs/md5
then
	SASLLIBS="$SASLLIBS ../../libs/md5/libmd5.la"
fi
AC_SUBST(SASLLIBS)

rm -f esmtp.authpam

cp -f $srcdir/esmtp.authpam.dist esmtp.authpam

if test -f /etc/pam.d/system-auth
then
	if ls /lib*/security/pam_stack.so 2>/dev/null >/dev/null
	then
		cp -f $srcdir/../../libs/imap/system-auth.authpam esmtp.authpam
	else
		cp -f $srcdir/../../libs/imap/system-auth2.authpam esmtp.authpam
	fi
fi

. ../../libs/rootcerts/rootcertsdir.cnf
AC_SUBST(cacerts)

. ../../libs/tcpd/couriertls.config
if test "$ssllib" = ""
then
	ssllib="gnutls"
fi
AC_SUBST(ssllib)

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/../../libs/docbook)

# This gets built by the make rule, and is not in the git repo

for f in esmtpd.dist.in esmtpd-ssl.dist.in esmtpd-msa.dist.in
do
    if test ! -f "$srcdir/$f"
    then
	cp -p "$srcdir/$f.git" "$srcdir/$f"
	touch "$srcdir/$f.git"
    fi
done

AC_OUTPUT(Makefile esmtpd esmtpd-ssl \
	esmtpd.dist esmtpd-msa.dist esmtpd-ssl.dist  \
	makesmtpaccess makeacceptmailfor \
	makepercentrelay staticlist.c mkesmtpdcert esmtpd.cnf.openssl)
