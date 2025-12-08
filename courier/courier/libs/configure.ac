dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright 1998 - 2008 S. Varshavchik.  See COPYING for
dnl distribution information.

AC_INIT([libcourier],[0.11],[courier-users@lists.sourceforge.net])

>confdefs.h  # Kill PACKAGE_ macros

AM_INIT_AUTOMAKE([foreign no-define])
AC_CONFIG_SRCDIR(aliases.h)
>confdefs.h # Kill PACKAGE_ macros.

AC_CONFIG_HEADERS(courier_lib_config.h)

dnl Checks for programs.

AC_PROG_MAKE_SET
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_17
AC_PROG_INSTALL
AC_PROG_LN_S
LT_INIT

if test x$GCC = xyes
then
	CFLAGS="-Wall $CFLAGS"
fi
if test x$GXX = xyes
then
	CXXFLAGS="-Wall $CXXFLAGS"
fi

CFLAGS="-I.. -I$srcdir/.. -I../.. -I$srcdir/../.. -I$srcdir/../../libs -I../../libs $CFLAGS"
CXXFLAGS="-I.. -I$srcdir/.. -I../.. -I$srcdir/../.. -I$srcdir/../../libs -I../../libs $CXXFLAGS"

AC_PATH_PROG(VALGRIND, valgrind, none)

if test "$VALGRIND" = "none"
then
    VALGRIND=""
else
    VALGRIND="$VALGRIND --tool=memcheck --leak-check=yes --error-exitcode=1 --child-silent-after-fork=yes"
fi

dnl Checks for libraries.
PKG_CHECK_MODULES(LIBIDN, libidn2 >= 0.0.0, [libidn=yes], [libidn=no])

dnl Checks for header files.

AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS_ONCE([sys/time.h])

AC_CHECK_HEADERS(strings.h fcntl.h limits.h unistd.h syslog.h sys/types.h sys/stat.h sys/time.h sys/ioctl.h sys/statfs.h sys/param.h sys/mount.h sys/select.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_TYPE_SIZE_T
AC_SYS_LARGEFILE

AC_CACHE_CHECK([for caddr_t],courier_cv_CADDR_T_DEFINED,

	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
caddr_t p;
]], [[
]])],[courier_cv_CADDR_T_DEFINED=yes],[courier_cv_CADDR_T_DEFINED=no])
)

if test "$courier_cv_CADDR_T_DEFINED" = "no"
then
	AC_DEFINE_UNQUOTED(caddr_t,my_caddr_t,
[ Whether caddr_t should be defined ])
fi

dnl Checks for library functions.
AC_CHECK_FUNCS(gethostname strerror strcasecmp strncasecmp setgroups initgroups sigblock statvfs statfs gettimeofday)

dnl Linux has sighold, but it's not prototyped by default.

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include	<signal.h>

extern int sighold(char *);
]], [[
int i=0;
]])],[],[AC_DEFINE_UNQUOTED(HAVE_SIGHOLD,1, Whether sighold() is available )])


AC_COMPILE_IFELSE([AC_LANG_PROGRAM([

#include <stdio.h>

int main()
{
   setvbuf(stderr, NULL, _IOLBF, BUFSIZ);
}
])],
	AC_DEFINE_UNQUOTED(HAVE_SETVBUF_IOLBF,1,[Whether setvbuf(..._IOLBF) works])
)

AC_CACHE_CHECK([for missing gethostname prototype],courier_cv_SYS_GETHOSTNAME,

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if HAVE_UNISTD_H
#include	<unistd.h>
#endif

#ifdef __cplusplus
extern "C"
#endif
int gethostname(int,int);
]], [[
]])],[courier_cv_SYS_GETHOSTNAME=yes],[courier_cv_SYS_GETHOSTNAME=no
])

)

if test $courier_cv_SYS_GETHOSTNAME = "no"
then
	AC_DEFINE_UNQUOTED(HAS_GETHOSTNAME,1,
	[ Whether gethostname() needs to be prototyped ])
fi

AC_ARG_WITH(qdircount,
[--with-qdircount=n        Queue subdirectory count], qdircount="$withval",
	qdircount=100)
AC_DEFINE_UNQUOTED(QDIRCOUNT,$qdircount, [ Queue directory hash count ])

AC_ARG_WITH(explicitsync, [--with-explicitsync       Explicitly sync cache files],
	explicitsync="$withval")

case "$explicitsync" in
y*|Y*|1*)
	AC_DEFINE_UNQUOTED(EXPLICITSYNC,1,
	[ Whether message files are explicitly flushed after creation ])
	;;
esac

AC_ARG_WITH(testmode, [], testmode="$withval", testmode="N")

case "$testmode" in
y*|y*|1)
	testmode=1
	CFLAGS=`echo "$CFLAGS" | sed 's/-O2//'`
	CXXFLAGS=`echo "$CXXFLAGS" | sed 's/-O2//'`
	;;
*)
	testmode=0
	;;
esac
AC_SUBST(testmode)

AC_MSG_CHECKING(for available modules)
for f in $srcdir/../module.*/courier.config
do
	test -f "$f" || continue
	dir=`echo $f | sed 's:/courier.config$::;s:.*/::;'`
	name=` . $f ; echo $NAME`
	distdirlist="$distdirlist $dir"
	eval "DIR_$name=$dir"
done
AC_MSG_RESULT($distdirlist)

AC_ARG_WITH(transport, [],
	transport="$withval",
	AC_MSG_ERROR(--with-transport must be specified))

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
AC_SUBST(transport)

AC_LANG([C++])
AC_CACHE_CHECK([if the C++ compiler needs -fhandle-exceptions],
	ac_cv_need_handlexceptions,

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
throw;
]])],[ac_cv_need_handlexceptions=no],[ac_cv_need_handlexceptions=yes])
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
AC_LANG([C])

AC_RUN_IFELSE([AC_LANG_SOURCE([[

#include	<stdio.h>
#include	<stdlib.h>
#include	<limits.h>
#include	<string.h>

int main()
{
char buf[BUFSIZ];
FILE *fp=fopen("conftestval", "w");

     if (!fp)
     {
	perror("fopen");
	exit(1);
     }
#ifdef ULLONG_MAX
       sprintf(buf, "-%llu", ULLONG_MAX);
#else
       sprintf(buf, "-%lu", ULONG_MAX);
#endif

	fprintf(fp, "%d\n", (int)(strlen(buf)+1));
	fclose(fp);
	return (0);
}

	]])],[ MAXLONGSIZE=`cat conftestval` ],[
		AC_MSG_ERROR(Unable to run test program.)
	 ],[
		MAXLONGSIZE=60
		AC_MSG_WARN([Cross compiling, setting MAXLONGSIZE to $MAXLONGSIZE])

		])

AC_ARG_WITH(courierlogger, [], courierlogger="$withval",
			   AC_MSG_ERROR([Error - no --with-courierlogger?]))
AC_DEFINE_UNQUOTED(COURIERLOGGER, "$courierlogger",
[Location of the courierlogger binary])

AC_DEFINE_UNQUOTED(MAXLONGSIZE, $MAXLONGSIZE, [ Maximum size of an long value ])

. ../../dbobj.config
dblibrary="../../$dblibrary"
AC_SUBST(dblibrary)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
