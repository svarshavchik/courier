#
# Copyright 1998 - 2008 Double Precision, Inc.  See COPYING for
# distribution information.

DEBUGLIB=@DEBUGLIB@

AM_CPPFLAGS=`@COURIERAUTHCONFIG@ --cppflags`

CLEANFILES=libexecdir.h localstatedir.h config module.esmtp permissions.dat
DISTCLEANFILES=esmtp.authpam imapd.cnf esmtpd.cnf

BUILT_SOURCES=localstatedir.h libexecdir.h

if HAVE_SGML

GITFILES=esmtpd.dist.in esmtpd-ssl.dist.in esmtpd-msa.dist.in

BUILT_SOURCES += $(GITFILES)

$(GITFILES): %: %.git
	perl -p -e 's/\$$Id:[^\$$]*/\$$Id: '"`git log --pretty=format:\"%H\" -n 1 -- $<`-`date +%Y%m%d%H%M%S`/" <$< >$@.tmp
	mv -f $@.tmp $@
endif

moduledir=$(libexecdir)/courier/modules/esmtp

databindir=$(datadir)
databin_SCRIPTS=esmtpd esmtpd-ssl makesmtpaccess makeacceptmailfor \
	makepercentrelay mkesmtpdcert

sysconf_DATA=esmtpd.cnf esmtpd.dist esmtpd-msa.dist esmtpd-ssl.dist \
	esmtp.authpam module.esmtp

module.esmtp: courier.config
	cp -f $(srcdir)/courier.config module.esmtp

esmtpd.cnf: $(top_builddir)/config.status

esmtpd.cnf: esmtpd.cnf.@ssllib@
	cp $< $@
	touch $@

module_PROGRAMS=courieresmtp courieresmtpd addcr
INSTALL=@INSTALL@ `test \`id -u\` != 0 || echo -o @mailuser@ -g @mailgroup@`
INSTALL_PROGRAM=${INSTALL} -m 550

EXTRA_DIST=courier.config staticlist.c esmtp.authpam.dist esmtpd.cnf.gnutls

noinst_LTLIBRARIES=librewrite.la libesmtp.la

install-data-hook:
	test `id -u` = 0 || exit 0 ; cd $(DESTDIR)$(moduledir) || exit 1 ; chown @mailuser@ . ; chgrp @mailgroup@ .

ldaddlibdl=@LIBDL@

courieresmtp_SOURCES=courieresmtp.c esmtpclient.c esmtpconfig.c esmtpconfig.h \
		staticlist.c

courieresmtpd_SOURCES=courieresmtpd.c esmtpconfig.c esmtpconfig.h esmtpiov.c \
		esmtpiov.h staticlist.c

courieresmtp_DEPENDENCIES=../../courier/libs/libmodule.la \
			librewrite.la \
			libesmtp.la \
			../../libs/imap/libimapd.la \
			../../courier/libs/libcommon.la \
			../../courier/libs/libcourier.la \
			@dblibrary@ \
			../../libs/rfc1035/librfc1035.a \
			../../libs/rfc2045/librfc2045.la \
			../../libs/rfc822/librfc822.la \
			@SASLLIBS@ \
			../../libs/tcpd/libtlsclient.la \
			../../libs/numlib/libnumlib.la \
			../../libs/waitlib/libwaitlib.a \
			../../libs/tcpd/libspipe.la \
			../../libs/md5/libmd5.la \
			../../libs/random128/librandom128.la \
			../../libs/soxwrap/libsoxwrap.a \
			../../libs/soxwrap/soxlibs.dep

commonLDADD=../../courier/libs/libmodule.la librewrite.la libesmtp.la \
			../../libs/imap/libimapd.la \
			../../courier/libs/libcommon.la \
			../../courier/libs/libcourier.la \
			../../libs/rfc1035/librfc1035.a \
			../../libs/rfc822/librfc822.la \
			../../libs/rfc2045/librfc2045.la \
			@SASLLIBS@ \
			../../libs/tcpd/libtlsclient.la \
			../../libs/numlib/libnumlib.la \
			../../libs/waitlib/libwaitlib.a \
			../../libs/tcpd/libspipe.la \
			@dblibrary@ \
			../../libs/md5/libmd5.la \
			../../libs/random128/librandom128.la \
			../../libs/soxwrap/libsoxwrap.a \
			-lcourier-unicode \
			`cat ../../libs/soxwrap/soxlibs.dep` \
			@NETLIBS@ $(ldaddlibdl) $(DEBUGLIB)

courieresmtp_LDADD=$(commonLDADD) -lcourierauthsaslclient -lcourierauth

courieresmtp_LDFLAGS=`@COURIERAUTHCONFIG@ --ldflags`

courieresmtpd_DEPENDENCIES=$(courieresmtp_DEPENDENCIES) \
				../../libs/tcpd/libspipe.la
courieresmtpd_LDADD=$(commonLDADD) ../../libs/tcpd/libspipe.la \
			-lcourierauthsasl -lcourierauth
courieresmtpd_LDFLAGS=`@COURIERAUTHCONFIG@ --ldflags`

addcr_SOURCES=addcr.c

install-data-local:
	test -f $(DESTDIR)$(sbindir)/courieresmtpd && exit 0; $(mkinstalldirs) $(DESTDIR)$(sbindir) || :; rm -f $(DESTDIR)$(sbindir)/courieresmtpd && $(LN_S) $(moduledir)/courieresmtpd $(DESTDIR)$(sbindir)/courieresmtpd
	-$(mkinstalldirs) $(DESTDIR)$(bindir)
	test -f $(DESTDIR)$(bindir)/addcr && exit 0; rm -f $(DESTDIR)$(bindir)/addcr && $(LN_S) $(moduledir)/addcr $(DESTDIR)$(bindir)/addcr
	-$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/esmtpacceptmailfor.dir
	-$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/esmtppercentrelay.dir
	-$(mkinstalldirs) $(DESTDIR)$(sbindir)
	for f in $(databin_SCRIPTS) ; do \
		rm -f $(DESTDIR)$(sbindir)/$$f ; \
		$(LN_S) $(datadir)/$$f $(DESTDIR)$(sbindir)/$$f ; done
	cat </dev/null >>$(DESTDIR)$(sysconfdir)/esmtpauthclient
	chmod 600 $(DESTDIR)$(sysconfdir)/esmtpauthclient
	test `id -u` = 0 || exit 0; \
		chown @mailuser@ $(DESTDIR)$(sysconfdir)/esmtpauthclient ; \
		chgrp @mailgroup@ $(DESTDIR)$(sysconfdir)/esmtpauthclient
	rm -f $(DESTDIR)$(sbindir)/esmtpd-msa $(DESTDIR)$(sbindir)/makesmtpaccess-msa
	$(LN_S) esmtpd $(DESTDIR)$(sbindir)/esmtpd-msa
	$(LN_S) makesmtpaccess $(DESTDIR)$(sbindir)/makesmtpaccess-msa

uninstall-local:
	rm -f $(DESTDIR)$(sbindir)/courieresmtpd
	rm -f $(DESTDIR)$(bindir)/addcr
	rm -f $(DESTDIR)$(sbindir)/esmtpd-msa
	rm -f $(DESTDIR)$(sbindir)/makesmtpaccess-msa
	for f in $(databin_SCRIPTS) ; do \
		rm -f $(DESTDIR)$(sbindir)/$$f ; done
	test "`cat $(DESTDIR)$(sysconfdir)/esmtpauthclient`" = "" || exit 0; \
		rm -f $(DESTDIR)$(sysconfdir)/esmtpauthclient

install-perms:
	@$(MAKE) install-perms-recursive
	@$(MAKE) install-perms-local

install-perms-recursive:
	@>permissions.dat
	@SUBDIRS="$(SUBDIRS)" ; for f in $$SUBDIRS ; do ( cd $$f && $(MAKE) $(AM_MAKEFLAGS) install-perms ) ; cat /dev/null >>$$f/permissions.dat ; cat $$f/permissions.dat >>permissions.dat ; done

install-perms-local:
	@echo $(moduledir) 755 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(sysconfdir)/module.esmtp 644 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(moduledir)/courieresmtp 550 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(moduledir)/courieresmtpd 550 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(moduledir)/addcr 555 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(sbindir)/courieresmtpd 555 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(bindir)/addcr 555 @mailuser@ @mailgroup@ >>permissions.dat
	@for f in $(databin_SCRIPTS) ; do \
	   echo $(sbindir)/$$f 555 @mailuser@ @mailgroup@ >>permissions.dat ; \
	   echo $(datadir)/$$f 555 @mailuser@ @mailgroup@ >>permissions.dat ; \
		done
	@echo $(sbindir)/esmtpd-msa 555 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(sbindir)/makesmtpaccess-msa 555 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(sysconfdir)/esmtpd.dist 644 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(sysconfdir)/esmtpd-msa.dist 644 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(sysconfdir)/esmtpd-ssl.dist 644 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(sysconfdir)/esmtpacceptmailfor.dir 755 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(sysconfdir)/esmtppercentrelay.dir 755 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(sysconfdir)/esmtp.authpam 644 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(sysconfdir)/esmtpd.cnf 644 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(sysconfdir)/esmtpauthclient 600 @mailuser@ @mailgroup@ config >>permissions.dat

librewrite_la_SOURCES=esmtp.c
librewrite_la_LDFLAGS=-static

libesmtp_la_SOURCES=libesmtp.c libesmtp.h
libesmtp_la_LDFLAGS=-static

localstatedir.h: config.status
	echo "#define LOCALSTATEDIR \"$(localstatedir)\"" >localstatedir.h

libexecdir.h: config.status
	echo "#define LIBEXECDIR \"$(libexecdir)\"" >libexecdir.h
