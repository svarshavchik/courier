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
		$(RPMFLAGS) @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2
	$$HOME/bin/rpmrepos.sh

bump.rpm.release: courier-analog.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@PACKAGE_VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@PACKAGE_VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{dist}/' \
		<courier-analog.spec >courier-analog.spec.new; \
	mv courier-analog.spec.new courier-analog.spec

# deb packaging

EXTRA_DIST += courier-debuild \
	packaging/debian/changelog \
	packaging/debian/changelog.in \
	packaging/debian/control \
	packaging/debian/copyright \
	packaging/debian/courier-analog.lintian-overrides \
	packaging/debian/rules \
	packaging/debian/source/format

packaging/debian/changelog: packaging/debian/changelog.in configure.ac
	CONFIG_FILES=packaging/debian/changelog $(srcdir)/config.status

deb: bump.deb.release
	$(MAKE) dist
	rm -rf @PACKAGE@-@VERSION@ deb
	tar xf @PACKAGE@-@VERSION@.tar.bz2
	cd @PACKAGE@-@VERSION@ && chmod +x ./courier-debuild && ./courier-debuild -us -uc && mv deb ../deb
	rm -rf @PACKAGE@-@VERSION@
	$$HOME/bin/debrepos.sh

bump.deb.release: packaging/debian/changelog
	VERSION="x"; \
	test -f $(srcdir)/deb.release && . $(srcdir)/deb.release; \
	NEXT_RELEASE=100; \
	test "$$VERSION" = "@PACKAGE_VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@PACKAGE_VERSION@" >$(srcdir)/deb.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/deb.release; \
	sed 's/^@PACKAGE@.*/@PACKAGE@ (@VERSION@-'$$RELEASE') devel; urgency=medium/' \
		<packaging/debian/changelog >packaging/debian/changelog.new; \
	mv packaging/debian/changelog.new packaging/debian/changelog

.PHONE: deb rpm

DATE := $(shell date -R)
dist-hook:
	chmod +x $(distdir)/courier-debuild
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog.in
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog

upload-beta:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam courier analog-devel `echo @PACKAGE_VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig

distrelease:
	$(MAKE) -$(MAKEFLAGS) distcheck
	$(MAKE) -$(MAKEFLAGS) rpm
	rm -f courier-analog.spec packaging/debian/changelog
	$(MAKE) -$(MAKEFLAGS) courier-analog.spec packaging/debian/changelog
	$(MAKE) -$(MAKEFLAGS) dist

upload-dist:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	git tag -s @PACKAGE_NAME@/@PACKAGE_VERSION@/`date +%Y%m%d%H%M%S` -m 'Tag version @PACKAGE_VERSION@'
	~/src/sftools/release.pl mrsam courier analog @PACKAGE_VERSION@ @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig
