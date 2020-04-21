dnl Process this file with autoconf to produce a configure script.
dnl
dnl
dnl Copyright 1998 - 2007 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_INIT(libuserdb, 0.10, [courier-users@lists.sourceforge.net])

>confdefs.h # Kill PACKAGE_ macros.

LPATH="$PATH:/usr/local/bin"

AC_CONFIG_SRCDIR(userdb.c)
AC_CONFIG_AUX_DIR(..)
AM_INIT_AUTOMAKE([foreign no-define])
AC_CONFIG_HEADERS(config.h)

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL
AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)

if test "$PERL" = "perl"
then
	AC_MSG_ERROR(Perl not found.)
fi

if test "$GCC" = "yes"
then
	CFLAGS="$CFLAGS -Wall"
fi

CFLAGS="$CFLAGS -I.. -I${srcdir}/.. -I../libs -I${srcdir}/../libs"

dnl Checks for libraries.

AC_ARG_WITH(userdb, [ ], userdb="$withval", userdb="/etc/userdb")
AC_SUBST(userdb)
USERDB="`echo $userdb | tr '[a-z]' '[A-Z]'`"
AC_SUBST(USERDB)

AC_ARG_WITH(userdbmandir, [ ], mandir="$withval")

AC_ARG_WITH(userdbtmpdir, [ ], tmpdir="$withval", tmpdir="")

changequote({,})

if test "$tmpdir" = ""
then
	tmpdir=`echo "$userdb" | sed 's/\/[^\/]*$//'`
fi

changequote([,])

AC_SUBST(userdbtmpdir)
AC_SUBST(tmpdir)

AC_ARG_WITH(makedatprog, [ ], makedat="$withval", makedat="$bindir/makedat")
AC_SUBST(makedat)

if test -d ${srcdir}/../libs/md5
then
	AC_DEFINE_UNQUOTED(HAVE_MD5,1,[ Whether libmd5.a is present ])
	MD5LIB=../libs/md5/libmd5.la
else
	MD5LIB=""
fi
AC_SUBST(MD5LIB)

if test -d ${srcdir}/../libs/sha1
then
	SHA1LIB=../libs/sha1/libsha1.la
else
	SHA1LIB=""
fi
AC_SUBST(SHA1LIB)

if test -d ${srcdir}/../libs/libhmac
then
	AC_DEFINE_UNQUOTED(HAVE_HMAC,1, [ Whether libhmac.a is present ])
	HMACLIB=../libs/libhmac/libhmac.la
else
	HMACLIB=""
fi
AC_SUBST(HMACLIB)

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(sys/stat.h sys/time.h unistd.h fcntl.h termios.h crypt.h)
AC_HEADER_TIME

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_PID_T
AC_TYPE_UID_T
AC_TYPE_SIGNAL
AC_SYS_LARGEFILE

dnl Other checks

AC_CHECK_LIB(crypt, crypt, CRYPTLIBS="-lcrypt")
saveLIBS="$LIBS"
LIBS="$CRYPTLIBS $LIBS"
AC_CHECK_FUNC(crypt, AC_DEFINE_UNQUOTED(HAVE_CRYPT,1,
			[ Whether the crypt() function is available ]))
LIBS="$saveLIBS"
AC_CACHE_CHECK([for crypt() prototype],userdb_cv_NEED_CRYPT_PROTOTYPE,

	AC_TRY_COMPILE( [
	       #if HAVE_CRYPT_H
	       #include <crypt.h>
	       #endif
	       #if HAVE_UNISTD_H
	       #include <unistd.h>
	       #endif
	       int crypt(int, int);

		], [], userdb_cv_NEED_CRYPT_PROTOTYPE=1,
			userdb_cv_NEED_CRYPT_PROTOTYPE=0 )

	)

AC_DEFINE_UNQUOTED(NEED_CRYPT_PROTOTYPE, $userdb_cv_NEED_CRYPT_PROTOTYPE,
			 [ Whether crypt() must be explicitly prototyped ])
AC_SUBST(CRYPTLIBS)

AC_CHECK_FUNCS(isatty gettimeofday)

AC_ARG_WITH(random, [ --with-random=/dev/urandom - location of the system random file generator
--without-random            - there is no system random file generator ],
	random="$withval",
	random="y")

case "$random" in
/*)
	;;
y*|1*)
	AC_CACHE_CHECK([for random source],userdb_cv_RANDOM,

		if test -c /dev/urandom
		then
			userdb_cv_RANDOM=/dev/urandom
		else
			if test -c /dev/random
			then
				userdb_cv_RANDOM=/dev/random
			else
				userdb_cv_RANDOM="none"
			fi
		fi
	)
	random="$userdb_cv_RANDOM"
	;;
*)
	random="none"
	;;
esac

if test "$random" != "none"
then
	AC_DEFINE_UNQUOTED(RANDOM, "$random",
				   [ System random number generator ])
fi

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/../libs/docbook)

AC_OUTPUT(Makefile userdb.pl makeuserdb pw2userdb)
