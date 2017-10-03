dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 1998 - 2017 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_PREREQ(2.59)

AC_INIT(courier, 0.78.1, [courier-users@lists.sourceforge.net])
version=$PACKAGE_VERSION
AC_CONFIG_SRCDIR(courier/courier.c)
AM_INIT_AUTOMAKE
>confdefs.h # Kill PACKAGE_ macros.

LPATH="$PATH:/usr/local/bin"

AC_PROG_CC
AM_PROG_LIBTOOL

AC_PROG_MAKE_SET
AC_PROG_CXX
AC_PROG_SYSCONFTOOL
AC_PATH_PROGS(SETENV, env)
AX_COURIER_UNICODE_VERSION
AC_ARG_ENABLE(root-check, [  --disable-root-check    Allow build as root (make check will fail)],
	rootcheck="$enableval",
	rootcheck="yes")

if test "$rootcheck" = "yes"
then
if test `id -u` = 0
then
	AC_MSG_WARN(=== Do not compile Courier as root.  Compile)
	AC_MSG_WARN(=== Courier as a non-root user then su to)
	AC_MSG_WARN(=== root before running make install.  You must now)
	AC_MSG_WARN(=== remove this entire directory and then extract the)
	AC_MSG_WARN(=== source code from the tarball as a non-root user)
	AC_MSG_WARN(=== and rerun the configure script.  If you have read)
	AC_MSG_WARN(=== the INSTALL file you should have known this.  So)
	AC_MSG_WARN(=== you better read INSTALL again, as well.)
	AC_MSG_ERROR(aborted.)
fi
fi

AC_CHECK_HEADER([pcre.h],
		[ : ],
		[

		AC_CHECK_HEADER([pcre/pcre.h],
				[ : ],
				[
						AC_MSG_ERROR(pcre.h not found - install PCRE from www.pcre.org)
				])
		]
)

if test x$prefix = xNONE
then
	prefix="/usr/lib/courier"
	ac_configure_args="$ac_configure_args --prefix=$prefix"
fi

if test x$exec_prefix = xNONE
then
	exec_prefix="$prefix"
fi

eval "libexecdir=$libexecdir"
eval "sysconfdir=$sysconfdir"
eval "localstatedir=$localstatedir"
eval "datadir=$datadir"

AC_ARG_WITH(certsdir, [ --with-certsdir Directory where certs are created ],
		      certsdir="$withval", certsdir=$datadir)

AC_ARG_WITH(makedatprog, [], ,
	ac_configure_args="$ac_configure_args --with-makedatprog=${libexecdir}/courier/makedatprog")

AC_ARG_WITH(piddir, [], ,
	ac_configure_args="$ac_configure_args --with-piddir=${localstatedir}/tmp")

AC_ARG_WITH(package, [], package="$withval",
	[
		package="$PACKAGE"
		ac_configure_args="$ac_configure_args --with-package=$PACKAGE --with-version=$VERSION"
	])

echo $version >version

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

$COURIERAUTHCONFIG --configfiles | sed 's/^/auth/' >configtmp
. ./configtmp
AC_ARG_WITH(courierlogger, [], : ,
			   [
			   ac_configure_args="$ac_configure_args --with-courierlogger=$authsbindir/courierlogger"])

if test `$COURIERAUTHCONFIG --version=0.65.0.20130910` = "no"
then
	AC_MSG_ERROR(Courier authentication library version 0.66 is required.)
fi

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
		[ AC_CHECK_LIB(db, db_open, [ LIBDB=-ldb ; LIBS="-ldb $LIBS"],
		[ AC_CHECK_LIB(db, db_env_create,
				[ LIBDB=-ldb; LIBS="-ldb $LIBS"]) ]
		)])

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
	USE_GDBM=1
	DBSUBDIR=libs/gdbmobj
	if test "$needs_withdb" = 1
	then
		ac_configure_args="$ac_configure_args --with-db=gdbm"
	fi
	dblibrary=libs/gdbmobj/libgdbmobj.la
else
	USE_DB=1
	DBSUBDIR=libs/bdbobj
	if test "$needs_withdb" = 1
	then
		ac_configure_args="$ac_configure_args --with-db=db"
	fi
	dblibrary=libs/bdbobj/libbdbobj.la
fi

AC_SUBST(DBSUBDIR)
AC_SUBST(USE_GDBM)
AC_SUBST(USE_DB)
AC_SUBST(LIBDB)
AC_SUBST(LIBGDBM)
AC_SUBST(dblibrary)

dnl
dnl Need to settle on our uid/gids here
dnl

changequote()

LB='['
RB=']'

changequote([,])

AC_ARG_WITH(mailuser,[],
	mailuser="$withval",
	[ mailuser="$authmailuser"
	ac_configure_args="$ac_configure_args --with-mailuser=$mailuser"]
	)

AC_ARG_WITH(mailgroup, [],
	mailgroup="$withval",
	[ mailgroup="$authmailgroup"
	ac_configure_args="$ac_configure_args --with-mailgroup=$mailgroup"]
	)

AC_ARG_WITH(mailuid,[],
	mailuid="$withval",
	[ mailuid="$authmailuid"
	ac_configure_args="$ac_configure_args --with-mailuid=$mailuid"]
	)

AC_ARG_WITH(mailgid, [],
	mailgid="$withval",
	[ mailgid="$authmailgid"
	ac_configure_args="$ac_configure_args --with-mailgid=$mailgid"]
	)

AC_SUBST(mailuser)
AC_SUBST(mailgroup)
AC_SUBST(mailuid)
AC_SUBST(mailgid)
rm -f configtmp

dnl for maildrop subdir

AC_ARG_ENABLE(trusted-groups,[], ,
	ac_configure_args="$ac_configure_args --enable-trusted-groups='$mailgroup'")

dnl for cgi subdir
AC_ARG_WITH(formdata, [], , ac_configure_args="$ac_configure_args --with-formdata")

#
# Unicode mess
#

AC_ARG_ENABLE(utf7-folder-encoding,
	[],
	[ utf7folder="$enableval"
	if test "$utf7folder" != "yes"
	then
		AC_MSG_ERROR(This option is no longer supported)
	fi
	] , [
	ac_configure_args="$ac_configure_args --enable-utf7-folder-encoding"
	utf7folder="yes"
	]
)

AC_PATH_PROGS(GPG, gpg gpg2, /usr/bin/gpg, $LPATH)

#
# Stick webadmin stuff here

AC_ARG_WITH(webadmindir, [] , ,
	ac_configure_args="$ac_configure_args --with-webadmindir=$datadir/courierwebadmin")

#
# autoconf bug
#

ac_given_INSTALL="$INSTALL"

#
# Get rid of internal arguments, save real arguments into configure.args

real_configure_args=""

for arg in $ac_configure_args
do
	case $arg in
	--with-courierlogger*)
		continue
		;;
	--with-makedatprog*)
		continue
		;;
	--with-webadmindir*)
		continue
		;;
	--with-piddir*)
		continue
		;;
	--with-package*)
		continue
		;;
	--with-version*)
		continue
		;;
	--enable-trusted-groups*)
		continue
		;;
	--with-formdata*)
		continue
		;;
	--with-mailuser*)
		;;
	--with-mailgroup*)
		;;
	--with-mailuid*)
		;;
	--with-mailgid*)
		;;
	*)
		real_configure_args="$real_configure_args $arg"
		;;
	esac
done

AC_ARG_WITH(repository, [], REPOSITORY="$withval")
AC_SUBST(REPOSITORY)

echo $real_configure_args >configure.args
AC_CONFIG_SUBDIRS(afx libs/gdbmobj libs/bdbobj libs/cgi libs/http11 libs/numlib libs/soxwrap libs/md5 libs/sha1 libs/libhmac libs/random128 libs/rfc822 libs/rfc1035 libs/rfc2045 libs/liblock libs/maildir libs/waitlib libs/threadlib libs/makedat libs/tcpd libs/rootcerts libs/imap libs/maildrop libs/ldapaddressbook libs/gpglib libs/pcp libs/sqwebmail webadmin courier)

AC_OUTPUT(Makefile courier.spec courier.service dbobj.h dbobj.config courier.sysvinit)
