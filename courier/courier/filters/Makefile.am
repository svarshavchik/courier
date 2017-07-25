#
# Copyright 2000-2002 Double Precision, Inc.  See COPYING for
# distribution information.

SUBDIRS=libfilter perlfilter

CLEANFILES=$(htmldoc_DATA) $(man_MANS) filtersocketdir.h filteractivedir.h \
	pidfile.h permissions.dat

piddir=@piddir@

htmldocdir=@datadir@/htmldoc
htmldoc_DATA=courierfilter.html courierperlfilter.html dupfilter.html ratefilter.html verifyfilter.html

databindir=$(datadir)
databin_SCRIPTS=filterctl
data_DATA=verifysender verifysenderfull

courierfilterexecdir=$(libexecdir)/courier
courierfilterexec_PROGRAMS=courierfilter

noinst_SCRIPTS=courierfilter.start

filterbindir=$(libexecdir)/filters
filterbin_PROGRAMS=dupfilter ratefilter verifyfilter

filtersocketdir=$(localstatedir)/filters
allfiltersocketdir=$(localstatedir)/allfilters

filterconfdir=$(sysconfdir)/filters
filteractivedir=$(sysconfdir)/filters/active

dupfilter_SOURCES=dupfilter.c duphash.c duphash.h msghash.c msghash.h
dupfilter_DEPENDENCIES=libfilter/libfilter.a ../../libs/md5/libmd5.la \
	../libs/libcommon.la ../libs/libcourier.la \
	../../libs/liblock/liblock.la \
	../../libs/threadlib/libthreadlib.a ../../libs/numlib/libnumlib.la \
	../../libs/threadlib/libthread.dep
dupfilter_LDADD=libfilter/libfilter.a ../../libs/md5/libmd5.la \
	../libs/libcommon.la ../libs/libcourier.la \
	../../libs/liblock/liblock.la \
	../../libs/threadlib/libthreadlib.a ../../libs/numlib/libnumlib.la \
	`cat ../../libs/threadlib/libthread.dep` @NETLIBS@

ratefilter_SOURCES=ratefilter.C
ratefilter_DEPENDENCIES=libfilter/libfilter.a ../../afx/libafx.a \
	../libs/libcommon.la ../libs/libcourier.la
ratefilter_LDADD=libfilter/libfilter.a ../../afx/libafx.a \
	../libs/libcommon.la ../libs/libcourier.la @NETLIBS@



verifyfilter_SOURCES=verifyfilter.c
verifyfilter_DEPENDENCIES=libfilter/libfilter.a \
	../module.esmtp/libesmtp.la \
	../libs/libcommon.la ../libs/libcourier.la \
	@dblibrary@ \
	../../libs/liblock/liblock.la \
	../../libs/threadlib/libthreadlib.a ../../libs/numlib/libnumlib.la \
	../../libs/threadlib/libthread.dep \
	../../libs/rfc1035/librfc1035.a \
	../../libs/rfc2045/librfc2045.la \
	../../libs/rfc822/librfc822.la \
	../../libs/tcpd/libspipe.la \
	../../libs/tcpd/libtlsclient.la \
	../../libs/md5/libmd5.la \
	../../libs/soxwrap/libsoxwrap.a \
	../../libs/soxwrap/soxlibs.dep

verifyfilter_LDADD=libfilter/libfilter.a \
	../module.esmtp/libesmtp.la \
	../libs/libcommon.la ../libs/libcourier.la \
	@dblibrary@ \
	../../libs/liblock/liblock.la \
	../../libs/threadlib/libthreadlib.a ../../libs/numlib/libnumlib.la \
	`cat ../../libs/threadlib/libthread.dep` \
	../../libs/rfc1035/librfc1035.a \
	../../libs/rfc2045/librfc2045.la \
	../../libs/rfc822/librfc822.la \
	../../libs/tcpd/libspipe.la \
	../../libs/tcpd/libtlsclient.la \
	../../libs/md5/libmd5.la \
	../../libs/soxwrap/libsoxwrap.a \
	`cat ../../libs/soxwrap/soxlibs.dep` \
	-lcourierauthsaslclient -lcourierauth -lcourier-unicode \
	@NETLIBS@

verifyfilter_LDFLAGS=`@COURIERAUTHCONFIG@ --ldflags`

courierfilter_SOURCES=courierfilter.c
courierfilter_DEPENDENCIES=../libs/libcommon.la ../libs/libcourier.la \
	../../libs/liblock/liblock.la ../../libs/numlib/libnumlib.la

courierfilter_LDADD=../libs/libcommon.la ../libs/libcourier.la \
	../../libs/liblock/liblock.la ../../libs/numlib/libnumlib.la

install-exec-hook:
	-$(mkinstalldirs) $(DESTDIR)$(sbindir)
	$(INSTALL_SCRIPT) courierfilter.start $(DESTDIR)$(sbindir)/courierfilter
	chmod 555 $(DESTDIR)$(sbindir)/courierfilter
	chmod 500 $(DESTDIR)$(courierfilterexecdir)/courierfilter

install-data-hook:
	-$(mkinstalldirs) $(DESTDIR)$(filtersocketdir)
	-$(mkinstalldirs) $(DESTDIR)$(allfiltersocketdir)
	-$(mkinstalldirs) $(DESTDIR)$(filterconfdir)
	-$(mkinstalldirs) $(DESTDIR)$(filteractivedir)
	-$(mkinstalldirs) $(DESTDIR)$(sbindir)
	-$(mkinstalldirs) $(DESTDIR)$(bindir)
	chmod 500 $(DESTDIR)$(datadir)/filterctl
	rm -f $(DESTDIR)$(sbindir)/filterctl
	$(LN_S) $(datadir)/filterctl $(DESTDIR)$(sbindir)/filterctl
	rm -f $(DESTDIR)$(bindir)/verifysmtp
	ln $(DESTDIR)$(filterbindir)/verifyfilter $(DESTDIR)$(bindir)/verifysmtp

	chmod 750 $(DESTDIR)$(filtersocketdir)
	chmod 750 $(DESTDIR)$(allfiltersocketdir)
	chmod 750 $(DESTDIR)$(filterconfdir)
	chmod 750 $(DESTDIR)$(filteractivedir)
	-chown @mailuser@ $(DESTDIR)$(filtersocketdir) \
		$(DESTDIR)$(allfiltersocketdir) \
		$(DESTDIR)$(filterconfdir) \
		$(DESTDIR)$(filteractivedir) 2>/dev/null
	-chgrp @mailgroup@ $(DESTDIR)$(filtersocketdir) \
		$(DESTDIR)$(allfiltersocketdir) \
		$(DESTDIR)$(filterconfdir) \
		$(DESTDIR)$(filteractivedir) 2>/dev/null
	cd $(DESTDIR)$(man8dir) || exit 0; \
		rm -f filterctl.8; $(LN_S) courierfilter.8 filterctl.8
	for f in $(htmldoc_DATA); do chmod 444 $(DESTDIR)$(htmldocdir)/$$f ; done
	test `id -u` = 0 || exit 0; for f in $(htmldoc_DATA); do chown bin $(DESTDIR)$(htmldocdir)/$$f ; chgrp bin $(DESTDIR)$(htmldocdir)/$$f ; done
	chmod 755 $(DESTDIR)$(filterbindir)/dupfilter
	chmod 755 $(DESTDIR)$(filterbindir)/verifyfilter
	chmod 755 $(DESTDIR)$(filterbindir)/ratefilter
	test `id -u` = 0 || exit 0; chown @mailuser@ $(DESTDIR)$(filterbindir)/dupfilter; chgrp @mailgroup@ $(DESTDIR)$(filterbindir)/dupfilter
	test `id -u` = 0 || exit 0; chown @mailuser@ $(DESTDIR)$(filterbindir)/ratefilter; chgrp @mailgroup@ $(DESTDIR)$(filterbindir)/ratefilter
	test `id -u` = 0 || exit 0; chown @mailuser@ $(DESTDIR)$(filterbindir)/verifyfilter; chgrp @mailgroup@ $(DESTDIR)$(filterbindir)/verifyfilter

install-perms:
	@$(MAKE) install-perms-recursive
	@$(MAKE) install-perms-local

install-perms-recursive:
	@>permissions.dat
	@SUBDIRS="$(SUBDIRS)" ; for f in $$SUBDIRS ; do ( cd $$f && $(MAKE) $(AM_MAKEFLAGS) install-perms ) ; cat /dev/null >>$$f/permissions.dat ; cat $$f/permissions.dat >>permissions.dat ; done

install-perms-local:
	@echo "$(filterbindir) 755 bin bin" >>permissions.dat
	@echo "$(filterbindir)/dupfilter 755 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(filterbindir)/ratefilter 755 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(filterbindir)/verifyfilter 755 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(bindir)/verifysmtp 755 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(filtersocketdir) 750 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(allfiltersocketdir) 750 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(filterconfdir) 750 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(filteractivedir) 750 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(datadir)/filterctl 500 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(datadir)/verifysender 644 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(datadir)/verifysenderfull 644 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(sbindir)/filterctl 500 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(sbindir)/courierfilter 555 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(courierfilterexecdir)/courierfilter 500 @mailuser@ @mailgroup@" >>permissions.dat
	@echo "$(man8dir)/courierfilter.8 644 bin bin man" >>permissions.dat
	@echo "$(man8dir)/dupfilter.8 644 bin bin man" >>permissions.dat
	@echo "$(man8dir)/ratefilter.8 644 bin bin man" >>permissions.dat
	@echo "$(man8dir)/verifyfilter.8 644 bin bin man" >>permissions.dat
	@echo "$(man8dir)/verifysmtp.8 644 bin bin man" >>permissions.dat
	@echo "$(man8dir)/courierperlfilter.8 644 bin bin man" >>permissions.dat
	@echo "$(man8dir)/filterctl.8 644 bin bin man" >>permissions.dat
	@for f in $(htmldoc_DATA); do echo "$(htmldocdir)/$$f 644 bin bin doc" >>permissions.dat ; done

filtersocketdir.h: config.status
	echo '#define FILTERSOCKETDIR "$(filtersocketdir)"' >filtersocketdir.h
	echo '#define ALLFILTERSOCKETDIR "$(allfiltersocketdir)"' >>filtersocketdir.h

filteractivedir.h: config.status
	echo '#define FILTERACTIVEDIR "$(filteractivedir)"' >filteractivedir.h

pidfile.h: config.status
	echo '#define PIDFILE "$(piddir)/courierfilter.pid"' >pidfile.h

man_MANS=courierfilter.8 dupfilter.8 ratefilter.8 verifyfilter.8 verifysmtp.8 courierperlfilter.8
BUILT_SOURCES=$(man_MANS) $(htmldoc_DATA) \
	filtersocketdir.h filteractivedir.h pidfile.h

uninstall-local:
	rm -f $(DESTDIR)$(man8dir)/filterctl.8
	rm -f $(DESTDIR)$(sbindir)/filterctl
	rm -f $(DESTDIR)$(sbindir)/courierfilter
	rm -f $(DESTDIR)$(bindir)/verifysmtp

EXTRA_DIST=testsuite testsuite.txt \
	courierfilter.8.in dupfilter.8.in verifyfilter.8.in verifysmtp.8.in courierperlfilter.8.in \
	courierfilter.html.in courierperlfilter.html.in dupfilter.html.in verifyfilter.html.in \
	ratefilter.8.in ratefilter.html.in

if HAVE_SGML
courierfilter.html.in: courierfilter.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html courierfilter.sgml courierfilter.html.in

courierfilter.8.in: courierfilter.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man courierfilter.sgml courierfilter.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv courierfilter.8 courierfilter.8.in

courierperlfilter.html.in: courierperlfilter.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html courierperlfilter.sgml courierperlfilter.html.in

courierperlfilter.8.in: courierperlfilter.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man courierperlfilter.sgml courierperlfilter.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv courierperlfilter.8 courierperlfilter.8.in

dupfilter.html.in: dupfilter.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html dupfilter.sgml dupfilter.html.in

dupfilter.8.in: dupfilter.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man dupfilter.sgml dupfilter.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv dupfilter.8 dupfilter.8.in

ratefilter.html.in: ratefilter.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html ratefilter.sgml ratefilter.html.in

ratefilter.8.in: ratefilter.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man ratefilter.sgml ratefilter.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv ratefilter.8 ratefilter.8.in

verifyfilter.html.in: verifyfilter.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html verifyfilter.sgml verifyfilter.html.in

verifyfilter.stamp: verifyfilter.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man verifyfilter.sgml verifyfilter.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	touch verifyfilter.stamp

verifyfilter.8.in: verifyfilter.stamp
	mv verifyfilter.8 verifyfilter.8.in
	touch verifyfilter.8.in

verifysmtp.8.in: verifyfilter.stamp
	mv verifysmtp.8 verifysmtp.8.in
	touch verifysmtp.8.in

endif

courierfilter.html: courierfilter.html.in
	./config.status --file=$@

courierfilter.8: courierfilter.8.in
	./config.status --file=$@

courierperlfilter.html: courierperlfilter.html.in
	./config.status --file=$@

courierperlfilter.8: courierperlfilter.8.in
	./config.status --file=$@

dupfilter.html: dupfilter.html.in
	./config.status --file=$@

dupfilter.8: dupfilter.8.in
	./config.status --file=$@

ratefilter.html: ratefilter.html.in
	./config.status --file=$@

ratefilter.8: ratefilter.8.in
	./config.status --file=$@

verifyfilter.html: verifyfilter.html.in
	./config.status --file=$@

verifyfilter.8: verifyfilter.8.in
	./config.status --file=$@

verifysmtp.8: verifysmtp.8.in
	./config.status --file=$@

check-am:
	@SHELL@ $(srcdir)/testsuite | diff - $(srcdir)/testsuite.txt
