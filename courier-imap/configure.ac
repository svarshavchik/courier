dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 1998 - 2025 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT([courier-imap],[5.2.11.20250504],[courier-imap@lists.sourceforge.net])
AC_CONFIG_SRCDIR(libs/imap/imapd.c)
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE(foreign)
>confdefs.h # Kill PACKAGE_ macros

AC_PROG_CC
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_SYSCONFTOOL
LT_INIT

AC_PATH_PROGS(PERL, perl5 perl, perl)
AC_PATH_PROGS(SETENV, env)

defaultprefix="/usr/lib/courier-imap"
AC_SUBST(defaultprefix)
AC_ARG_WITH(notice,[AS_HELP_STRING([--with-notice=unicode],
[The --with-notice=unicode option is required, and indicates that you
are aware of the instructions in INSTALL for upgrading to this
version])], [notice="$withval"], [])

case "$notice" in
unicode)
	;;
*)
	AC_MSG_ERROR([
************************************************************************
Please rerun configure and add the

      --with-notice=unicode

option.

This indicates that you've read the instructions in the maildirmake
manual page (see "Converting pre-unicode format maildirs" at
https://www.courier-mta.org/maildirmake.html, or in the man page in
this package) about updating from earlier versions; specifically the
need to convert any existing maildirs to a Unicode naming scheme,
and the need to update any other Courier packages together with
updating this one.

If you are building RPMs directly using the spec file from the source
tarball, add a

      --define 'notice_option --with-notice=unicode'

option to the rpmbuild command:

$ rpmbuild -ta --define 'notice_option --with-notice=unicode'

This message is only meant to make sure you are aware of the contents
of the man page about updating from earlier versions, and the need for
manual steps to update any existing maildirs.

If you are installing for the first time, you can simply rerun configure
with this option.
************************************************************************])
	;;
esac

PKG_CHECK_MODULES(LIBIDN, libidn2 >= 2.0.5, [libidn=yes], [libidn=no])
if test "$libidn" != "yes"
then
	AC_MSG_ERROR([libidn not found])
fi

AC_PATH_PROGS(COURIERAUTHCONFIG, courierauthconfig)

if test "$COURIERAUTHCONFIG" = ""
then
	hash='#'
	AC_MSG_WARN(=== Courier authentication library not found.)
	AC_MSG_WARN(=== You need to download and install)
	AC_MSG_WARN([=== https://www.courier-mta.org/download.php${hash}authlib first.])
	AC_MSG_WARN(=== If courier-authlib is installed in a non-default)
	AC_MSG_WARN([=== directory, set the COURIERAUTHCONFIG environment])
	AC_MSG_WARN(=== variable to the full path to the courierauthconfig)
	AC_MSG_WARN(=== binary and rerun this configure script.)
	AC_MSG_WARN([])
	AC_MSG_ERROR(courierauthconfig not found)
fi
if test `$COURIERAUTHCONFIG --version=0.70.0.20200518` = "no"
then
	AC_MSG_ERROR(Courier authentication library version 0.71 is required.)
fi

$COURIERAUTHCONFIG --configfiles >conftest || exit 1
sed 's/^/auth/' <conftest >conftest2
. ./conftest2
if test ! -x $authsbindir/courierlogger$EXEEXT
then
	AC_MSG_ERROR(authlib configuration error - $authsbindir/courierlogger$EXEEXT not found)
fi
courierlogger=$authsbindir/courierlogger$EXEEXT
AC_SUBST(courierlogger)

AX_COURIER_UNICODE_VERSION(2.1)

if test x$prefix = xNONE
then
	prefix="$defaultprefix"
	ac_configure_args="$ac_configure_args -prefix=$prefix"
fi

if test x$exec_prefix = xNONE
then
	exec_prefix="$prefix"
fi

eval "exec_prefix=$exec_prefix"
eval "libexecdir=$libexecdir"
eval "localstatedir=$localstatedir"
eval "datarootdir=$datarootdir"
eval "datadir=$datadir"

AC_ARG_WITH(certsdir, [ --with-certsdir Directory where certs are created ],
certsdir="$withval", certsdir="$datadir")

AC_SUBST(certsdir)

dnl Check whether we should use gdbm, or db.

needs_withdb=0

AC_ARG_WITH(db, [  --with-db=gdbm          Use the GDBM library.
  --with-db=db            Use the libdb.a library.],
	db="$withval", db=""; needs_withdb=1)

case "$db" in
gdbm)
	;;
db)
	;;
"")
	;;
*)
	AC_MSG_ERROR(Invalid --with-db option.)
	;;
esac

saveLIBS="$LIBS"

if test "$db" != "db"
then
	AC_CHECK_LIB(gdbm, gdbm_open, [ LIBGDBM=-lgdbm ; LIBS="-lgdbm $LIBS" ])
	AC_CHECK_FUNC(gdbm_open, [ HAVE_GDBM=y ])
fi

LIBS="$saveLIBS"

if test "$db" != "gdbm"
then

	AC_CHECK_LIB(db, dbopen, [ LIBDB=-ldb ; LIBS="-ldb $LIBS" ],
		AC_CHECK_LIB(db, db_open, [ LIBDB=-ldb ; LIBS="-ldb $LIBS"],
		AC_CHECK_LIB(db, db_env_create,
				[ LIBDB=-ldb; LIBS="-ldb $LIBS"])
	))

	AC_CHECK_FUNC(dbopen, HAVE_BDB=1)
	AC_CHECK_FUNC(db_open, HAVE_BDB=1)
	AC_CHECK_FUNC(db_env_create, HAVE_BDB=1)
fi

LIBS="$saveLIBS"

if test "$HAVE_GDBM$HAVE_BDB" = ""
then
	AC_MSG_ERROR(Cannot find either the gdbm or the db library.)
fi

USE_GDBM=0
USE_DB=0
if test "$HAVE_GDBM" = "y"
then
	if test "$needs_withdb" = 1
	then
		ac_configure_args="$ac_configure_args --with-db=gdbm"
	fi
	dblibrary=libs/gdbmobj/libgdbmobj.la
	USE_GDBM=1
else
	if test "$needs_withdb" = 1
	then
		ac_configure_args="$ac_configure_args --with-db=db"
	fi
	dblibrary=libs/bdbobj/libbdbobj.la
	USE_DB=1
fi

AC_SUBST(USE_GDBM)
AC_SUBST(USE_DB)
AC_SUBST(dblibrary)

AC_ARG_WITH(userdb, [], userdb="$withval", userdb=/etc/userdb
	ac_configure_args="$ac_configure_args --with-userdb=$userdb")
AC_SUBST(userdb)
AC_ARG_WITH(makedatprog, [], ,
	ac_configure_args="$ac_configure_args --with-makedatprog='${libexecdir}/makedatprog'")

AC_ARG_WITH(mailuser, [], , ac_configure_args="$ac_configure_args --with-mailuser=$authmailuser")
AC_ARG_WITH(mailgroup, [], , ac_configure_args="$ac_configure_args --with-mailgroup=$authmailgroup")
AC_SUBST(authmailuser)
AC_SUBST(authmailgroup)

AC_CONFIG_SUBDIRS(libs/numlib libs/bdbobj libs/gdbmobj libs/soxwrap
	libs/rfc822 libs/rfc1035 libs/rfc2045
	libs/md5 libs/sha1 libs/random128 libs/liblock
	libs/maildir libs/makedat libs/libhmac
	libs/waitlib libs/tcpd libs/rootcerts libs/imap)

# Courier-IMAP standalone does not use SOCKSv5

AC_ARG_WITH(socks, [], , ac_configure_args="$ac_configure_args --without-socks")

# Neither does it use the change password feature

AC_ARG_WITH(authchangepwdir, [], ,
	ac_configure_args="$ac_configure_args --with-authchangepwdir=/var/tmp/dev/null")

INSTALL_DATA='${INSTALL} -m 0600'
INSTALL_SCRIPT='${INSTALL}'

if ulimit -v 2>/dev/null
then
	ULIMIT='ulimit -v'
else
	ULIMIT='ulimit -d'
fi
AC_SUBST(ULIMIT)

#
# autoconf bug
#

ac_given_INSTALL="$INSTALL"

binPROGRAMS_exec='${binPROGRAMS}'
sbinPROGRAMS_exec='${sbinPROGRAMS}'
libexecPROGRAMS_exec='${libexecPROGRAMS}'

AC_SUBST(binPROGRAMS_exec)
AC_SUBST(sbinPROGRAMS_exec)
AC_SUBST(libexecPROGRAMS_exec)

AC_ARG_WITH(package, [], package="$withval",
	[
		package="$PACKAGE"
		ac_configure_args="$ac_configure_args --with-package=$PACKAGE --with-version=$VERSION"
	])

AM_CONDITIONAL(HAVE_HTML, test -f ${srcdir}/html/main.html)

AC_ARG_WITH(certsdir, [ --with-certsdir Directory where certs are created ],
		      certsdir="$withval", certsdir=$datadir)

env LC_ALL=C perl -e 'use POSIX qw(strftime); print strftime("DATE=\"%a %b %d %Y\"; DATEFULL=\"%a, %d %b %Y %H:%M:%S %z\"", localtime)' >configure.date
. ./configure.date
rm configure.date
AC_SUBST(DATE)
AC_SUBST(DATEFULL)


AC_CONFIG_FILES(Makefile dbobj.h dbobj.config imapd.rc imapd-ssl.rc
		pop3d.rc pop3d-ssl.rc makeimapaccess
		authconfig
		courier-imap.spec
		courier-debuild
		INSTALL.html
		packaging/suse/courier-imap.init
		packaging/systemd/courier-imap.sysvinit
		packaging/systemd/courier-imap.service)
AC_OUTPUT
