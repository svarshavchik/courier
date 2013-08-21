# Copyright 2000-2008 Double Precision, Inc.  See COPYING for
# distribution information.

AUTOMAKE_OPTIONS=dist-bzip2

datascriptdir=$(pkgdatadir)
datascript_SCRIPTS=sysconftool sysconftoolize.pl sysconftoolcheck
datascript_DATA=sysconftoolize.am

acdir=@ACDIR@
ac_DATA=sysconftool.m4

man_MANS=sysconftool.1 sysconftoolcheck.1 sysconftool.7
noinst_DATA=sysconftool.1.html sysconftoolcheck.1.html sysconftool.7.html

CLEANFILES=$(man_MANS) $(noinst_DATA)

EXTRA_DIST= \
  sysconftool.1.html.in sysconftool.7.html.in sysconftoolcheck.1.html.in \
  sysconftool.1.in sysconftool.7.in sysconftoolcheck.1.in \
  testsuite testsuite.txt sysconftool.spec sysconftoolize.am index.html \
  COPYING COPYING.GPL


if HAVE_SGML
sysconftool.1.html.in: sysconftool1.sgml docbook/sgml2html
	docbook/sgml2html sysconftool1.sgml sysconftool.1.html.in

sysconftool.1.in: sysconftool1.sgml docbook/sgml2man
	docbook/sgml2man sysconftool1.sgml sysconftool.1.in
	mv sysconftool.1 sysconftool.1.in

sysconftool.7.html.in: sysconftool7.sgml docbook/sgml2html
	docbook/sgml2html sysconftool7.sgml sysconftool.7.html.in

sysconftool.7.in: sysconftool7.sgml docbook/sgml2man
	docbook/sgml2man sysconftool7.sgml sysconftool.7.in
	mv sysconftool.7 sysconftool.7.in

sysconftoolcheck.1.html.in: sysconftoolcheck.sgml docbook/sgml2html
	docbook/sgml2html sysconftoolcheck.sgml sysconftoolcheck.1.html.in

sysconftoolcheck.1.in: sysconftoolcheck.sgml docbook/sgml2man
	docbook/sgml2man sysconftoolcheck.sgml sysconftoolcheck.1.in
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
	$(LN_S) $(datadir)/sysconftool/sysconftoolize.pl $(DESTDIR)$(bindir)/sysconftoolize
	$(LN_S) $(datadir)/sysconftool/sysconftoolcheck $(DESTDIR)$(bindir)/sysconftoolcheck

uninstall-local:
	rm -f $(DESTDIR)$(bindir)/sysconftoolize $(DESTDIR)$(bindir)/sysconftoolcheck

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
		--target noarch sysconftool-@VERSION@.tar.bz2

bump.rpm.release: sysconftool.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{?dist}/' \
		<sysconftool.spec >sysconftool.spec.new; \
	mv sysconftool.spec.new sysconftool.spec

distrelease:
	$(MAKE) rpm
	rm -f sysconftool.spec
	$(MAKE)
	$(MAKE)
	$(MAKE) dist
	$(MAKE) dorpm

upload-beta:
	test -z "$(SFPASSWD)" && exit 1; exit 0
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam $(SFPASSWD) courier @PACKAGE@-devel `echo @VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig

upload-dist:
	test -z "$(SFPASSWD)" && exit 1; exit 0
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	git tag -s @PACKAGE@/@VERSION@/`date +%Y%m%d%H%M%S` -m 'Tag version @VERSION@'
	socksify ~/src/sftools/release.pl mrsam $(SFPASSWD) courier @PACKAGE@ @VERSION@ @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig


release-dist:
	test -z "$(SFPASSWD)" && exit 1; exit 0
	./changelog.pl >changelog.txt
	emacs releasenotes.txt
	test -f releasenotes.txt || exit 1
	socksify ~/src/sftools/release.pl mrsam $(SFPASSWD) courier @PACKAGE@ @VERSION@ releasenotes.txt changelog.txt @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig
	socksify ssh courier.sourceforge.net rm -f /home/groups/c/co/courier/@PACKAGE@.beta

htmldoc: $(noinst_DATA) index.html
	for f in $^; do d=`basename $$f`; tidy -i -q --tidy-mark no --doctype transitional --input-encoding utf8 --output-encoding utf8 -asxml <$$f >$$f.tmp ; xsltproc --nonet $$HOME/www/www.courier-mta.org/xsl/sysconftool.xsl $$f.tmp  >html/$$d; rm -f $$f.tmp; done
