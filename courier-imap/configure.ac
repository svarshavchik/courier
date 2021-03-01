dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 1998 - 2021 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_PREREQ(2.59)
AC_INIT(courier-imap, 5.0.14.20210301, [courier-imap@lists.sourceforge.net])
AC_CONFIG_SRCDIR(libs/imap/imapd.c)
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE
>confdefs.h # Kill PACKAGE_ macros

AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_SYSCONFTOOL
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL

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
http://www.courier-mta.org/maildirmake.html, or in the man page in
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

AC_ARG_ENABLE(root-check, [  --disable-root-check    Allow build as root (make check will fail)],
	rootcheck="$enableval",
	rootcheck="yes")

if test "$rootcheck" = "yes"
then
if test -w /etc
then
	AC_MSG_WARN(=== Do not compile Courier-IMAP as root.  Compile)
	AC_MSG_WARN(=== Courier-IMAP as a non-root user then su to)
	AC_MSG_WARN(=== root before running make install.  You must now)
	AC_MSG_WARN(=== remove this entire directory and then extract the)
	AC_MSG_WARN(=== source code from the tarball as a non-root user)
	AC_MSG_WARN(=== and rerun the configure script.  If you have read)
	AC_MSG_WARN(=== the INSTALL file you should have known this.  So)
	AC_MSG_WARN(=== you better read INSTALL again.)
	AC_MSG_ERROR(aborted.)
fi
fi

PKG_CHECK_MODULES(LIBIDN, libidn >= 0.0.0, [libidn=yes], [libidn=no])
if test "$libidn" != "yes"
then
	AC_MSG_ERROR([libidn not found])
fi

AC_ARG_WITH(redhat, [], [ redhat=$withval ], [redhat=no])

if test "$srcdir" = "."
then
	case `./config.guess` in
	*-redhat-*)
		if test "$redhat" = "no"
		then
			hash='#'
			AC_MSG_WARN(=== I think you are trying to run this configure script)
			AC_MSG_WARN([=== on Red Hat/Fedora.  You're doing too much work!])
			AC_MSG_WARN([=== It's much faster to create installable binary RPMs])
			AC_MSG_WARN([=== like this: http://www.courier-mta.org/FAQ.html${hash}rpm])
			AC_MSG_WARN(=== When you do this you may find that RPM will tell you)
			AC_MSG_WARN([=== to install some other software first, before trying to])
			AC_MSG_WARN([=== build this one, and even tell you the name of RPMs you])
			AC_MSG_WARN([=== need to install from the distribution CD.  That's much])
			AC_MSG_WARN([=== easier than trying to figure out the same from some])
			AC_MSG_WARN([=== cryptic error message.])
			AC_MSG_WARN([])
			AC_MSG_WARN([=== Even if you don't intend to use everything you need to])
			AC_MSG_WARN([=== have in order to build via RPM, you should still do as])
			AC_MSG_WARN([=== you're told.  All the extra stuff (LDAP, SQL, etc...)])
			AC_MSG_WARN([=== goes into RPM sub-packages, which do not need to be])
			AC_MSG_WARN([=== installed.])
			AC_MSG_WARN([=== But, if you insist, you can simply add '--with-redhat'])
			AC_MSG_WARN(=== parameter to this configure script and not see this)
			AC_MSG_WARN(=== error message.  You should also do this when upgrading)
			AC_MSG_WARN([=== and you didn't use RPM with the older version.])
			AC_MSG_ERROR(... in either case you better know what you're doing!)
		fi
		;;
	*)
		;;
	esac
fi

AC_PATH_PROGS(COURIERAUTHCONFIG, courierauthconfig)

if test "$COURIERAUTHCONFIG" = ""
then
	hash='#'
	AC_MSG_WARN(=== Courier authentication library not found.)
	AC_MSG_WARN(=== You need to download and install)
	AC_MSG_WARN([=== http://www.courier-mta.org/download.php${hash}authlib first.])
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

AC_CONFIG_FILES(Makefile dbobj.h dbobj.config imapd.rc imapd-ssl.rc
		pop3d.rc pop3d-ssl.rc makeimapaccess
		authconfig
		courier-imap.spec
		INSTALL.html
		packaging/suse/courier-imap.init
		packaging/systemd/courier-imap.sysvinit
		packaging/systemd/courier-imap.service)
AC_OUTPUT
