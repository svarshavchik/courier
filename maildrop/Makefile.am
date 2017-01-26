#
# Copyright 1998 - 2009 Double Precision, Inc.  See COPYING for
# distribution information.

@SET_MAKE@

AUTOMAKE_OPTIONS=dist-bzip2

DISTCHECK_CONFIGURE_FLAGS=--enable-maildirquota --enable-userdb

HTML2TXT=`which lynx 2>/dev/null && echo "lynx -dump -nolist" && exit; echo "links -dump -no-numbering -no-references"`

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
	maildirmake.html maildropex.html \
	maildir.html \
	maildropfilter.html maildropgdbm.html maildrop.html mailbot.html \
	makemime.html reformail.html reformime.html

if HAVE_DBLIBRARY
MAILDROPHTML += makedat.html
endif

MAILDROPMAN=lockmail.1 maildirmake.1 maildrop.1 mailbot.1 maildropex.7 \
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
librfc822_la_LDFLAGS=-rpath $(libdir)
librfc2045_la_LDFLAGS=-rpath $(libdir)

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
	$(INSTALL_PROGRAM) libs/rfc2045/reformime $(DESTDIR)$(bindir)/reformime
	$(INSTALL_PROGRAM) libs/rfc2045/makemime $(DESTDIR)$(bindir)/makemime
	test "@USE_DB@@USE_GDBM@" = "00" && exit 0; \
		$(INSTALL_PROGRAM) libs/makedat/makedatprog $(DESTDIR)$(bindir)/makedatprog
	test "@USE_DB@@USE_GDBM@" = "00" && exit 0; \
		$(INSTALL_SCRIPT)  libs/makedat/makedat     $(DESTDIR)$(bindir)/makedat
	. libs/maildrop/uidgid ; test -z "$$gid" && exit 0; test -w /etc || exit 0; cd $(DESTDIR)$(bindir) && chgrp $$gid maildrop lockmail
	. libs/maildrop/uidgid ; test -z "$$uid" && exit 0; test -w /etc || exit 0; cd $(DESTDIR)$(bindir) && chown $$uid maildrop lockmail && chmod 755 maildrop lockmail

uninstall-maildrop:
	for f in maildrop mailbot maildirmake reformail lockmail makemime reformime; do \
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

maildirmake.html: libs/maildir/maildirmake.html
	cp -f libs/maildir/maildirmake.html maildirmake.html

maildirmake.1: libs/maildir/maildirmake.1
	cp -f libs/maildir/maildirmake.1 .

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

dist-hook: INSTALL.txt UPGRADE.txt maildrop.spec maildrop.lpspec
	rm -f $(distdir)/INSTALL $(distdir)/UPGRADE $(distdir)/README
	$(HTML2TXT) $(srcdir)/README.html >$(distdir)/README
	cp INSTALL.txt $(distdir)/INSTALL
	cp UPGRADE.txt $(distdir)/UPGRADE
	cp maildrop.spec maildrop.lpspec $(distdir)
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

WWW=$$HOME/www/www.courier-mta.org

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
		@PACKAGE@-@VERSION@.tar.bz2

bump.rpm.release: maildrop.spec maildrop.lpspec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{courier_release}/' \
		<maildrop.spec >maildrop.spec.new; \
	mv maildrop.spec.new maildrop.spec; \
	sed 's/^Release: .*/Release: '$$RELEASE'/' \
		<maildrop.lpspec >maildrop.lpspec.new; \
	mv maildrop.lpspec.new maildrop.lpspec

lpm: lpm-build

lpm-build:
	$(MAKE) -$(MAKEFLAGS) bump.rpm.release
	$(MAKE) -$(MAKEFLAGS) dist
	rm -rf lpm
	mkdir lpm
	lpbuild `test "@REPOSITORY@" == "" || echo --define "repository=1"` --srcbuild -o lpm @PACKAGE@-@VERSION@.tar.bz2

publish: publish-@REPOSITORY@

publish-dev:
	$(MAKE) -$(MAKEFLAGS) dopublish SUBDIR=dev

publish-prod:
	$(MAKE) -$(MAKEFLAGS) dopublish SUBDIR=prod

dopublish:
	lpbuild --resign lpm/*
	rm -rf lpmtmp
	mkdir lpmtmp
	ln lpm/* lpmtmp
	rm -f lpmtmp/*debuginfo*
	mv lpmtmp/*.src.lp repository-$(SUBDIR)/`lpm -q --distribution`/sources
	mv lpmtmp/*.lp repository-$(SUBDIR)/`lpm -q --distribution`/packages
	rm -rf lpmtmp
	ls -t repository-$(SUBDIR)/`lpm -q --distribution`/sources | tail +6 | xargs -r -n 1 lpm --erase repository-$(SUBDIR)
	lpm --sync repository-$(SUBDIR)
	rsync -a -v --delete-after repository-$(SUBDIR)/`lpm -q --distribution`/. headache:lpmtool.com/www/repositories/$(SUBDIR)/libs/maildrop/`lpm -q --distribution`

upload-beta:
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam courier maildrop-devel `echo @VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig

upload-dist:
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	set -e; v="@PACKAGE@/@VERSION@/`date +%Y%m%d%H%M%S`"; git tag -s $$v -m 'Tag version @VERSION@'; cd libs; git tag -s $$v -m 'Tag version @VERSION@'
	socksify ~/src/sftools/release.pl mrsam courier maildrop @VERSION@ @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig
