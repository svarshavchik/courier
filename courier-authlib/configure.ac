dnl Process this file with autoconf to produce a configure script.
dnl
dnl
dnl Copyright 1998 - 2021 Double Precision, Inc.  See COPYING for
dnl distribution information.

AC_PREREQ(2.63)
AC_INIT([courier-authlib],[0.71.3],[courier-users@lists.sourceforge.net])

AC_CONFIG_SRCDIR([courierauth.h])

AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([courier_auth_config.h])
>confdefs.h  # Kill PACKAGE_ macros

LPATH="$PATH:/usr/local/bin"

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_SYSCONFTOOL
LT_INIT(dlopen)
LTDL_INIT
AC_SUBST(LTDLINCL)
AC_SUBST(LIBLTDL)
AC_PATH_PROGS(PERL, perl5 perl, perl, $LPATH)
AC_PATH_PROGS(COURIERCONFIG, courier-config, courier-config, $LPATH)

AC_CHECK_HEADER(ltdl.h, [ : ],
			AC_MSG_ERROR([Unable to find ltdl.h. Please install Libtool's ltdl library]))

if test "$PERL" = "perl"
then
	AC_MSG_ERROR(Perl is required)
fi

test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
eval "prefix=$prefix"
eval "exec_prefix=$exec_prefix"
eval "sysconfdir=$sysconfdir"
eval "bindir=$bindir"
eval "sbindir=$sbindir"
eval "localstatedir=$localstatedir"
eval "libexecdir=$libexecdir"
eval "datarootdir=$datarootdir"
eval "datedir=$datadir"

AC_PATH_PROGS(EXPECT, expect, expect, $LPATH)

if test "$EXPECT" = "expect"
then
	AC_MSG_WARN(-----------------------------------------------------)
	AC_MSG_WARN(expect not found - will not be able to change passwds)
	AC_MSG_WARN(in webmail)
	AC_MSG_WARN(-----------------------------------------------------)
	sleep 5
else
	AC_DEFINE_UNQUOTED(HAVE_EXPECT, 1, [ Whether expect(1) is installed ])
fi

AC_PATH_PROGS(PASSWD, passwd, passwd, $LPATH)
AC_SUBST(PASSWD)

if test "$GCC" = "yes"
then
	CFLAGS="$CFLAGS -Wall"
	CXXFLAGS="$CXXFLAGS -Wall"
fi

CFLAGS="$CFLAGS -Ilibs -I${srcdir}/libs"
CXXFLAGS="$CXXFLAGS -Ilibs -I${srcdir}/libs"

AC_MSG_CHECKING(whether -lm is needed for floor)
AC_TRY_LINK_FUNC(floor,
	AC_MSG_RESULT(no),
	LIBM="-lm"
	AC_MSG_RESULT(yes))

AC_ARG_WITH(pkgconfdir,
[  --with-pkgconfdir=d     Install config files in directory ],
     [pkgconfdir="$withval"],
     [pkgconfdir="$sysconfdir/authlib"])

saveLIBS="$LIBS"
NETLIBS=""
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

AX_COURIER_UNICODE_VERSION(2.1)

dnl #########################################################################

dnl Prepare authuserdb module if userdb library is available

dnl #########################################################################


AC_ARG_WITH(db, [  --with-db=gdbm          Use the GDBM library.
  --with-db=db            Use the libdb.a library.],
	db="$withval", needs_withdb=1)

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
	AC_CHECK_LIB(gdbm, gdbm_open,
	 [ LIBGDBM=-lgdbm ; LIBS="-lgdbm $LIBS" ])
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
	LIBDB=""
	USE_GDBM=1
	if test "$needs_withdb" = 1
	then
		ac_configure_args="$ac_configure_args --with-db=gdbm"
	fi
	dblibrary=libs/gdbmobj/libgdbmobj.la
else
	LIBGDBM=""
	USE_DB=1
	if test "$needs_withdb" = 1
	then
		ac_configure_args="$ac_configure_args --with-db=db"
	fi
	dblibrary=libs/bdbobj/libbdbobj.la
fi

AC_SUBST(USE_GDBM)
AC_SUBST(USE_DB)

AC_ARG_WITH(authuserdb,
[ --without-authuserdb               Do not include the authuserdb module ],
	doauthuserdb="$withval",
	doauthuserdb="no"
	if test -d ${srcdir}/userdb
	then
		doauthuserdb="yes"
	fi)

if test "$doauthuserdb" = "no"
then
	AUTHUSERDB=""
else
	AUTHUSERDB="authuserdb"
	LIBAUTHUSERDB="libauthuserdb.la"

fi

AC_ARG_WITH(makedatprog, [ ], [ : ],
			 [
			 ac_configure_args="$ac_configure_args --with-makedatprog=$libexecdir/courier-authlib/makedatprog"
			 ])

cat >dbobj.config <<EOF
LIBDB=$LIBDB
LIBGDBM=$LIBGDBM
dblibrary=$dblibrary
EOF

AC_SUBST(USE_GDBM)
AC_SUBST(USE_DB)

AC_SUBST(dblibrary)
AC_SUBST(LIBGDBM)
AC_SUBST(LIBDB)
AC_SUBST(LIBAUTHUSERDB)

userdb="$pkgconfdir/userdb"
AC_ARG_WITH(userdb, [], [ userdb="$withval" ], [ac_configure_args="$ac_configure_args '--with-userdb=$userdb'"])

AC_DEFINE_UNQUOTED(USERDB,"$userdb", [ Location of the userdb database ])
AC_SUBST(userdb)

dnl Checks for header files.
dnl
dnl Because autoconf sets macros for C preprocessor where
dnl AC_CHECK_HEADERS appears first, the first AC_CHECK_HEADERS
dnl must not place in conditional level but top level.
dnl This is a dummy AC_CHECK_HEADERS for it.
dnl
AC_CHECK_HEADERS(stdio.h)

dnl #########################################################################

dnl Prepare authpam module if libpam is available.

dnl #########################################################################

AC_ARG_WITH(authpam,
[ --without-authpam                  Do not include the authpam module ],
	doauthpam="$withval")

AC_CHECK_HEADERS(security/pam_appl.h Pam/pam_appl.h)
if test "$doauthpam" = "no"
then
	LIBDL=""
else
	saveLIBS="$LIBS"
	LIBDL=""
	AC_CHECK_LIB(dl, dlopen, [ LIBDL="-ldl" ])
	LIBS="$saveLIBS"

	AC_CHECK_LIB(pam, pam_start,
		[ HAVE_PAM=1
		LIBS="-lpam $LIBDL $LIBS"
		AC_CHECK_FUNCS(pam_setcred)],

		HAVE_PAM=0,
		$LIBDL)
	LIBS="$saveLIBS"
fi
AC_SUBST(LIBDL)

LIBAUTHPAM=""
if test "$HAVE_PAM" = 1
then
	LIBAUTHPAM=libauthpam.la
fi

AC_ARG_WITH(authpam-libraries,
[ --with-authpam-libraries="libs"    Link 'libs' with authpam, this may be
                                    required for FreeBSD 3.3],
	authpamcrypt="$withval")

AUTHPAMCRYPT="$authpamcrypt"
AC_SUBST(AUTHPAMCRYPT)
AC_SUBST(LIBAUTHPAM)

dnl #########################################################################

dnl Prepare authldap module if ldap functions are available.

dnl #########################################################################

AC_ARG_WITH(authldap,
[ --without-authldap                 Do not include the authldap module ],
	doauthldap="$withval",
	doauthldap="yes")

AC_CHECK_HEADERS(lber.h)
AC_CHECK_HEADERS(ldap.h)
AC_CHECK_HEADERS(netinet/in.h)

if test "$doauthldap" = "no"
then
	HAVE_LDAP=0
else
	saveLIBS="$LIBS"
	LIBS="$NETLIBS $LIBS"
	LDAPLIBS=""

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
#include <sys/types.h>
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

	AC_CHECK_LIB(lber, ber_scanf,
		[ LDAPLIBS="-llber $LDAPLIBS" LIBS="-llber $LIBS" ])
	AC_CHECK_LIB(ldap, ldap_open,
		[ LDAPLIBS="-lldap $LDAPLIBS" ; LIBS="-lldap $LIBS" ])
	AC_CHECK_FUNC(ldap_search_st, HAVE_LDAP=1, HAVE_LDAP=0)
	AC_CHECK_FUNC(ldap_start_tls_s, HAVE_LDAP_TLS=1, HAVE_LDAP_TLS=0)
	AC_CHECK_FUNC(ldap_result2error, HAVE_LDAP_RESULT2ERROR=1, HAVE_LDAP_RESULT2ERROR=0)
	AC_CHECK_FUNC(ldap_parse_result, HAVE_LDAP_PARSE_RESULT=1, HAVE_LDAP_PARSE_RESULT=0)
	LIBS="$saveLIBS"
fi

AC_ARG_WITH(authldaprc,
[ --with-authldaprc=filename         Expect to find authldaprc here ],
	authldaprc="$withval",
	authldaprc="$pkgconfdir/authldaprc")
AC_SUBST(authldaprc)

LIBAUTHLDAP=""
if test "$HAVE_LDAP" = 1
then
	LIBAUTHLDAP="libauthldap.la"
else
	authldaprc=""
fi
AC_SUBST(LDAPLIBS)
AC_SUBST(LIBAUTHLDAP)

AM_CONDITIONAL(HAVE_LDAP, test "$HAVE_LDAP" = 1)
AC_DEFINE_UNQUOTED(HAVE_LDAP_TLS,$HAVE_LDAP_TLS,
	[ Whether we have ldap_start_tls_s ])
AC_DEFINE_UNQUOTED(HAVE_LDAP_RESULT2ERROR, $HAVE_LDAP_RESULT2ERROR,
	[ Whether we have ldap_result2error() function ])
AC_DEFINE_UNQUOTED(HAVE_LDAP_PARSE_RESULT, $HAVE_LDAP_PARSE_RESULT,
					   [ Whether we have ldap_parse_result() function])
dnl #########################################################################

dnl Prepare authpwd module

dnl #########################################################################

AC_ARG_WITH(authpwd,
[ --without-authpwd                  Do not include the authpwd module ],
	doauthpwd="$withval",
	doauthpwd="yes"
	test "$HAVE_PAM" = 1 && doauthpwd="no"
	test "$HAVE_LDAP" = 1 && doauthpwd="no"
	)

HAVE_PWD=1
if test "$doauthpwd" = "no"
then
	HAVE_PWD=0
fi
AC_CHECK_FUNCS(endpwent)

dnl #########################################################################

dnl Prepare authshadow module if shadow functions are available.

dnl #########################################################################

AC_ARG_WITH(authshadow,
[ --without-authshadow               Do not include the authshadow module ],
	doauthshadow="$withval",
	doauthshadow="yes"
	test "$HAVE_PAM" = 1 && doauthshadow="no"
	test "$HAVE_LDAP" = 1 && doauthshadow="no")

AC_CHECK_HEADERS(shadow.h)
saveLIBS="$LIBS"
SHADOWLIBS=""
AC_CHECK_LIB(shadow, getspent,
	[ SHADOWLIBS="-lshadow" ; LIBS="-lshadow $LIBS" ])
AC_CHECK_FUNCS(endspent getspent)
LIBS="$saveLIBS"

if test "$doauthshadow" = "no"
then
	HAVE_SHADOW=0
else

	saveLIBS="$LIBS"
	AC_CHECK_LIB(shadow, getspent)
	AC_CHECK_FUNC(getspent, HAVE_SHADOW=1, HAVE_SHADOW=0)
	LIBS="$saveLIBS"
fi

LIBAUTHSHADOW=""
if test "$HAVE_SHADOW" = 1
then
	LIBAUTHSHADOW="libauthshadow.la"
fi
AC_SUBST(SHADOWLIBS)
AC_SUBST(LIBAUTHSHADOW)

LIBAUTHPWD=""
if test "$HAVE_PWD" = 1
then
	LIBAUTHPWD="libauthpwd.la"
fi
AC_SUBST(LIBAUTHPWD)

dnl #########################################################################

dnl Prepare authpgsql module

dnl #########################################################################

AC_ARG_WITH(authpgsqlrc,
[ --with-authpgsqlrc=filename        Expect to find authpgsql here ],
	authpgsqlrc="$withval",
	authpgsqlrc="$pkgconfdir/authpgsqlrc")
AC_SUBST(authpgsqlrc)

AC_ARG_WITH(authpgsql,
[ --without-authpgsql               Do not include the authpgsql module ],
        doauthpgsql="$withval")

AC_PATH_PROGS(PG_CONFIG, pg_config, pg_config, $LPATH)

PGSQL_LIBS="-lpq"
AC_ARG_WITH(pgsql-libs,
[ --with-pgsql-libs=DIR		Look for pgsql libs in this dir ],
	PGSQL_LIBS="-L$withval $PGSQL_LIBS",
	if test -x $PG_CONFIG
	then
		PGSQL_LIBS="-L`$PG_CONFIG --libdir` $PGSQL_LIBS"
	fi
)
AC_ARG_WITH(pgsql-includes,
[ --with-pgsql-includes=DIR	Look for pgsql includes in this dir ],
	PGSQL_CFLAGS="-I$withval",
	if test -x $PG_CONFIG
	then
		PGSQL_CFLAGS="-I`$PG_CONFIG --includedir`"
	fi
)

if test "$doauthpgsql" = ""
then
	LIBS="$PGSQL_LIBS $LIBS"
	AC_CHECK_FUNC(PQsetdbLogin,
		doauthpgsql="yes"
		)
	LIBS="$saveLIBS"
fi

if test "$doauthpgsql" != "yes"
then
	LIBAUTHPGSQL=""
	HAVE_AUTHPGSQL=0
else
	saveLIBS="$LIBS"
	LIBS="$PGSQL_LIBS $LIBS"
	AC_CHECK_FUNC(PQsetdbLogin,
		AUTHPGSQL="authpgsql"
		HAVE_AUTHPGSQL=1,
		AC_MSG_ERROR([--with-authpgsql specified but no libpq.so]))
	LIBS="$saveLIBS"
	HAVE_AUTHPGSQL=1
	CFLAGS="$PGSQL_CFLAGS $CFLAGS"
	CXXFLAGS="$PGSQL_CFLAGS $CXXFLAGS"
	LIBAUTHPGSQL="libauthpgsql.la"
fi

AC_SUBST(PGSQL_LIBS)
AC_SUBST(LIBAUTHPGSQL)
AM_CONDITIONAL(HAVE_AUTHPGSQL, test "$HAVE_AUTHPGSQL" = 1)

dnl #########################################################################

dnl Prepare authmysql module

dnl #########################################################################

AC_ARG_WITH(authmysqlrc,
[ --with-authmysqlrc=filename        Expect to find authmysql here ],
	authmysqlrc="$withval",
	authmysqlrc="$pkgconfdir/authmysqlrc")
AC_SUBST(authmysqlrc)

AC_ARG_WITH(authmysql,
[ --without-authmysql               Do not include the authmysql module ],
        doauthmysql="$withval")
AC_ARG_WITH(mysql-libs,
[ --with-mysql-libs=DIR		Look for mysql libs in this dir ],
	MYSQL_LIBS="-L$withval -lmysqlclient"
)
AC_ARG_WITH(mysql-includes,
[ --with-mysql-includes=DIR	Look for mysql includes in this dir ],
	MYSQL_CFLAGS="-I$withval"
)

AC_PATH_PROGS(MYSQL_CONFIG, mysql_config, mysql_config, $LPATH)

if test -x "$MYSQL_CONFIG"
then
	MYSQL_CFLAGS="`$MYSQL_CONFIG --cflags`"
	MYSQL_LIBS="`$MYSQL_CONFIG --libs`"

	eval "MYSQL_CFLAGS=\"\`echo $MYSQL_CFLAGS\`\""
	eval "MYSQL_LIBS=\"\`echo $MYSQL_LIBS\`\""
fi

if test "$doauthmysql" = ""
then
	LIBS="$MYSQL_LIBS $LIBS"
	AC_CHECK_FUNC(mysql_connect,
		doauthmysql="yes"
		)
	AC_CHECK_FUNC(mysql_real_connect,
		doauthmysql="yes"
		)
	LIBS="$saveLIBS"
fi

if test "$doauthmysql" != "yes"
then
	LIBAUTHMYSQL=""
	HAVE_AUTHMYSQL=0
else
	saveLIBS="$LIBS"
	LIBS="$MYSQL_LIBS $LIBS"
	AC_CHECK_FUNC(mysql_connect,
		LIBAUTHMYSQL="libauthmysql.la"
		HAVE_AUTHMYSQL=1,
		[
			AC_CHECK_FUNC(mysql_real_connect,
				LIBAUTHMYSQL="libauthmysql.la"
				HAVE_AUTHMYSQL=1,

					AC_MSG_ERROR([--with-authmysql specified but no mysqlclient.so])
					)
		]
)
	LIBS="$saveLIBS"
	HAVE_AUTHMYSQL=1
	CFLAGS="$MYSQL_CFLAGS $CFLAGS"
	CXXFLAGS="$MYSQL_CFLAGS $CXXFLAGS"
fi
AC_SUBST(LIBAUTHMYSQL)
AC_SUBST(MYSQL_LIBS)
AM_CONDITIONAL(HAVE_AUTHMYSQL, test "$HAVE_AUTHMYSQL" = 1)

dnl #########################################################################

dnl Prepare authsqlite module

dnl #########################################################################

AC_ARG_WITH(authsqliterc,
[ --with-authsqliterc=filename        Expect to find authmysql here ],
	authsqliterc="$withval",
	authsqliterc="$pkgconfdir/authsqliterc")
AC_SUBST(authsqliterc)

AC_ARG_WITH(authsqlite,
[ --without-authsqlite               Do not include the authsqlite module ],
        doauthsqlite="$withval")

AC_ARG_WITH(sqlite-libs,
[ --with-sqlite-libs=DIR		Look for sqlite libs in this dir ],

	SQLITE_LIBS="-lsqlite3"
)
AC_ARG_WITH(sqlite-includes,
[ --with-sqlite-includes=DIR	Look for sqlite includes in this dir ],
	SQLITE_CFLAGS="-I$withval"
)

if test "$doauthsqlite" = ""
then
	SQLITE_CFLAGS="`pkg-config --cflags sqlite3 2>/dev/null`"
	SQLITE_LIBS="`pkg-config --libs sqlite3 2>/dev/null`"
        saveLIBS="$LIBS"
	LIBS="$SQLITE_LIBS $LIBS"
	AC_CHECK_FUNC(sqlite3_version,
		doauthsqlite="yes"
		)
	LIBS="$saveLIBS"
fi

if test "$doauthsqlite" != "yes"
then
	SQLITE_LIBS=""
	SQLITE_CFLAGS=""
	HAVE_AUTHSQLITE=0
	LIBAUTHSQLITE=""
else
	saveLIBS="$LIBS"
	LIBS="$SQLITE_LIBS $LIBS"
	AC_CHECK_FUNC(sqlite3_version,
		[ : ],
		[ AC_MSG_ERROR([Cannot link with $SQLITE_LIBS]) ]
		)
	LIBS="$saveLIBS"
	HAVE_AUTHSQLITE=1
	CFLAGS="$SQLITE_CFLAGS $CFLAGS"
	CXXFLAGS="$SQLITE_CXXFLAGS $CFLAGS"
	LIBAUTHSQLITE="libauthsqlite.la"

fi
AC_SUBST(SQLITE_LIBS)
AC_SUBST(LIBAUTHSQLITE)
AM_CONDITIONAL(HAVE_AUTHSQLITE, test "$HAVE_AUTHSQLITE" = 1)

dnl #########################################################################

dnl Prepare authcustom stub module.

dnl #########################################################################


AC_ARG_WITH(authcustom,
[ --without-authcustom               Do not include the authcustom module ],
	doauthcustom="$withval",
	doauthcustom=yes)

LIBAUTHCUSTOM=""

if test "$doauthcustom" = "yes"
then
	LIBAUTHCUSTOM="libauthcustom.la"
fi

AM_CONDITIONAL(HAVE_CUSTOM, test "$AUTHCUSTOM" != "")
AC_SUBST(LIBAUTHCUSTOM)


dnl #########################################################################

dnl Prepare authpipe module.

dnl #########################################################################


AC_ARG_WITH(pipeprog,
[ --with-pipeprog=filename        Expect to find the pipe-prog here ],
	authProg="$withval",
	authProg="$pkgconfdir/authProg")
AC_SUBST(authProg)

AC_ARG_WITH(authpipe,
[ --without-authpipe               Do not include the authpipe module ],
	doauthpipe="$withval",
	doauthpipe=yes)

LIBAUTHPIPE=""

if test "$doauthpipe" = "yes"
then
	LIBAUTHPIPE="libauthpipe.la"
fi

AM_CONDITIONAL(HAVE_PIPE, test "$AUTHPIPE" != "")
AC_SUBST(LIBAUTHPIPE)



dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(sys/stat.h sys/time.h sys/wait.h sys/select.h unistd.h fcntl.h crypt.h termios.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_UID_T
dnl Other checks
AC_CHECK_FUNCS(setsid setlogin)
AC_CHECK_LIB(crypt, crypt, CRYPTLIBS="-lcrypt")
saveLIBS="$LIBS"
LIBS="$CRYPTLIBS $LIBS"
AC_CHECK_FUNC(crypt, AC_DEFINE_UNQUOTED(HAVE_CRYPT, 1,
		     [ Whether we have the crypt() function ]))
AC_CHECK_FUNC(bcrypt, AC_DEFINE_UNQUOTED(HAVE_BCRYPT, 1,
		     [ Whether we have the bcrypt() function ]))
LIBS="$saveLIBS"
AC_CACHE_CHECK([for crypt() prototype],userdb_cv_NEED_CRYPT_PROTOTYPE,

	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
	       #if HAVE_CRYPT_H
               #include <crypt.h>
               #endif
	       #if HAVE_UNISTD_H
	       #include <unistd.h>
	       #endif
	       int crypt(int, int);

		]], [[]])],[userdb_cv_NEED_CRYPT_PROTOTYPE=1],[userdb_cv_NEED_CRYPT_PROTOTYPE=0 ])

	)

AC_DEFINE_UNQUOTED(NEED_CRYPT_PROTOTYPE, $userdb_cv_NEED_CRYPT_PROTOTYPE,
			 [ Whether we must a prototype for crypt()] )
AC_SUBST(CRYPTLIBS)

dnl #########################################################################

dnl Prepare the authdaemon module.

dnl #########################################################################

AC_ARG_WITH(authdaemonrc,
[ --with-authdaemonrc=filename       Expect to find authdaemonrc here ],
	authdaemonrc="$withval",
	authdaemonrc="$pkgconfdir/authdaemonrc")
AC_SUBST(authdaemonrc)

AC_ARG_WITH(authdaemonvar,
[ --with-authdaemonvar=directory     Directory where authdaemon.pid and
                                    the listening socket is created],
	authdaemonvar="$withval",
	authdaemonvar="$localstatedir/spool/authdaemon")

AC_SUBST(authdaemonvar)

AC_SUBST(LIBM)

rm -f authdaemonrc.h authldaprc.h authmysqlrc.h authpgsqlrc.h vpopmail_config.h

dnl
dnl Need to settle on our uid/gids here
dnl

result=""

if test -x "$COURIERCONFIG"
then
	$COURIERCONFIG >conftest.out || exit 1
	sed -n '/^mail/p' <conftest.out >conftest2.out || exit 1
	. ./conftest2.out
	rm -f conftest.out conftest2.out

	cmailuser="$mailuser"
	cmailgroup="$mailgroup"
	result=" (from courier-config)"
fi

if test -x "$bindir/courierauthconfig"
then
	$bindir/courierauthconfig --configfiles >conftest.out || exit 1
	sed -n '/^mail/p' <conftest.out >conftest2.out || exit 1
	. ./conftest2.out
	rm -f conftest.out conftest2.out

	cmailuser="$mailuser"
	cmailgroup="$mailgroup"
	result=" (from previous courierauthconfig)"
fi

changequote()

LB='['
RB=']'

changequote([,])

AC_MSG_CHECKING(for mail userid)

AC_ARG_WITH(mailuser,[  --with-mailuser=user    Specify mail user name (defaults to
                          courier, daemon, admin, bin, or root)],
	mailuser="$withval",

	if test "$cmailuser" = ""
	then
		AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <pwd.h>
#include <stdlib.h>

static const char *ids $LB $RB={"courier","daemon","adm","bin","root", 0};

int main()
{
int i;
FILE *f;
char	*p;

	for (i=0; ids $LB i $RB; i++)
		if (getpwnam(ids $LB i $RB))	break;

	f=fopen("conftest.out", "w");
	if (f && ids $LB i $RB)
	{
		fprintf(f, "%s\n", ids $LB i $RB);
		fclose(f);
		exit(0);
	}
	fclose(f);
	exit (1);
	return (1);
}
	]])],[mailuser=`cat conftest.out`],[AC_MSG_ERROR(Cannot determine mail user, use --with-mailuser.)],[AC_MSG_ERROR(Must use --with-mailuser when cross compiling.)])
	else
		mailuser="$cmailuser"
	fi
	ac_configure_args="$ac_configure_args --with-mailuser=$mailuser")

AC_MSG_RESULT([$mailuser $result])

AC_MSG_CHECKING(for mail group id)

AC_ARG_WITH(mailgroup, [  --with-mailgroup=group  Specify mail group name (defaults to courier,
                          daemon, sys, adm, or root)],
	mailgroup="$withval",

	if test "$cmailgroup" = ""
	then
		AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <grp.h>
#include <stdlib.h>

static const char *ids $LB $RB={"courier", "daemon","sys","adm", "root", 0};

int main()
{
int i;
FILE *f;

	for (i=0; ids $LB i $RB; i++)
		if (getgrnam(ids $LB i $RB))	break;

	f=fopen("conftest.out", "w");
	if (f && ids $LB i $RB)
	{
		fprintf(f, "%s\n", ids $LB i $RB);
		fclose(f);
		exit(0);
	}
	fclose(f);
	exit (1);
	return (1);
}
	]])],[mailgroup=`cat conftest.out`],[AC_MSG_ERROR(Cannot determine mail group, use --with-mailgroup.)],[AC_MSG_ERROR(Must use --with-mailgroup when cross compiling.)])
	else
		mailgroup="$cmailgroup"
	fi
	ac_configure_args="$ac_configure_args --with-mailgroup=$mailgroup"
	)
AC_MSG_RESULT([$mailgroup $result])
AC_SUBST(mailuser)
AC_SUBST(mailgroup)
rm -f conftest.out

AC_ARG_WITH(stdheaderdir,
[  --without-stdheaderdir  Header files will be installed into a directory
                          not in the compiler's default search path.],
	[
	if test "$withval" = "no"
	then
		AC_DEFINE_UNQUOTED(HAVE_NOSTDHEADERDIR, 1,
		[ Whether header installation directory is nontstandard ])
	fi
	])

AC_CACHE_CHECK([for socklen_t],
	authlib_cv_hassocklen_t,

AC_COMPILE_IFELSE([
AC_LANG_SOURCE( [
#include <sys/types.h>
#include <sys/socket.h>

socklen_t sl_t;
],[
	accept(0, 0, &sl_t);
])],
	authlib_cv_hassocklen_t=yes,
	authlib_cv_hassocklen_t=no)
)

socklen_t="int"

if test $authlib_cv_hassocklen_t = yes
then
	:
else
	AC_DEFINE_UNQUOTED(socklen_t, int, [ Default definition for socklen_t ])
fi

AC_ARG_WITH(repository, [], REPOSITORY="$withval")
AC_SUBST(REPOSITORY)

AM_CONDITIONAL(HAVE_SGML, test -d ${srcdir}/libs/docbook)

save_LDFLAGS="$LDFLAGS"
LDFLAGS="-Wl,--enable-new-dtags $LDFLAGS"

AC_CACHE_CHECK([--enable-new-dtags option],authlib_cv_enable_new_dtags,


	AC_LINK_IFELSE([AC_LANG_PROGRAM([[
		]], [[return 0; ]])],[authlib_cv_enable_new_dtags=yes],[authlib_cv_enable_new_dtags=no ])

)

if test "$authlib_cv_enable_new_dtags" = "yes"
then
   courierauth_ldflags="-Wl,--enable-new-dtags $courierauth_ldflags"
fi

AC_SUBST(courierauth_ldflags)

VERSION_INFO=0:0:0
AC_SUBST(VERSION_INFO)

# This gets built by the make rule, and is not in the git repo

for f in authdaemonrc.in
do
    if test ! -f "$srcdir/$f"
    then
	cp -p "$srcdir/$f.git" "$srcdir/$f"
	touch "$srcdir/$f.git"
    fi
done

AC_CONFIG_SUBDIRS(libs/bdbobj libs/gdbmobj libs/md5 libs/sha1 libs/libhmac libs/numlib libs/makedat userdb libs/rfc822 libs/random128 libs/liblock liblog)

AC_CONFIG_FILES(Makefile authdaemond authdaemonrc authsystem.passwd README.authdebug.html dbobj.config dbobj.h courier-authlib.spec courier-authlib.sysvinit courier-authlib.service userdb-test-cram-md5.pl)
AC_OUTPUT
