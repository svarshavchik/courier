#
# Copyright 1998 - 2022 Double Precision, Inc.  See COPYING for
# distribution information.

@SET_MAKE@

AUTOMAKE_OPTIONS=dist-bzip2

DISTCHECK_CONFIGURE_FLAGS=--enable-maildirquota --enable-userdb

HTML2TXT=elinks -dump -no-numbering -no-references

SUBDIRS=libs/numlib libs/liblock libs/rfc822 libs/rfc2045 @DBSUBDIR@ libs/makedat libs/maildir libs/maildrop

DIST_SUBDIRS=libs/numlib libs/liblock libs/rfc822 libs/rfc2045 libs/gdbmobj libs/bdbobj libs/makedat libs/maildir libs/maildrop

HTMLWWW=index.html documentation.html download.html links.html
EXTRA_DIST=README.html INSTALL.html UPGRADE.html README.postfix\
	README.dovecotauth \
	maildroptips.txt COPYING.GPL manpage.css $(HTMLWWW)


if HAVE_SGML
manpage.css: libs/docbook/manpage.css
	cp -f $(srcdir)/libs/docbook/manpage.css manpage.css
endif

scriptdir=$(pkgdatadir)/scripts
htmldir=$(docdir)/html

DELIVERQUOTAHTMLSOURCE=maildirquota.html deliverquota.html

DELIVERQUOTAHTML=$(DELIVERQUOTAHTMLSOURCE)
DELIVERQUOTAMAN=maildirquota.7 deliverquota.8

#  Install deliverquota into libdir/bin
#
#  maildrop < 0.74 installed bindir/maildrop.deliverquota and created a
#  soft link in bindir from deliverquota to maildrop.deliverquota
#
#  remove maildrop 0.74 installlation, if present.  Starting with maildrop 0.75
#  it is expected that the default login shell will add libdir/bin to PATH,
#  where deliverquota will be installed.
#  Create a soft link from bindir/deliverquota to libdir/bin/deliverquota

install-deliverquota:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) libs/maildir/deliverquota $(DESTDIR)$(bindir)/deliverquota
	cd $(DESTDIR)$(bindir); rm -f maildrop.deliverquota

uninstall-deliverquota:
	rm -f $(DESTDIR)$(bindir)/deliverquota

MAILDROPHTML=lockmail.html maildiracl.html maildirkw.html \
	maildirwatch.html \
	maildirmake.html maildropex.html \
	maildir.html \
	maildropfilter.html maildropgdbm.html maildrop.html mailbot.html \
	makemime.html reformail.html reformime.html

if HAVE_DBLIBRARY
MAILDROPHTML += makedat.html
endif

MAILDROPMAN=lockmail.1 maildirmake.1 maildrop.1 mailbot.1 maildropex.7 \
	maildirkw.1 maildirwatch.1 \
	maildir.5 \
	maildropfilter.7 maildropgdbm.7 makemime.1 reformail.1 reformime.1

if HAVE_DBLIBRARY
MAILDROPMAN += makedat.1
endif

EXTRA_LTLIBRARIES=librfc822.la librfc2045.la

if DEVEL
DEVEL_MAN=rfc822.3 rfc2045.3
endif

sysincludedir=$(includedir)
sysinclude_DATA=@DEVEL_INCLUDES@

lib_LTLIBRARIES=@DEVEL_LIBS@

htmldataclean=$(DELIVERQUOTAHTML) \
	$(MAILDROPHTML) rfc822.html rfc2045.html

html_DATA=$(htmldataclean) manpage.css

if !HAVE_DBLIBRARY
html_DATA += makedat.html
endif

man_MANS=$(DELIVERQUOTAMAN) \
	$(MAILDROPMAN) $(DEVEL_MAN)

if !HAVE_DBLIBRARY
man_MANS += makedat.1
endif

DISTCLEANFILES=$(htmldataclean) $(man_MANS)

librfc822_la_SOURCES=
librfc2045_la_SOURCES=

librfc822_la_LIBADD=libs/rfc822/librfc822.la libs/rfc822/libencode.la
librfc2045_la_LIBADD=libs/rfc2045/librfc2045.la
librfc822_la_LDFLAGS=-rpath $(libdir) -version-info @LIBVERSION_INFO@
librfc2045_la_LDFLAGS=-rpath $(libdir) -version-info @LIBVERSION_INFO@

install-data-local: install-man install-deliverquota install-maildrop

uninstall-local: uninstall-deliverquota uninstall-maildrop

deliverquota.8: libs/maildir/deliverquota.8
	cp -f libs/maildir/deliverquota.8 deliverquota.8

deliverquota.html: libs/maildir/deliverquota.html
	cp -f libs/maildir/deliverquota.html deliverquota.html

#  In maildrop 0.74, maildirmake was installed as bindir/maildrop.maildirmake
#  and a soft link was created from maildirmake to maildrop.maildirmake
#
#  Starting with maildrop 0.75, maildirmake gets installed into libdir/bin,
#  and install-exec-local will remove maildrop 0.74's maildirmake, and create
#  a soft link.
#  It is expected that the default login script will add libdir/bin to PATH
#
#  Same thing goes for deliverquota and maildrop.deliverquota

install-maildrop:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	for f in maildrop mailbot reformail ; do \
		$(INSTALL_PROGRAM) libs/maildrop/$$f $(DESTDIR)$(bindir)/$$f ; \
	done
	$(INSTALL_PROGRAM) libs/liblock/lockmail $(DESTDIR)$(bindir)/lockmail
	$(INSTALL_PROGRAM) libs/maildir/maildirmake $(DESTDIR)$(bindir)/maildirmake
	$(INSTALL_PROGRAM) libs/maildir/maildirkw $(DESTDIR)$(bindir)/maildirkw
	$(INSTALL_PROGRAM) libs/maildir/maildirwatch $(DESTDIR)$(bindir)/maildirwatch
	$(INSTALL_PROGRAM) libs/rfc2045/reformime $(DESTDIR)$(bindir)/reformime
	$(INSTALL_PROGRAM) libs/rfc2045/makemime $(DESTDIR)$(bindir)/makemime
	test "@USE_DB@@USE_GDBM@" = "00" && exit 0; \
		$(INSTALL_PROGRAM) libs/makedat/makedatprog $(DESTDIR)$(bindir)/makedatprog
	test "@USE_DB@@USE_GDBM@" = "00" && exit 0; \
		$(INSTALL_SCRIPT)  libs/makedat/makedat     $(DESTDIR)$(bindir)/makedat
	. libs/maildrop/uidgid ; test -z "$$gid" && exit 0; test -w /etc || exit 0; cd $(DESTDIR)$(bindir) && chgrp $$gid maildrop lockmail
	. libs/maildrop/uidgid ; test -z "$$uid" && exit 0; test -w /etc || exit 0; cd $(DESTDIR)$(bindir) && chown $$uid maildrop lockmail && chmod 755 maildrop lockmail

uninstall-maildrop:
	for f in maildrop mailbot maildirmake reformail lockmail makemime reformime maildirkw maildirwatch; do \
		rm -f $(DESTDIR)$(bindir)/$$f ; \
	done
	test "@USE_DB@@USE_GDBM@" = "00" && exit 0; \
		rm -f $(DESTDIR)$(bindir)/makedat ; \
		rm -f $(DESTDIR)$(bindir)/makedatprog


all-recursive: all-make-check

all-make-check:
	$(MAKE) all-gmake-check FOO=BAR

all-gmake-check:
	@test "$(FOO)" = "BAR" && exit 0 ; echo "" ; \
		echo "ERROR: Something's wrong.  If you're using xBSD, you must use gmake" ; \
		echo "instead of make, as you were told by the INSTALL file (you did read the" ; \
		echo "INSTALL file, did you?)" ; echo "" ; exit 1

check-am:
	cd libs/maildrop; $(MAKE) check-version TOPLEVEL_VERSION="`echo $(VERSION) | cut -d. -f1-3`"

makedat.html: libs/maildrop/makedat.html
	cp -f $(srcdir)/libs/maildrop/makedat.html makedat.html

makedat.1: libs/maildrop/makedat.1
	cp -f $(srcdir)/libs/maildrop/makedat.1 makedat.1

reformime.html: $(srcdir)/libs/rfc2045/reformime.html
	cp -f $(srcdir)/libs/rfc2045/reformime.html reformime.html

reformime.1: $(srcdir)/libs/rfc2045/reformime.1
	cp -f $(srcdir)/libs/rfc2045/reformime.1 reformime.1

makemime.html: $(srcdir)/libs/rfc2045/makemime.html
	cp -f $(srcdir)/libs/rfc2045/makemime.html makemime.html

makemime.1: $(srcdir)/libs/rfc2045/makemime.1
	cp -f $(srcdir)/libs/rfc2045/makemime.1 makemime.1

lockmail.html: libs/liblock/lockmail.html
	cp -f $(srcdir)/libs/liblock/lockmail.html lockmail.html

lockmail.1: libs/liblock/lockmail.1
	cp -f $(srcdir)/libs/liblock/lockmail.1 lockmail.1

maildir.html: libs/maildir/maildir.html
	cp -f $(srcdir)/libs/maildir/maildir.html maildir.html

maildir.5: libs/maildir/maildir.5
	cp -f $(srcdir)/libs/maildir/maildir.5 maildir.5

maildiracl.html: libs/maildir/maildiracl.html
	cp -f libs/maildir/maildiracl.html maildiracl.html

maildirkw.html: libs/maildir/maildirkw.html
	cp -f $(srcdir)/libs/maildir/maildirkw.html maildirkw.html

maildirwatch.html: libs/maildir/maildirwatch.html
	cp -f $(srcdir)/libs/maildir/maildirwatch.html maildirwatch.html

maildirmake.html: libs/maildir/maildirmake.html
	cp -f libs/maildir/maildirmake.html maildirmake.html

maildirmake.1: libs/maildir/maildirmake.1
	cp -f libs/maildir/maildirmake.1 .

maildirkw.1: libs/maildir/maildirkw.1
	cp -f $(srcdir)/libs/maildir/maildirkw.1 .

maildirwatch.1: libs/maildir/maildirwatch.1
	cp -f $(srcdir)/libs/maildir/maildirwatch.1 .

maildropex.html: libs/maildrop/maildropex.html
	cp -f $(srcdir)/libs/maildrop/maildropex.html maildropex.html

maildropex.7: libs/maildrop/maildropex.7
	cp -f $(srcdir)/libs/maildrop/maildropex.7 maildropex.7

maildropfilter.html: libs/maildrop/maildropfilter.html
	cp -f libs/maildrop/maildropfilter.html maildropfilter.html

maildropfilter.7: libs/maildrop/maildropfilter.7
	cp -f libs/maildrop/maildropfilter.7 maildropfilter.7

maildropgdbm.html: libs/maildrop/maildropgdbm.html
	cp -f $(srcdir)/libs/maildrop/maildropgdbm.html maildropgdbm.html

maildropgdbm.7: libs/maildrop/maildropgdbm.7
	cp -f $(srcdir)/libs/maildrop/maildropgdbm.7 maildropgdbm.7

maildrop.html: libs/maildrop/maildrop.html
	cp -f libs/maildrop/maildrop.html maildrop.html

maildrop.1: libs/maildrop/maildrop.1
	cp -f libs/maildrop/maildrop.1 maildrop.1

mailbot.html: libs/maildrop/mailbot.html
	cp -f $(srcdir)/libs/maildrop/mailbot.html mailbot.html

mailbot.1: libs/maildrop/mailbot.1
	cp -f $(srcdir)/libs/maildrop/mailbot.1 mailbot.1

reformail.html: libs/maildrop/reformail.html
	cp -f $(srcdir)/libs/maildrop/reformail.html reformail.html

reformail.1: libs/maildrop/reformail.1
	cp -f $(srcdir)/libs/maildrop/reformail.1 reformail.1

maildirquota.html: libs/maildir/maildirquota.html
	cp -f $(srcdir)/libs/maildir/maildirquota.html maildirquota.html

maildirquota.7: libs/maildir/maildirquota.7
	cp -f $(srcdir)/libs/maildir/maildirquota.7 maildirquota.7

rfc822.h: libs/rfc822/rfc822.h
	cp -f $(srcdir)/libs/rfc822/rfc822.h .

rfc2047.h: libs/rfc822/rfc2047.h
	cp -f $(srcdir)/libs/rfc822/rfc2047.h .

rfc2045.h: libs/rfc2045/rfc2045.h
	cp -f $(srcdir)/libs/rfc2045/rfc2045.h .

rfc2045.3: libs/rfc2045/rfc2045.3
	cp -f $(srcdir)/libs/rfc2045/rfc2045.3 .

rfc822.html: libs/rfc822/rfc822.html
	cp -f $(srcdir)/libs/rfc822/rfc822.html .

rfc822.3: libs/rfc822/rfc822.html
	cp -f $(srcdir)/libs/rfc822/rfc822.3 .

rfc2045.html: libs/rfc2045/rfc2045.html
	cp -f $(srcdir)/libs/rfc2045/rfc2045.html .

CLEANFILES=INSTALL.txt UPGRADE.txt

INSTALL.txt: INSTALL.html
	$(HTML2TXT) $(srcdir)/INSTALL.html >INSTALL.txt

UPGRADE.txt: UPGRADE.html
	$(HTML2TXT) $(srcdir)/UPGRADE.html >UPGRADE.txt

maildroptips.txt: libs/maildrop/maildroptips.html
	$(HTML2TXT) libs/maildrop/maildroptips.html >maildroptips.txt

DATE := $(shell date -R)

dist-hook: INSTALL.txt UPGRADE.txt maildrop.spec
	rm -f $(distdir)/INSTALL $(distdir)/UPGRADE $(distdir)/README
	$(HTML2TXT) $(srcdir)/README.html >$(distdir)/README
	cp INSTALL.txt $(distdir)/INSTALL
	cp UPGRADE.txt $(distdir)/UPGRADE
	cp maildrop.spec $(distdir)
	chmod +x $(distdir)/courier-debuild
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog.in
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog

EXTRA_DIST += courier-debuild \
	packaging/debian/auto_install.sh.in \
	packaging/debian/changelog \
	packaging/debian/changelog.in \
	packaging/debian/control \
	packaging/debian/copyright \
	packaging/debian/lintian-overrides.allow-errors \
	packaging/debian/maildrop.lintian-overrides \
	packaging/debian/maildrop-dev.lintian-overrides \
	packaging/debian/maildrop-dev.soname.lintian-overrides \
	packaging/debian/rules \
	packaging/debian/source/format \
	packaging/debian/source/lintian-overrides

packaging/debian/changelog: packaging/debian/changelog.in configure.ac
	CONFIG_FILES=packaging/debian/changelog $(srcdir)/config.status

deb: bump.deb.release
	$(MAKE) dist
	rm -rf @PACKAGE@-@VERSION@ deb
	tar xf @PACKAGE@-@VERSION@.tar.bz2
	cd @PACKAGE@-@VERSION@ && ./courier-debuild -us -uc && mv deb ../deb && cd .. && rm -rf @PACKAGE@-@VERSION@
	$$HOME/bin/debrepos.sh

deb-pbuild: bump.deb.release
	@test -z "$(PBUILD)" || exit 0; echo "Set PBUILD please"; exit 1
	. deb.release; \
	R="`echo $(PBUILD) | tr '.' '+'`"; \
	sed 's/^\([^ ][^ ]*\).*/\1 (@VERSION@-'$$RELEASE'+'$$R') devel; urgency=medium/' \
		<packaging/debian/changelog >packaging/debian/changelog.new; \
	mv packaging/debian/changelog.new packaging/debian/changelog
	$(MAKE) dist
	rm -rf @PACKAGE@-@VERSION@ deb/pbuild
	tar xf @PACKAGE@-@VERSION@.tar.bz2
	cd @PACKAGE@-@VERSION@ && chmod +x ./courier-debuild && DEBGCC=default ./courier-debuild -us -uc && mv deb ../deb/pbuild
	rm -rf @PACKAGE@-@VERSION@

bump.deb.release: packaging/debian/changelog
	VERSION="x"; \
	test -f $(srcdir)/deb.release && . $(srcdir)/deb.release; \
	NEXT_RELEASE=100; \
	test "$$VERSION" = "@PACKAGE_VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@PACKAGE_VERSION@" >$(srcdir)/deb.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/deb.release; \
	. /etc/lsb-release; \
	R=`echo $$DISTRIB_RELEASE | tr '.' '+'`; \
	sed 's/^\([^ ][^ ]*\).*/\1 (@VERSION@-'$$RELEASE'+'$$R') devel; urgency=medium/' \
		<packaging/debian/changelog >packaging/debian/changelog.new; \
	mv packaging/debian/changelog.new packaging/debian/changelog

#
#  For maintainer's use only
#
distrelease:
	umask 022; $(MAKE) distcheck
	$(MAKE) rpm
	$(MAKE)
	$(MAKE)
	$(MAKE) dist
	$(MAKE) htmldoc

dollar=$$
id=$(dollar)Id

WWW=$$HOME/www/hostrocket/courier-mta.org

htmldoc: $(DELIVERQUOTAHTMLSOURCE) $(htmldataclean) libs/maildrop/maildroptips.html $(HTMLWWW)
	for f in $^; do xsltproc --nonet $(WWW)/xsl/maildrop.xsl $$f | perl -ne 's~\$(id):.*\$$~\$(id):'"$$h"'\$$~; print' >$(WWW)/maildrop/`basename $$f`; done
	cp manpage.css html

rpm: rpm-build

rpm-build:
	$(MAKE) -$(MAKEFLAGS) bump.rpm.release
	$(MAKE) -$(MAKEFLAGS) dist
	$(MAKE) -$(MAKEFLAGS) dorpm

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
	$$HOME/bin/rpmrepos.sh

bump.rpm.release: maildrop.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@PACKAGE_VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@PACKAGE_VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{courier_release}/' \
		<maildrop.spec >maildrop.spec.new; \
	mv maildrop.spec.new maildrop.spec

upload-beta:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam courier maildrop-devel `echo @PACKAGE_VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig

upload-dist:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	set -e; v="@PACKAGE_NAME@/@PACKAGE_VERSION@/`date +%Y%m%d%H%M%S`"; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'; cd libs; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'
	socksify ~/src/sftools/release.pl mrsam courier maildrop @PACKAGE_VERSION@ @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig
