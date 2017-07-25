#
# Copyright 1998 - 2008 Double Precision, Inc.  See COPYING for
# distribution information.

CLEANFILES=sysconfdir.h $(man_MANS) $(htmldoc_DATA) module.uucp permissions.dat

moduledir=$(libexecdir)/courier/modules/uucp

INSTALL=@INSTALL@ `test \`id -u\` != 0 || echo -o @mailuser@ -g @mailgroup@`
INSTALL_PROGRAM=${INSTALL} -m 550

module.uucp: courier.config
	cp -f $(srcdir)/courier.config module.uucp

sysconf_DATA=module.uucp
module_PROGRAMS=courieruucp

databindir=$(datadir)
databin_SCRIPTS=makeuucpneighbors

EXTRA_DIST=courier.config staticlist.c courieruucp.8.in courieruucp.html.in

noinst_LTLIBRARIES=librewrite.la

install-data-hook:
	-$(mkinstalldirs) $(DESTDIR)$(moduledir)
	-$(mkinstalldirs) $(DESTDIR)$(sbindir)
	@test `id -u` = 0 || exit 0 ; cd $(DESTDIR)$(moduledir) || exit 1 ; chown @mailuser@ . ; chgrp @mailgroup@ .
	rm -f $(DESTDIR)$(sbindir)/makeuucpneighbors ; \
		$(LN_S) $(datadir)/makeuucpneighbors \
			$(DESTDIR)$(sbindir)/makeuucpneighbors
	rm -f $(DESTDIR)$(man8dir)/makeuucpneighbors.8
	$(LN_S) courieruucp.8 $(DESTDIR)$(man8dir)/makeuucpneighbors.8
	for f in $(htmldoc_DATA); do chmod 444 $(DESTDIR)$(htmldocdir)/$$f ; done
	test `id -u` = 0 || exit 0; for f in $(htmldoc_DATA); do chown bin $(DESTDIR)$(htmldocdir)/$$f ; chgrp bin $(DESTDIR)$(htmldocdir)/$$f ; done


courieruucp_SOURCES=uucp.c staticlist.c

courieruucp_DEPENDENCIES=../../courier/libs/libmodule.la \
	librewrite.la \
	../../courier/libs/libcommon.la \
	../../courier/libs/libcourier.la \
	../../libs/rfc2045/librfc2045.la \
	../../libs/rfc822/librfc822.la \
	../../libs/numlib/libnumlib.la \
	../../libs/waitlib/libwaitlib.a \
	@dblibrary@

courieruucp_LDADD=../../courier/libs/libmodule.la \
	librewrite.la \
	../../courier/libs/libcommon.la \
	../../courier/libs/libcourier.la \
	../../libs/rfc2045/librfc2045.la \
	../../libs/rfc822/librfc822.la \
	../../libs/numlib/libnumlib.la \
	../../libs/waitlib/libwaitlib.a \
	@dblibrary@ -lcourier-unicode

htmldocdir=@datadir@/htmldoc
htmldoc_DATA=courieruucp.html
man_MANS=courieruucp.8

BUILT_SOURCES=sysconfdir.h courieruucp.8.in courieruucp.html.in

uninstall-local:
	rm -f $(DESTDIR)$(sbindir)/makeuucpneighbors
	rm -f $(DESTDIR)$(man8dir)/makeuucpneighbors.8

install-perms:
	@$(MAKE) install-perms-recursive
	@$(MAKE) install-perms-local

install-perms-recursive:
	@>permissions.dat
	@SUBDIRS="$(SUBDIRS)" ; for f in $$SUBDIRS ; do ( cd $$f && $(MAKE) $(AM_MAKEFLAGS) install-perms ) ; cat /dev/null >>$$f/permissions.dat ; cat $$f/permissions.dat >>permissions.dat ; done

install-perms-local:
	@echo $(moduledir) 755 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(sysconfdir)/module.uucp 644 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(moduledir)/courieruucp 755 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(htmldocdir)/courieruucp.html 444 bin bin doc >>permissions.dat
	@echo $(sbindir)/makeuucpneighbors 755 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(databindir)/makeuucpneighbors 755 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(man8dir)/courieruucp.8 644 bin bin man >>permissions.dat
	@echo $(man8dir)/makeuucpneighbors.8 644 bin bin man >>permissions.dat

librewrite_la_SOURCES=uucpstub.c
librewrite_la_LDFLAGS=-static

sysconfdir.h: config.status
	echo "#define SYSCONFDIR \"$(sysconfdir)\"" >sysconfdir.h

if HAVE_SGML

courieruucp.html.in: courieruucp.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html courieruucp.sgml courieruucp.html.in

courieruucp.8.in: courieruucp.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man courieruucp.sgml courieruucp.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv courieruucp.8 courieruucp.8.in

endif

courieruucp.html: courieruucp.html.in
	./config.status --file=$@

courieruucp.8: courieruucp.8.in
	./config.status --file=$@
