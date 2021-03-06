#
# Copyright 2004 Double Precision, Inc.  See COPYING for
# distribution information.

AUTOMAKE_OPTIONS=dist-bzip2

bin_SCRIPTS=courier-analog
man_MANS=courier-analog.8
noinst_DATA=courier-analog.html manpage.css

EXTRA_DIST=courier-analog.8 courier-analog.html manpage.css courier-analog.spec COPYING COPYING.GPL

if HAVE_SGML
courier-analog.html: courier-analog.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html courier-analog.sgml courier-analog.html

courier-analog.8: courier-analog.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man courier-analog.sgml courier-analog.8

manpage.css: libs/docbook/manpage.css
	cp libs/docbook/manpage.css .
endif

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
		$(RPMFLAGS) @PACKAGE@-@VERSION@.tar.bz2

bump.rpm.release: courier-analog.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{dist}/' \
		<courier-analog.spec >courier-analog.spec.new; \
	mv courier-analog.spec.new courier-analog.spec

upload-beta:
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam courier analog-devel `echo @VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig

distrelease:
	$(MAKE) -$(MAKEFLAGS) distcheck
	$(MAKE) -$(MAKEFLAGS) rpm
	$(MAKE) -$(MAKEFLAGS) courier-analog.spec
	$(MAKE) -$(MAKEFLAGS) dist

upload-dist:
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	git tag -s @PACKAGE@/@VERSION@/`date +%Y%m%d%H%M%S` -m 'Tag version @VERSION@'
	~/src/sftools/release.pl mrsam courier analog @VERSION@ @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig
