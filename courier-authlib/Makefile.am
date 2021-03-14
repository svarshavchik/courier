#
# Copyright 1998 - 2020 Double Precision, Inc.  See COPYING for
# distribution information.

AUTOMAKE_OPTIONS=dist-bzip2

SUBDIRS=libs/gdbmobj libs/bdbobj libs/md5 libs/sha1 libs/libhmac libs/numlib libs/makedat userdb libs/rfc822 libs/random128 libs/liblock liblog

AM_CPPFLAGS=-I liblock $(LTDLINCL)
BUILT_SOURCES=

modules=@LIBAUTHUSERDB@ \
	@LIBAUTHPAM@ @LIBAUTHPWD@ @LIBAUTHSHADOW@ \
	@LIBAUTHPGSQL@ @LIBAUTHLDAP@ @LIBAUTHMYSQL@ @LIBAUTHSQLITE@ \
	@LIBAUTHCUSTOM@ @LIBAUTHPIPE@

pkglibexecdir=$(libexecdir)/courier-authlib
pkglibexec_SCRIPTS=authsystem.passwd

pkglib_LTLIBRARIES=libcourierauth.la \
	libcourierauthcommon.la libcourierauthsasl.la libcourierauthsaslclient.la \
	$(modules)
sbin_SCRIPTS=authdaemond
sbin_PROGRAMS=authtest authenumerate authpasswd
bin_PROGRAMS=courierauthconfig

EXTRA_LTLIBRARIES=libauthuserdb.la \
	libauthpam.la libauthpwd.la libauthshadow.la \
	libauthpgsql.la \
	libauthldap.la \
	libauthmysql.la \
	libauthsqlite.la \
	libauthcustom.la \
	libauthpipe.la

CLEANFILES=authldaprc.h authmysqlrc.h authsqliterc.h authdaemonrc.h \
	authpgsqlrc.h authpiperc.h authdaemon \
	authlib.3 authlib.html packageversion.h \
	authchangepwdir.h sbindir.h pkglibdir.h pkgincludedir.h \
	mailusergroup.h authinfodaemondlist.c permissions.dat \
	README.authdebug.html

DISTCLEANFILES=dbobj.config README_authlib.html

commonlibdep=libcourierauthcommon.la
commonldflags=-module -rpath $(pkglibdir) -export-symbols-regex 'courier_auth.*_init'  -version-info @VERSION_INFO@ @courierauth_ldflags@
commonlibadd=libcourierauthcommon.la

libcourierauthcommon_t=@CRYPTLIBS@
libcourierauthcommon_la_SOURCES= \
	auth.h courierauth.h \
	authconfigfile.cpp \
	authconfigfile.h \
	authpwdenumerate.c \
	authsaslfrombase64.c authsasltobase64.c \
	courierauthstaticlist.h \
	authsyschangepwd.c authsyscommon.c \
	authldapescape.c \
	cramlib.c \
	checkpassword.c cryptpassword.c \
	checkpasswordmd5.c checkpasswordsha1.c

libcourierauthcommon_la_DEPENDENCIES=libcourierauth.la
libcourierauthcommon_la_LIBADD=libcourierauth.la
libcourierauthcommon_la_LDFLAGS=$(libcourierauthcommon_t:%=-Wl,%) -version-info @VERSION_INFO@

libcourierauth_la_SOURCES=authdaemon.c authdaemonlib.c preauthdaemon.c \
	authmoduser2.c authmoduser3.c debug.c \
	courierauthdebug.h \
	authoption.c
libcourierauth_la_LIBADD=libs/libhmac/libhmac.la libs/md5/libmd5.la libs/sha1/libsha1.la libs/numlib/libnumlib.la \
	libs/random128/librandom128.la libs/rfc822/libencode.la
libcourierauth_la_LDFLAGS=@NETLIBS@ -version-info @VERSION_INFO@

# The userdb module

libauthuserdb_t=userdb/libuserdb.la @dblibrary@ @LIBGDBM@ @LIBDB@
libauthuserdb_la_SOURCES=authuserdb.c preauthuserdb.c preauthuserdbcommon.c \
	 authuserdbpwd.c
libauthuserdb_la_DEPENDENCIES=libcourierauthcommon.la @dblibrary@
libauthuserdb_la_LIBADD=$(commonlibadd) $(libauthuserdb_t)
libauthuserdb_la_LDFLAGS=$(commonldflags)

# the authpam module

libauthpam_t=-lpam @SHADOWLIBS@ @AUTHPAMCRYPT@ @LIBDL@

libauthpam_la_SOURCES=authpam.c preauthpam.c authwait.h
libauthpam_la_DEPENDENCIES=$(commonlibdep)
libauthpam_la_LIBADD=$(commonlibadd) $(libauthpam_t)
libauthpam_la_LDFLAGS=$(commonldflags)

# The authpwd module

libauthpwd_la_SOURCES=authpwd.c preauthpwd.c
libauthpwd_la_DEPENDENCIES=$(commonlibdep)
libauthpwd_la_LIBADD=$(commonlibadd)
libauthpwd_la_LDFLAGS=$(commonldflags)

# The authshadow module

libauthshadow_t=@SHADOWLIBS@

libauthshadow_la_SOURCES=authshadow.c preauthshadow.c
libauthshadow_la_DEPENDENCIES=$(commonlibdep)
libauthshadow_la_LIBADD=$(commonlibadd) $(libauthshadow_t)
libauthshadow_la_LDFLAGS=$(commonldflags)

# The authpgsql module

libauthpgsql_t=@PGSQL_LIBS@ @LIBM@ @NETLIBS@

libauthpgsql_la_SOURCES=authpgsql.c authpgsqllib.cpp authpgsql.h
libauthpgsql_la_DEPENDENCIES=$(commonlibdep)
libauthpgsql_la_LIBADD=$(commonlibadd) $(libauthpgsql_t)
libauthpgsql_la_LDFLAGS=$(commonldflags)

if HAVE_AUTHPGSQL

install-authpgsqlrc:
	$(mkinstalldirs) $(DESTDIR)`dirname @authpgsqlrc@` || :
	$(INSTALL_DATA) -m 660 $(srcdir)/authpgsqlrc \
		$(DESTDIR)@authpgsqlrc@.dist
	-chown @mailuser@ $(DESTDIR)@authpgsqlrc@.dist
	-chgrp @mailgroup@ $(DESTDIR)@authpgsqlrc@.dist

uninstall-authpgsqlrc:
	rm -f $(DESTDIR)@authpgsqlrc@.dist

else

install-authpgsqlrc:
	@:

uninstall-authpgsqlrc:
	@:
endif

authpgsqlrc.h:
	echo "#define AUTHPGSQLRC \"@authpgsqlrc@\"" >authpgsqlrc.h


# The authldap module

libauthldap_t=@LDAPLIBS@ @NETLIBS@
libauthldap_la_SOURCES=authldap.c preauthldap.c authldaplib.cpp authldap.h
libauthldap_la_DEPENDENCIES=$(commonlibdep)
libauthldap_la_LIBADD=$(commonlibadd) $(libauthldap_t)
libauthldap_la_LDFLAGS=$(commonldflags)

if HAVE_LDAP

install-authldaprc:
	$(mkinstalldirs) $(DESTDIR)`dirname @authldaprc@` || :
	$(INSTALL_DATA) -m 660 $(srcdir)/authldaprc \
		$(DESTDIR)@authldaprc@.dist
	-chown @mailuser@ $(DESTDIR)@authldaprc@.dist
	-chgrp @mailgroup@ $(DESTDIR)@authldaprc@.dist

uninstall-authldaprc:
	rm -f $(DESTDIR)@authldaprc@.dist

else
install-authldaprc:
	@:

uninstall-authldaprc:
	@:
endif

authldaprc.h:
	echo "#define AUTHLDAPRC \"@authldaprc@\"" >authldaprc.h

# The authmysql module

libauthmysql_t=@MYSQL_LIBS@ @LIBM@ @NETLIBS@

libauthmysql_la_SOURCES=authmysql.cpp preauthmysql.cpp authmysqllib.cpp authmysql.h
libauthmysql_la_DEPENDENCIES=$(commonlibdep)
libauthmysql_la_LIBADD=$(commonlibadd) $(libauthmysql_t)
libauthmysql_la_LDFLAGS=$(commonldflags)

if HAVE_AUTHMYSQL
install-authmysqlrc:
	$(mkinstalldirs) $(DESTDIR)`dirname @authmysqlrc@` || :
	$(INSTALL_DATA) -m 660 $(srcdir)/authmysqlrc \
		$(DESTDIR)@authmysqlrc@.dist
	-chown @mailuser@ $(DESTDIR)@authmysqlrc@.dist
	-chgrp @mailgroup@ $(DESTDIR)@authmysqlrc@.dist

uninstall-authmysqlrc:
	rm -f $(DESTDIR)@authmysqlrc@.dist

else
install-authmysqlrc:
	@:

uninstall-authmysqlrc:
	@:
endif

authmysqlrc.h:
	echo "#define AUTHMYSQLRC \"@authmysqlrc@\"" >authmysqlrc.h

# The authsqlite module

libauthsqlite_t=@SQLITE_LIBS@ @NETLIBS@

libauthsqlite_la_SOURCES=authsqlite.cpp authsqlitelib.cpp authsqlite.h
libauthsqlite_la_DEPENDENCIES=$(commonlibdep)
libauthsqlite_la_LIBADD=$(commonlibadd) $(libauthsqlite_t)
libauthsqlite_la_LDFLAGS=$(commonldflags)

if HAVE_AUTHSQLITE
install-authsqliterc:
	$(mkinstalldirs) $(DESTDIR)`dirname @authsqliterc@` || :
	$(INSTALL_DATA) -m 660 $(srcdir)/authsqliterc \
		$(DESTDIR)@authsqliterc@.dist
	-chown @mailuser@ $(DESTDIR)@authsqliterc@.dist
	-chgrp @mailgroup@ $(DESTDIR)@authsqliterc@.dist

uninstall-authsqliterc:
	rm -f $(DESTDIR)@authsqliterc@.dist

else
install-authsqliterc:
	@:

uninstall-authsqliterc:
	@:
endif

authsqliterc.h:
	echo "#define AUTHSQLITERC \"@authsqliterc@\"" >authsqliterc.h

# The authcustom module

libauthcustom_la_SOURCES=authcustom.c preauthcustom.c authcustom.h
libauthcustom_la_DEPENDENCIES=$(commonlibdep)
libauthcustom_la_LIBADD=$(commonlibadd)
libauthcustom_la_LDFLAGS=$(commonldflags)

# The authpipe module
libauthpipe_la_SOURCES=authpipe.c authpipelib.h authpipelib.c
libauthpipe_la_DEPENDENCIES=$(commonlibdep)
libauthpipe_la_LIBADD=$(commonlibadd)
libauthpipe_la_LDFLAGS=$(commonldflags)

authpipelib.lo: authpipelib.c authpipelib.h authpiperc.h

authpiperc.h:
	echo "#define PIPE_PROGRAM \"@authProg@\"" >authpiperc.h


#
# AUTHMODULES should be listed in the best authentication order
#

noinst_PROGRAMS= \
	authdaemontest authmksock \
	authdaemondprog

install-authdaemonrc:
	$(mkinstalldirs) $(DESTDIR)`dirname @authdaemonrc@` || :
	sed 's/@ALLMODULES@/$(modules:lib%.la=%)/' <authdaemonrc >authdaemonrc.tmp
	$(INSTALL_DATA) -m 660 authdaemonrc.tmp $(DESTDIR)@authdaemonrc@.dist
	rm -f authdaemonrc.tmp
	-chown @mailuser@ $(DESTDIR)@authdaemonrc@.dist
	-chgrp @mailgroup@ $(DESTDIR)@authdaemonrc@.dist

uninstall-authdaemonrc:
	rm -f $(DESTDIR)@authdaemonrc@.dist

authdaemonrc.h:
	echo "#define AUTHDAEMONRC \"@authdaemonrc@\"" >authdaemonrc.h
	echo "#define AUTHDAEMONVAR \"@authdaemonvar@\"" >>authdaemonrc.h
	echo "#define AUTHDAEMONPID AUTHDAEMONVAR \"/pid\"" >>authdaemonrc.h
	echo "#define AUTHDAEMONSOCK AUTHDAEMONVAR \"/socket\"" >>authdaemonrc.h
	echo "#define AUTHDAEMONLOCK AUTHDAEMONVAR \"/lock\"" >>authdaemonrc.h

BUILT_SOURCES += authdaemonrc.h

HTMLFILES=README.html NEWS.html INSTALL.html \
	README.authsqlite.html README.authmysql.html README.authpostgres.html


if HAVE_SGML

GITFILES=authldaprc authmysqlrc authpgsqlrc authsqliterc authdaemonrc.in

$(GITFILES): %: %.git
	perl -p -e 's/\$$Id:[^\$$]*/\$$Id: '"`git log --pretty=format:\"%H\" -n 1 -- $<`-`date +%Y%m%d%H%M%S`/" <$< >$@.tmp
	mv -f $@.tmp $@
endif

EXTRA_DIST=$(HTMLFILES) sysconftool COPYING COPYING.GPL\
	authldap.schema \
	authldap.ldif \
	$(GITFILES) README.ldap \
	README.authmysql.myownquery \
	README.authdebug.html.in \
	courier-authlib.spec \
	pgpkeys.txt \
	README_authlib.html.in \
	authlib.html.in authlib.3.in \
	authpasswd.html authpasswd.1 \
	authtest.html authtest.1 \
	auth_sasl.html auth_sasl.3 auth_sasl_ex.3 \
	auth_generic.html auth_generic.3 auth_generic_meta.3 \
	auth_login.html auth_login.3 auth_login_meta.3 \
	auth_passwd.html auth_passwd.3 \
	auth_meta.html auth_meta.3 \
	auth_getuserinfo.html auth_getuserinfo.3 auth_getuserinfo_meta.3 \
	auth_mkhomedir.html auth_mkhomedir.3 \
	auth_enumerate.html auth_enumerate.3 \
	auth_getoption.html auth_getoption.3 \
	samplepipe.pl

README: README.html
	links -dump -no-references README.html >README

NEWS: NEWS.html
	links -dump -no-references NEWS.html >NEWS

INSTALL: INSTALL.html
	rm -f INSTALL; links -dump -no-references INSTALL.html >INSTALL

libcourierauthsasl_la_SOURCES=\
	authsasl.c courierauthsasl.h \
	authsaslcram.c authsasllogin.c authsaslplain.c \
	authsaslfrombase64.c authsasltobase64.c
libcourierauthsasl_la_LIBADD=libcourierauthcommon.la libcourierauth.la
libcourierauthsasl_la_LDFLAGS=-export-symbols-regex '^auth_sasl|auth_sasl_ex|auth_sasl_extract_userid$$' -version-info @VERSION_INFO@

libcourierauthsaslclient_la_SOURCES=courierauthsaslclient.h \
	authsaslclient.c authsaslclient.h authsaslclientcram.c \
	authsaslclientcrammd5.c authsaslclientcramsha1.c \
	authsaslclientlogin.c authsaslclientplain.c \
	authsaslclientexternal.c \
	authsaslfrombase64.c authsasltobase64.c
libcourierauthsaslclient_la_LIBADD=libcourierauth.la
libcourierauthsaslclient_la_LDFLAGS=-export-symbols-regex '^auth_sasl_client$$' -version-info @VERSION_INFO@

include_HEADERS=courierauth.h courierauthsasl.h \
	courierauthsaslclient.h courierauthdebug.h courierauthstaticlist.h

courierauthconfig_SOURCES=authinfo.c

authpasswd_SOURCES=authpasswd.c
authpasswd_LDADD=libcourierauthcommon.la \
        libs/libhmac/libhmac.la libs/md5/libmd5.la libs/sha1/libsha1.la libs/rfc822/libencode.la

authtest_SOURCES=authtest.c
authtest_DEPENDENCIES=libcourierauth.la
authtest_LDADD=libcourierauth.la

authenumerate_SOURCES=authenumerate.c
authenumerate_DEPENDENCIES = libcourierauth.la \
	libs/numlib/libnumlib.la
authenumerate_LDADD = libcourierauth.la \
	libs/numlib/libnumlib.la
authenumerate_LDFLAGS=

#
# The main daemon.

authdaemondprog_SOURCES=authdaemond.c authdaemondcpp.cpp

authdaemond.o: authdaemond.h

authdaemond.h: libcourierauthcommon.la
	. ./libcourierauthcommon.la; echo '#define COURIERDLEXT "'`echo $$dlname | sed s/libcourierauthcommon//`'"' >authdaemond.h

CLEANFILES += authdaemond.h

authdaemondprog_DEPENDENCIES=libcourierauthcommon.la \
	libcourierauth.la \
	libs/liblock/liblock.la $(modules) \
	libs/libhmac/libhmac.la libs/md5/libmd5.la libs/sha1/libsha1.la \
	libs/rfc822/libencode.la libs/numlib/libnumlib.la
authdaemondprog_LDADD=$(LIBLTDL) libcourierauthcommon.la libcourierauth.la libs/liblock/liblock.la \
	libs/libhmac/libhmac.la libs/md5/libmd5.la libs/sha1/libsha1.la \
	libs/rfc822/libencode.la libs/numlib/libnumlib.la
authdaemondprog_LDFLAGS=-export-dynamic $(modules:%=-dlopen %) @NETLIBS@

authdaemontest_SOURCES=authdaemontest.c
authdaemontest_DEPENDENCIES=libcourierauthcommon.la
authdaemontest_LDADD=libcourierauthcommon.la @NETLIBS@
authdaemontest_LDFLAGS=-static

authmksock_SOURCES=authmksock.c
authmksock_LDADD=@NETLIBS@

man3=authlib.3 auth_generic.3 auth_login.3 auth_getuserinfo.3 auth_mkhomedir.3 \
	auth_enumerate.3 auth_passwd.3 auth_getoption.3 auth_sasl.3 \
	auth_meta.3

extraman3=auth_generic_meta.3 auth_login_meta.3 auth_getuserinfo_meta.3 \
	auth_sasl_ex.3
man1=authpasswd.1 authtest.1

man_MANS=$(man1) $(man3) $(extraman3)

BUILT1=authlib.html authldaprc.h authmysqlrc.h authpgsqlrc.h authpiperc.h \
	authdaemonrc.h packageversion.h \
	authchangepwdir.h sbindir.h \
	pkglibdir.h pkgincludedir.h mailusergroup.h \
	README_authlib.html
BUILT_SOURCES += $(BUILT1) $(GITFILES) $(man3:%.3=%.html) $(man1:%.1=%.html) \
	authsqliterc.h authmysqlrc.h authpgsqlrc.h

authchangepwdir.h: config.status
	echo '#define AUTHCHANGEPWDIR "$(pkglibexecdir)"' >authchangepwdir.h

sbindir.h: config.status
	echo '#define SBINDIR "@sbindir@"' >sbindir.h

pkglibdir.h: config.status
	echo '#define PKGLIBDIR "$(pkglibdir)"' >pkglibdir.h

packageversion.h: config.status
	echo '#define PKGVERSION "@PACKAGE_VERSION@"' >packageversion.h

pkgincludedir.h: config.status
	echo '#define PKGINCLUDEDIR "$(includedir)"' >pkgincludedir.h

mailusergroup.h: config.status
	echo '#define MAILUSER "@mailuser@"' >mailusergroup.h
	echo '#define MAILGROUP "@mailgroup@"' >>mailusergroup.h

noinst_DATA=$(BUILT_SOURCES)

dist-hook:
	for f in $(BUILT1) ; do rm -f $(distdir)/$$f; done

# automake still a bit stupid...

install-data-hook: install-authdaemonrc install-authpgsqlrc \
	install-authldaprc install-authmysqlrc install-authsqliterc
	:

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(bindir) || :
	$(mkinstalldirs) $(DESTDIR)$(sbindir) || :
	$(mkinstalldirs) $(DESTDIR)$(pkglibexecdir) || :
	chmod 755 $(DESTDIR)$(pkglibexecdir)
	-chown @mailuser@ $(DESTDIR)$(pkglibexecdir)
	-chgrp @mailgroup@ $(DESTDIR)$(pkglibexecdir)
	$(INSTALL_PROGRAM_ENV) $(LIBTOOL) --mode=install $(INSTALL_PROGRAM) authdaemondprog $(DESTDIR)$(pkglibexecdir)/authdaemond
	$(mkinstalldirs) $(DESTDIR)@authdaemonvar@ || :
	chmod 750 $(DESTDIR)@authdaemonvar@
	-chown @mailuser@ $(DESTDIR)@authdaemonvar@
	-chgrp @mailgroup@ $(DESTDIR)@authdaemonvar@
	test "@LIBAUTHUSERDB@" = "" && exit 0 ;\
		$(INSTALL_PROGRAM_ENV) $(INSTALL_PROGRAM) libs/makedat/makedatprog \
				$(DESTDIR)$(pkglibexecdir)/makedatprog ; \
		$(INSTALL_SCRIPT) userdb/pw2userdb \
				$(DESTDIR)$(sbindir)/pw2userdb ; \
		$(INSTALL_SCRIPT) userdb/makeuserdb \
				$(DESTDIR)$(sbindir)/makeuserdb ; \
		$(INSTALL_SCRIPT) userdb/userdb.pl \
				$(DESTDIR)$(sbindir)/userdb ; \
		$(INSTALL_PROGRAM_ENV) $(INSTALL_PROGRAM) userdb/userdbpw \
				$(DESTDIR)$(sbindir)/userdbpw ; \
		$(INSTALL_SCRIPT) userdb-test-cram-md5.pl \
				$(DESTDIR)$(sbindir)/userdb-test-cram-md5

uninstall-hook: uninstall-authldaprc uninstall-authdaemonrc uninstall-authmysqlrc uninstall-authpgsqlrc uninstall-authsqliterc
	rm -f $(DESTDIR)$(pkglibexecdir)/authdaemond
	test "@AUTHUSERDB@" = "" && exit 0 ;\
		rm -f $(DESTDIR)$(pkglibexecdir)/makedatprog \
				$(DESTDIR)$(sbindir)/pw2userdb \
				$(DESTDIR)$(sbindir)/makeuserdb \
				$(DESTDIR)$(sbindir)/userdb \
				$(DESTDIR)$(sbindir)/userdbpw \
				$(DESTDIR)$(sbindir)/userdb-test-cram-md5

authlib.html: authlib.html.in
	$(SHELL) ./config.status --file=authlib.html

authlib.3: authlib.3.in
	$(SHELL) ./config.status --file=authlib.3

if HAVE_SGML

authpasswd.html: authpasswd.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html authpasswd.sgml authpasswd.html

authpasswd.1: authpasswd.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man authpasswd.sgml authpasswd.1

authtest.html: authtest.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html authtest.sgml authtest.html

authtest.1: authtest.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man authtest.sgml authtest.1

authlib.html.in: authlib.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html authlib.sgml authlib.html.in

authlib.3.in: authlib.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man authlib.sgml authlib.3.in
	mv authlib.3 authlib.3.in

README_authlib.html.in: README_authlib.sgml optionlist.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html README_authlib.sgml README_authlib.html.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_generic.html: auth_generic.sgml authcallback.sgml optionlist.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html auth_generic.sgml auth_generic.html "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_generic.3: auth_generic_meta.3
	touch $@

auth_generic_meta.3: auth_generic.sgml authcallback.sgml optionlist.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man auth_generic.sgml auth_generic_meta.3 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_login.html: auth_login.sgml authcallback.sgml optionlist.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html auth_login.sgml auth_login.html "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_login.3: auth_login_meta.3
	touch $@

auth_login_meta.3: auth_login.sgml authcallback.sgml optionlist.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man auth_login.sgml auth_login_meta.3 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_getuserinfo.html: auth_getuserinfo.sgml authcallback.sgml optionlist.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html auth_getuserinfo.sgml auth_getuserinfo.html "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"
auth_getuserinfo.3: auth_getuserinfo_meta.3
	touch $@

auth_getuserinfo_meta.3: auth_getuserinfo.sgml authcallback.sgml optionlist.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man auth_getuserinfo.sgml auth_getuserinfo_meta.3 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_meta.3: auth_meta.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man auth_meta.sgml auth_meta.3 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_meta.html: auth_meta.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html auth_meta.sgml auth_meta.html "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_mkhomedir.html: auth_mkhomedir.sgml authcallback.sgml optionlist.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html auth_mkhomedir.sgml auth_mkhomedir.html "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_mkhomedir.3: auth_mkhomedir.sgml authcallback.sgml optionlist.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man auth_mkhomedir.sgml auth_mkhomedir.3 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_enumerate.html: auth_enumerate.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html auth_enumerate.sgml auth_enumerate.html "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_enumerate.3: auth_enumerate.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man auth_enumerate.sgml auth_enumerate.3 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_passwd.html: auth_passwd.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html auth_passwd.sgml auth_passwd.html "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_passwd.3: auth_passwd.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man auth_passwd.sgml auth_passwd.3 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_getoption.html: auth_getoption.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html auth_getoption.sgml auth_getoption.html "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_getoption.3: auth_getoption.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man auth_getoption.sgml auth_getoption.3 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_sasl.html: auth_sasl.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html auth_sasl.sgml auth_sasl.html "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_sasl.3: auth_sasl.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man auth_sasl.sgml auth_sasl.3 "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/authlib/"

auth_sasl_ex.3: auth_sasl.3
	touch $@

endif

authldap.ldif: authldap.schema
	echo 'dn: cn=courier,cn=schema,cn=config' >authldap.ldif.tmp
	echo 'objectClass: olcSchemaConfig' >>authldap.ldif.tmp
	echo 'cn: courier' >>authldap.ldif.tmp
	sed '/^#/d;s/^attributetype/olcAttributeTypes:/;s/^objectclass/olcObjectClasses:/;s/^$$/#/' <$(srcdir)/authldap.schema | tr '\011' ' ' >>authldap.ldif.tmp
	mv -f authldap.ldif.tmp authldap.ldif

README_authlib.html: README_authlib.html.in config.status
	$(SHELL) ./config.status --file=README_authlib.html

authdaemond.o: authdaemonrc.h authdaemond.c


install-migrate: authmigrate
	@SHELL@ authmigrate

install-configure:
	test -f $(DESTDIR)@authdaemonrc@.dist && @SYSCONFTOOL@ $(DESTDIR)@authdaemonrc@.dist || true
	test -f $(DESTDIR)@authmysqlrc@.dist && @SYSCONFTOOL@ $(DESTDIR)@authmysqlrc@.dist || true
	test -f $(DESTDIR)@authpgsqlrc@.dist && @SYSCONFTOOL@ $(DESTDIR)@authpgsqlrc@.dist || true
	test -f $(DESTDIR)@authldaprc@.dist && @SYSCONFTOOL@ $(DESTDIR)@authldaprc@.dist || true
	test -f $(DESTDIR)@authsqliterc@.dist && @SYSCONFTOOL@ $(DESTDIR)@authsqliterc@.dist || true

# For maintainer's use only

distrelease:
	umask 022; $(MAKE) distcheck
	$(MAKE) rpm
	$(MAKE)
	$(MAKE)
	rm -f @PACKAGE_NAME@.spec
	$(MAKE) @PACKAGE_NAME@.spec
	$(MAKE) dist

rpm: rpm-build

rpm-build:
	$(MAKE) bump.rpm.release
	$(MAKE) dist
	$(MAKE) dorpm

publish: publish-@REPOSITORY@

publish-dev:
	$(MAKE) dopublish SUBDIR=dev

publish-prod:
	$(MAKE) dopublish SUBDIR=prod

dopublish:
	lpbuild --resign lpm/*
	rm -rf lpmtmp
	mkdir lpmtmp
	ln lpm/* lpmtmp
	rm -f lpmtmp/*debuginfo*
	mv lpmtmp/*.src.lp repository-$(SUBDIR)/`lpm -q --distribution`/sources
	mv lpmtmp/*.lp repository-$(SUBDIR)/`lpm -q --distribution`/packages
	rm -rf lpmtmp
	ls -t repository-$(SUBDIR)/`lpm -q --distribution`/sources | tail -n +6 | xargs -r -n 1 lpm --erase repository-$(SUBDIR)
	lpm --sync repository-$(SUBDIR)
	rsync -a -v --delete-after repository-$(SUBDIR)/`lpm -q --distribution`/. headache:lpmtool.com/www/repositories/$(SUBDIR)/courier-authlib/`lpm -q --distribution`

dorpm:
	rm -rf rpm/BUILD/*
	rm -f rpm/RPMS/*/*
	rm -f rpm/SOURCES/*
	rm -f rpm/SPECS/*
	rm -f rpm/SRPMS/*
	rpmbuild -ta --clean \
		--define "_topdir `pwd`/rpm" \
		--define '_rpmdir %{_topdir}/RPMS' \
		--define '_srcrpmdir %{_topdir}/SRPMS' \
		--define '_sourcedir %{_topdir}/SOURCES' \
		--define '_specdir %{_topdir}/SPECS' \
		--define '_builddir %{_topdir}/BUILD' \
		--define '_build_name_fmt %%{ARCH}/%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm' \
		--define '_tmppath %{_var}/tmp' \
		--define '__spec_prep_pre %{___build_pre}' \
		@PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2

bump.rpm.release: @PACKAGE_NAME@.spec
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=100; \
	test "$$VERSION" = "@PACKAGE_VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@PACKAGE_VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{?dist}%{?courier_release}/' \
		<@PACKAGE_NAME@.spec >@PACKAGE_NAME@.spec.new; \
	mv @PACKAGE_NAME@.spec.new @PACKAGE_NAME@.spec;

upload-beta:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam courier authlib-devel `echo @PACKAGE_VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig

upload-dist:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	set -e; v="@PACKAGE_NAME@/@PACKAGE_VERSION@/`date +%Y%m%d%H%M%S`"; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'; cd libs; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'
	socksify ~/src/sftools/release.pl mrsam courier authlib @PACKAGE_VERSION@ @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig

HTMLDOCFILES=$(HTMLFILES) README.authdebug.html README_authlib.html \
	$(man3:%.3=%.html) $(man1:%.1=%.html) userdb/userdb.html userdb/makeuserdb.html userdb/userdbpw.html

htmldoc: $(HTMLDOCFILES)
	for f in $^; do d=`basename $$f`; test "$$d" = "README.html" && d="index.html"; tidy -i -q --tidy-mark no --doctype transitional --input-encoding utf8 --output-encoding utf8 -asxml <$$f >$$f.tmp ; xsltproc --nonet $$HOME/www/www.courier-mta.org/xsl/authlib.xsl $$f.tmp | perl -ne 's~\$(id):.*\$$~\$(id):'"$$h"'\$$~; print' >$$HOME/www/www.courier-mta.org/authlib/$$d; rm -f $$f.tmp; done
