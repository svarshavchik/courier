# Copyright 2000-2022 S. Varshavchik.  See COPYING for
# distribution information.

AUTOMAKE_OPTIONS=dist-bzip2

datascriptdir=$(pkgdatadir)
datascript_SCRIPTS=sysconftool sysconftoolize.pl sysconftoolcheck
datascript_DATA=sysconftoolize.am

acdir=@ACDIR@
ac_DATA=sysconftool.m4

man_MANS=sysconftool.1 sysconftoolize.1 sysconftoolcheck.1 sysconftool.7
noinst_DATA=sysconftool.1.html sysconftoolcheck.1.html sysconftool.7.html

CLEANFILES=$(man_MANS) $(noinst_DATA)

EXTRA_DIST= \
  sysconftool.1.html.in sysconftool.7.html.in sysconftoolcheck.1.html.in \
  sysconftool.1.in sysconftool.7.in sysconftoolcheck.1.in \
  testsuite testsuite.txt sysconftool.spec sysconftoolize.am index.html \
  COPYING COPYING.GPL

# .deb packaging

EXTRA_DIST += \
	sysconftoolize.1 \
	courier-debuild \
	packaging/debian/changelog \
	packaging/debian/changelog.in \
	packaging/debian/control \
	packaging/debian/copyright \
	packaging/debian/rules \
	packaging/debian/source/format

if HAVE_SGML
sysconftool.1.html.in: sysconftool1.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html sysconftool1.sgml sysconftool.1.html.in

sysconftool.1.stamp: sysconftool1.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man sysconftool1.sgml sysconftool.1.in "--stringparam man.base.url.for.relative.links https://www.courier-mta.org/sysconftool/"
	mv sysconftool.1 sysconftool.1.in
	touch sysconftool.1.stamp

sysconftool.1.in: sysconftool.1.stamp
	touch sysconftool.1.in

sysconftoolize.1: sysconftool.1.stamp
	touch sysconftoolize.1

sysconftool.7.html.in: sysconftool7.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html sysconftool7.sgml sysconftool.7.html.in

sysconftool.7.in: sysconftool7.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man sysconftool7.sgml sysconftool.7.in "--stringparam man.base.url.for.relative.links https://www.courier-mta.org/sysconftool/"
	mv sysconftool.7 sysconftool.7.in

sysconftoolcheck.1.html.in: sysconftoolcheck.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html sysconftoolcheck.sgml sysconftoolcheck.1.html.in

sysconftoolcheck.1.in: sysconftoolcheck.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man sysconftoolcheck.sgml sysconftoolcheck.1.in "--stringparam man.base.url.for.relative.links https://www.courier-mta.org/sysconftool/"
	mv sysconftoolcheck.1 sysconftoolcheck.1.in
endif

sysconftool.1.html: sysconftool.1.html.in
	CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

sysconftool.7.html: sysconftool.7.html.in
	CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

sysconftoolcheck.1.html: sysconftoolcheck.1.html.in
	CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

sysconftool.1: sysconftool.1.in
	CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

sysconftool.7: sysconftool.7.in
	CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

sysconftoolcheck.1: sysconftoolcheck.1.in
	CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status


check:
	@SHELL@ $(srcdir)/testsuite @PERL@ 2>&1 | cmp -s - $(srcdir)/testsuite.txt

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	rm -f $(DESTDIR)$(bindir)/sysconftoolize
	rm -f $(DESTDIR)$(bindir)/sysconftoolcheck
	$(LN_S) `realpath --relative-to $(bindir) -m $(datadir)/sysconftool/sysconftoolize.pl` $(DESTDIR)$(bindir)/sysconftoolize
	$(LN_S) `realpath --relative-to $(bindir) -m $(datadir)/sysconftool/sysconftoolcheck` $(DESTDIR)$(bindir)/sysconftoolcheck

uninstall-local:
	rm -f $(DESTDIR)$(bindir)/sysconftoolize $(DESTDIR)$(bindir)/sysconftoolcheck

DATE := $(shell date -R)
dist-hook:
	chmod +x $(distdir)/courier-debuild
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog.in
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog

rpm: rpm-build

rpm-build: sysconftool.spec
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
		--target noarch sysconftool-@PACKAGE_VERSION@.tar.bz2
	$$HOME/bin/rpmrepos.sh

bump.rpm.release: sysconftool.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=100; \
	test "$$VERSION" = "@PACKAGE_VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@PACKAGE_VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{?dist}/' \
		<sysconftool.spec >sysconftool.spec.new; \
	mv sysconftool.spec.new sysconftool.spec

distrelease:
	$(MAKE) rpm
	rm -f sysconftool.spec packaging/debian/changelog
	$(MAKE) sysconftool.spec packaging/debian/changelog
	$(MAKE) dist
	$(MAKE) dorpm

packaging/debian/changelog: packaging/debian/changelog.in configure.ac
	CONFIG_FILES=packaging/debian/changelog $(srcdir)/config.status

deb: bump.deb.release
	$(MAKE) dist
	rm -rf @PACKAGE@-@VERSION@ deb
	tar xf @PACKAGE@-@VERSION@.tar.bz2
	cd @PACKAGE@-@VERSION@ && chmod +x ./courier-debuild && ./courier-debuild -us -uc && mv deb ../deb
	rm -rf @PACKAGE@-@VERSION@
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
	cd @PACKAGE@-@VERSION@ && chmod +x ./courier-debuild && ./courier-debuild -us -uc && mv deb ../deb/pbuild
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

upload-beta:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam courier @PACKAGE_NAME@-devel `echo @PACKAGE_VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig

upload-dist:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	git tag -s @PACKAGE_NAME@/@PACKAGE_VERSION@/`date +%Y%m%d%H%M%S` -m 'Tag version @PACKAGE_VERSION@'
	socksify ~/src/sftools/release.pl mrsam courier @PACKAGE_NAME@ @PACKAGE_VERSION@ @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig

htmldoc: $(noinst_DATA) index.html
	rm -rf html
	mkdir html
	for f in $^; do d=`basename $$f`; tidy -i -q --tidy-mark no --doctype transitional --input-encoding utf8 --output-encoding utf8 -asxml <$$f >$$f.tmp ; xsltproc --nonet $$HOME/www/hostrocket/courier-mta.org/xsl/sysconftool.xsl $$f.tmp  >html/$$d; rm -f $$f.tmp; done
	mv html/* $$HOME/www/hostrocket/courier-mta.org/sysconftool
	rm -rf html

.PHONY: rpm deb
