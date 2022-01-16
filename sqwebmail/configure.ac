dnl Process this file with autoconf to produce a configure script.
dnl
dnl
dnl Copyright 1998 - 2021 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_PREREQ(2.59)
AC_INIT(sqwebmail, 6.0.7, [courier-sqwebmail@lists.sourceforge.net])
AC_CONFIG_SRCDIR(dbobj.config.in)
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE
>confdefs.h # Kill PACKAGE_ macros

LPATH="$PATH:/usr/local/bin"

AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_PROG_SYSCONFTOOL
AC_PROG_CC
AC_PROG_CC_C99
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL

AC_PATH_PROGS(SETENV, env)

dnl Checks for programs.
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
$COURIERAUTHCONFIG --configfiles >conftest || exit 1
sed 's/^/auth/' <conftest >conftest2
. ./conftest2
if test ! -x $authsbindir/courierlogger$EXEEXT
then
	AC_MSG_ERROR(authlib configuration error - $authsbindir/courierlogger$EXEEXT not found)
fi
courierlogger=$authsbindir/courierlogger$EXEEXT
AC_SUBST(courierlogger)

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


AC_SUBST(mailuser)
AC_SUBST(mailgroup)

if test `$COURIERAUTHCONFIG --version=0.70.0.20200518` = "no"
then
	AC_MSG_ERROR(Courier authentication library version 0.71 is required.)
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
	USE_GDBM=1
	if test "$needs_withdb" = 1
	then
		ac_configure_args="$ac_configure_args --with-db=gdbm"
	fi
	dblibrary=libs/gdbmobj/libgdbmobj.la
	LIBDB=""
else
	USE_DB=1
	if test "$needs_withdb" = 1
	then
		ac_configure_args="$ac_configure_args --with-db=db"
	fi
	dblibrary=libs/bdbobj/libbdbobj.la
	LIBGDBM=""
fi

AC_SUBST(USE_GDBM)
AC_SUBST(USE_DB)
AC_SUBST(dblibrary)

AX_COURIER_UNICODE_VERSION(2.1)

dnl Checks for header files.

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

dnl Checks for library functions.

dnl Other


dnl ***********************************************************************

dnl Find my cgi-bin directory

dnl ***********************************************************************

cgibindir=""
imagedir=""

for f in /var/lib/httpd /var/www /home/httpd /usr/local/apache /usr/local/www /var/lib/apache/share
do
	if test -d $f/cgi-bin
	then
		cgibindir=$f/cgi-bin
	fi

	if test -d $f/htdocs
	then
		imagedir="$f/htdocs/webmail"
	fi
	if test -d $f/html
	then
		imagedir="$f/html/webmail"
	fi
	# FreeBSD Apache port
	if test -d $f/data
	then
		imagedir="$f/data/webmail"
	fi
done

AC_ARG_ENABLE(cgibindir, [  --enable-cgibindir={dir}   HTTP server's cgi-bin directory.],
	cgibindir="$enableval",
	[
		if test "$cgibindir" = ""
		then
			AC_MSG_ERROR([Unable to find your cgi-bin directory, specify --enable-cgibindir.])
		fi
		if test "$imagedir" != ""
		then
			if test "$imageurl" = ""
			then
				imageurl="/webmail"
			fi
		fi
	] )

AC_SUBST(cgibindir)

AC_ARG_ENABLE(imagedir, [  --enable-imagedir={dir}    HTTP server's document directory for images.],
	imagedir="$enableval",
		[
		if test "$imagedir" = ""
		then
			AC_MSG_ERROR([Unable to find your htdocs or html directory, specify --enable-imagedir.])
		fi
		]
)
AC_SUBST(imagedir)

AC_MSG_CHECKING(where to install sqwebmail cgi binary)
AC_MSG_RESULT($cgibindir)
AC_MSG_CHECKING(where to install images)
AC_MSG_RESULT($imagedir)

dnl for cgi subdir

AC_ARG_WITH(formdata, [], , ac_configure_args="$ac_configure_args --with-formdata")

AC_PATH_PROGS(GPG, gpg gpg2, /usr/bin/gpg, $LPATH)

AC_ARG_WITH(plenty-of-beer, [
  --enable-imageurl={url}   URL to the document directory for images.
  --without-fcgi        Do not compile fastcgi support
  --with-fcgi           Compile with fastcgi support
  --without-ispell      Do not compile ispell support
  --with-ispell=prog    Pathname to ispell (if not in default path)
  --enable-autorenamesent  Enable Autorename Sent folder periodically (default)
  --disable-autorenamesent Disable Autorename Sent folder periodically
  --enable-https          Generate https:// URLs for all accesses
  --enable-https=login    Use https:// URLs for password transmission only
  --enable-hardtimeout=7200  Hard session timeout (2hrs default)
  --enable-softtimeout=1200  Inactivity timeout (20 min default)
  --enable-autopurge=7       Automatically purge deleted messages (days).
  --enable-maxpurge=90       Maximum value for autopurge (days).
  --prefix={dir}             Install sqwebmail HTML data in {dir}, instead of
                             PREFIX/share/sqwebmail (which is usually
                             /usr/lib/sqwebmail)
  --with-defaultlang={lang}  Default language to use (only en, so far)
  --enable-mimetypes={dir}   Your mime.types file.
  --enable-bannerprog=prog   Program which will generate a banner.
  --with-maxargsize=nbytes       Limit maximum size of CGI args
  --with-maxformargsize=nbytes   Maximum size of multipart/formdata uploads
  --with-cachedir            Cache logins
  --with-cachedir=dir        Use dir instead of /var/run/sqwebmail or /var/cache/sqwebmail
  --with-cacheowner=user     Owner of the cache directory (default: bin)
  --without-gzip             Do not use gzip to compress messages])

AC_ARG_ENABLE(hostname, [],
	AC_MSG_ERROR(--enable-hostname option is obsolete, see INSTALL))

if test "x$prefix" = xNONE
then
	prefix=/usr/lib/sqwebmail
	ac_configure_args="$ac_configure_args --prefix=$prefix"
fi

test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
eval "prefix=$prefix"
eval "libexecdir=$libexecdir"
eval "localstatedir=$localstatedir"

# For rfc2045

AC_ARG_WITH(package, [], ,
	ac_configure_args="$ac_configure_args --with-package=sqwebmail --with-version=$VERSION")

AC_ARG_WITH(socks, [], ,
	ac_configure_args="$ac_configure_args --without-socks")

#
# autoconf bug
#

ac_given_INSTALL="$INSTALL"

pkglibPROGRAMS_exec='$(pkglibPROGRAMS)'
AC_SUBST(pkglibPROGRAMS_exec)

AM_CONDITIONAL(HAVE_HTMLPAGES, test -d $srcdir/html)

AC_ARG_WITH(repository, [], REPOSITORY="$withval")
AC_SUBST(REPOSITORY)

AC_CONFIG_SUBDIRS(libs/numlib libs/bdbobj libs/gdbmobj libs/md5 libs/sha1
	libs/random128 libs/libhmac libs/liblock
	libs/rfc822 libs/rfc2045 libs/maildir libs/soxwrap
	libs/http11 libs/cgi libs/ldapaddressbook
	libs/gpglib libs/pcp libs/sqwebmail)
AC_OUTPUT(Makefile dbobj.h dbobj.config sqwebmail.spec sqwebmail.lpspec sqwebmaild.rc)
