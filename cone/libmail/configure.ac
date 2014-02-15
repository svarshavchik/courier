#
# Copyright 2002-2009, Double Precision Inc.
#
# See COPYING for distribution information.
#

dnl Process this file with autoconf to produce a configure script.
AC_INIT(libmail, 0.10, [courier-cone@lists.sourceforge.net])
>confdefs.h # Kill PACKAGE_MACROS

AC_CONFIG_SRCDIR(mail.C)
AC_CONFIG_AUX_DIR(..)
AC_CONFIG_HEADERS(libmail_config.h)
AM_INIT_AUTOMAKE([no-define])

dnl Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AM_PROG_LIBTOOL

LPATH="$PATH:/usr/local/bin:/usr/sbin:/sbin:/lib"

AC_PATH_PROG(SENDMAIL, sendmail, /usr/bin/sendmail, $LPATH)

AC_DEFINE_UNQUOTED(SENDMAIL, "$SENDMAIL",
			     [ Local sendmail program ])

dnl Checks for libraries.

AC_ARG_WITH(libidn, AC_HELP_STRING([--with-libidn=[DIR]],
		    [Support IDN (needs GNU Libidn)]),
		    libidn=$withval, libidn=yes)

if test "$libidn" != "no"
then
	PKG_CHECK_MODULES(LIBIDN, libidn >= 0.0.0, [libidn=yes], [libidn=no])
        if test "$libidn" != "yes"
	then
		libidn=no
         	AC_MSG_WARN([Libidn not found])
	else
		libidn=yes
		AC_DEFINE(LIBIDN, 1, [Define to 1 if you want Libidn.])
       	fi
fi
AC_MSG_CHECKING([if Libidn should be used])
AC_MSG_RESULT($libidn)

dnl Checks for header files.
AC_CHECK_HEADERS(sys/time.h sys/wait.h unistd.h sys/select.h fcntl.h utime.h termios.h)
AC_HEADER_TIME
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT

dnl Checks for typedefs, structures, and compiler characteristics.

AC_TYPE_SIGNAL
AC_SYS_LARGEFILE

dnl Checks for library functions.

if test "$GXX" = "yes"
then
	CPPFLAGS="-Wall $CPPFLAGS"
fi
CFLAGS="-I../libs -I${srcdir}/../libs $CFLAGS"
CXXFLAGS="-I../libs -I${srcdir}/../libs $CXXFLAGS"

. ../libs/tcpd/couriertls.config

if test "$couriertls" != ""
then
	LIBCOURIERTLS="../libs/tcpd/libcouriertls.la"
fi
AC_SUBST(LIBCOURIERTLS)

CPPFLAGS="-I.. -I${srcdir}/.. -I../libs -I${srcdir}/../libs $cppflags $CPPFLAGS"

changequote(<,>)

echo 'static const char * const mimetypefiles[]={' >mimetypefiles.h

changequote([,])

AC_ARG_ENABLE(mimetypes, [  --enable-mimetypes={dir}   Your mime.types file.],
			 echo "\"$enableval\"," >>mimetypefiles.h
)

for f in /usr/lib /usr/local/lib /usr/lib/pine /usr/local/lib/pine /etc \
         /var/lib/httpd/conf /home/httpd/conf /usr/local/etc/apache \
         /usr/local/apache/conf /var/lib/apache/etc
do
        if test -f $f/mime.types
        then
		echo "\"$f/mime.types\"," >>mimetypefiles.h
        fi
done

echo '0};' >>mimetypefiles.h

AC_ARG_WITH(devel, [  --with-devel                  Install development libraries],
		   devel="$withval", devel=no)

case "$devel" in
y*|Y*)
	INSTINCLUDES="install-includes"
	UNINSTINCLUDES="uninstall-includes"
	;;
esac
AC_SUBST(INSTINCLUDES)
AC_SUBST(UNINSTINCLUDES)

AC_CHECK_FUNCS(utime utimes)

# Debugging:

AC_ARG_ENABLE(debugging, [ --enable-debugging         Maintainer option ],
			 debugging="$enableval", debugging="no")

case "$debugging" in
y*|Y*)
	debugging="1"
	;;
*)
	debugging="0"
	;;
esac

AC_DEFINE_UNQUOTED(LIBMAIL_THROW_DEBUG, $debugging, [ Debugging purposes ])

AC_OUTPUT(Makefile)
