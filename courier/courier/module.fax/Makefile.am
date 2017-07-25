#
# Copyright 2002-2004 Double Precision, Inc.  See COPYING for
# distribution information.
#

CLEANFILES=filterbindir.h faxtmpdir.h sendfax.h $(man_MANS) $(htmldoc_DATA) \
	module.fax permissions.dat

filterbindir=@filterbindir@
filterbin_SCRIPTS=init text-plain.filter application-postscript.filter \
	application-pdf.filter image-gif.filter image-jpeg.filter \
	image-png.filter coverpage new_fax

BUILT_SOURCES=filterbindir.h faxtmpdir.h sendfax.h \
	courierfax.8.in courierfax.html.in

filterbindir.h: config.status
	echo '#define FILTERBINDIR "@filterbindir@"' >filterbindir.h

faxtmpdir.h: config.status
	echo '#define FAXTMPDIR "@faxtmpdir@"' >faxtmpdir.h

sendfax.h: config.status
	echo '#define SENDFAX "@SENDFAX@"' >sendfax.h

moduledir=$(libexecdir)/courier/modules/fax

INSTALL=@INSTALL@ `test \`id -u\` != 0 || echo -o @mailuser@ -g @mailgroup@`
INSTALL_PROGRAM=${INSTALL} -m 550

module.fax: courier.config
	cp -f $(srcdir)/courier.config module.fax

sysconf_DATA=module.fax faxcoverpage.tr.dist faxrc.dist faxnotifyrc.dist
module_PROGRAMS=courierfax

EXTRA_DIST=courier.config faxcoverpage.tr.dist faxrc.dist courierfax.8.in \
	faxnotifyrc.dist courierfax.html.in

noinst_LTLIBRARIES=librewrite.la

install-data-hook:
	-$(mkinstalldirs) $(DESTDIR)$(moduledir)
	@test `id -u` = 0 || exit 0 ; cd $(DESTDIR)$(moduledir) || exit 1 ; chown @mailuser@ . ; chgrp @mailgroup@ .
	-$(mkinstalldirs) $(DESTDIR)@faxtmpdir@
	chmod 700 $(DESTDIR)@faxtmpdir@
	-chown @mailuser@ $(DESTDIR)@faxtmpdir@ 2>/dev/null
	-chgrp @mailgroup@ $(DESTDIR)@faxtmpdir@ 2>/dev/null

install-perms:
	@$(MAKE) install-perms-recursive
	@$(MAKE) install-perms-local

install-perms-recursive:
	@>permissions.dat
	@SUBDIRS="$(SUBDIRS)" ; for f in $$SUBDIRS ; do ( cd $$f && $(MAKE) $(AM_MAKEFLAGS) install-perms ) ; cat /dev/null >>$$f/permissions.dat ; cat $$f/permissions.dat >>permissions.dat ; done

install-perms-local:
	@echo $(moduledir) 755 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(htmldocdir)/courierfax.html 444 bin bin doc >>permissions.dat
	@echo $(man8dir)/courierfax.8 644 bin bin man >>permissions.dat
	@echo $(sysconfdir)/module.fax 644 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(sysconfdir)/faxcoverpage.tr.dist 644 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(sysconfdir)/faxrc.dist 640 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(sysconfdir)/faxnotifyrc.dist 644 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(moduledir)/courierfax 755 @mailuser@ @mailgroup@ >>permissions.dat
	@for f in $(filterbin_SCRIPTS) ; do echo $(filterbindir)/$$f 555 @mailuser@ @mailgroup@ >>permissions.dat ; done
	@echo @faxtmpdir@ 700 @mailuser@ @mailgroup@ >>permissions.dat

librewrite_la_SOURCES=faxstub.c
librewrite_la_LDFLAGS=-static

courierfax_SOURCES=courierfax.c faxconvert.c faxconvert.h
courierfax_DEPENDENCIES=@courierdir@/courier/libs/libmodule.la \
		librewrite.la \
		@courierdir@/courier/libs/libcommon.la \
		@courierdir@/courier/libs/libcourier.la \
		@courierdir@/libs/rfc2045/librfc2045.la \
		@courierdir@/libs/rfc822/librfc822.la \
		@courierdir@/libs/gpglib/libgpg.la \
		@courierdir@/libs/numlib/libnumlib.la \
		@courierdir@/libs/waitlib/libwaitlib.a

courierfax_LDADD=$(courierfax_DEPENDENCIES) -lcourier-unicode # -lefence

htmldocdir=@datadir@/htmldoc
htmldoc_DATA=courierfax.html
man_MANS=courierfax.8

if HAVE_SGML

GITFILES=faxrc.dist faxnotifyrc.dist faxcoverpage.tr.dist

BUILT_SOURCES += $(GITFILES)

$(GITFILES): %: %.git
	perl -p -e 's/\$$Id:[^\$$]*/\$$Id: '"`git log --pretty=format:\"%H\" -n 1 -- $<`-`date +%Y%m%d%H%M%S`/" <$< >$@.tmp
	mv -f $@.tmp $@

courierfax.html.in: courierfax.sgml ../../libs/docbook/sgml2html
	../../libs/docbook/sgml2html courierfax.sgml courierfax.html.in

courierfax.8.in: courierfax.sgml ../../libs/docbook/sgml2man
	../../libs/docbook/sgml2man courierfax.sgml courierfax.8.in "--stringparam man.base.url.for.relative.links http://www.courier-mta.org/"
	mv courierfax.8 courierfax.8.in

endif

courierfax.html: courierfax.html.in
	./config.status --file=$@

courierfax.8: courierfax.8.in
	./config.status --file=$@
