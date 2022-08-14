# Copyright 2004-2022 Double Precision, Inc.
# See COPYING for distribution information.
#
AUTOMAKE_OPTIONS=dist-bzip2

SUBDIRS=libs/soxwrap
AM_CPPFLAGS=$(LTDLINCL) @COURIERAUTHINCLUDE@

bin_SCRIPTS=socksify mkbl4
libexec_PROGRAMS=sockd
BUILT_SOURCES=configfile.h \
		socksify.html socksrc.html sockd.html mkbl4.html libsocks.sym
include_HEADERS=socks.h

lib_LTLIBRARIES=libsocks.la libsockswrap.la
noinst_PROGRAMS=testconnect testbind testcidr
noinst_SCRIPTS=testcidr.sh sockd.rc

install-exec-local: sockd.rc
	$(mkinstalldirs) $(DESTDIR)@sbindir@ || :
	$(INSTALL) -m 755 sockd.rc $(DESTDIR)@sbindir@/sockd

libsocks_la_SOURCES=sox.c connectfunc.h stubfuncs.h parseconfigfile.h toipv4.h
libsocks_la_LIBADD=@NETLIBS@ @DLLIB@
libsocks_la_DEPENDENCIES=libsocks.sym
libsocks_la_LDFLAGS=-export-symbols libsocks.sym -version-info @LIBVERSION_INFO@

libsocks.sym: socks.h
	@SED@ -n '1,/__BEGIN__/d;/__END__/,$$d;/(/p' <socks.h | \
		@SED@ 's/(.*//;s/.* //' >libsocks.sym

man_MANS=socksify.1 socksrc.5 socks.3 sockd.1 mkbl4.8

configfile.h: config.status
	echo '#define CONFIGFILE "@sysconfdir@/socksrc"' >configfile.h

CLEANFILES = ${BUILT_SOURCES} ${man_MANS} socksify

libsockswrap_la_SOURCES=sockswrap.c
libsockswrap_la_LIBADD=libsocks.la

sockd_SOURCES=sockd.c toipv4.h
sockd_DEPENDENCIES=libs/soxwrap/libmksocket.a
sockd_LDADD=$(sockd_DEPENDENCIES) -lcourierauth -lgdbm
sockd_LDFLAGS=@COURIERAUTHLINK@


testconnect_SOURCES=testconnect.c printaddr.h
testconnect_LDADD=libsocks.la
testconnect_LDFLAGS=-static

testbind_SOURCES=testbind.c printaddr.h
testbind_LDADD=libsocks.la

testcidr_SOURCES=testcidr.c cidr.h

EXTRA_DIST=socksify.html.in socksify.1.in socksrc.html.in socksrc.5.in \
	socksify.in socks.html socks.3 courier-sox.spec \
	sockd.html.in sockd.1.in mkbl4.html.in mkbl4.8.in \
	pgpkeys.txt

install-data-hook:
	${mkinstalldirs} $(DESTDIR)@sysconfdir@/socksrc || :
	$(INSTALL) system $(DESTDIR)@sysconfdir@/socksrc/system
	chmod 755 $(DESTDIR)@sysconfdir@/socksrc
	chmod 644 $(DESTDIR)@sysconfdir@/socksrc/system

uninstall-local:
	rm -f $(DESTDIR)@sysconfdir@/socksrc/system
	rm -f $(DESTDIR)@sbindir@/sockd

if HAVE_SGML
socksify.html.in: socksify.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html socksify.sgml socksify.html.in

socksify.1.in: socksify.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man socksify.sgml socksify.1.in
	mv socksify.1 socksify.1.in

socksrc.html.in: socksrc.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html socksrc.sgml socksrc.html.in

socksrc.5.in: socksrc.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man socksrc.sgml socksrc.5.in
	mv socksrc.5 socksrc.5.in

socks.html: socks.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html socks.sgml socks.html

socks.3: socks.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man socks.sgml socks.3

sockd.html.in: sockd.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html sockd.sgml sockd.html.in

sockd.1.in: sockd.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man sockd.sgml sockd.1.in
	mv sockd.1 sockd.1.in

mkbl4.html.in: mkbl4.sgml libs/docbook/sgml2html
	libs/docbook/sgml2html mkbl4.sgml mkbl4.html.in

mkbl4.8.in: mkbl4.sgml libs/docbook/sgml2man
	libs/docbook/sgml2man mkbl4.sgml mkbl4.8.in
	mv mkbl4.8 mkbl4.8.in
endif

socksify.1: socksify.1.in
	./config.status --file=$@

socksify.html: socksify.html.in
	./config.status --file=$@

sockd.1: sockd.1.in
	./config.status --file=$@

sockd.html: sockd.html.in
	./config.status --file=$@

socksrc.5: socksrc.5.in
	./config.status --file=$@

socksrc.html: socksrc.html.in
	./config.status --file=$@

mkbl4.8: mkbl4.8.in
	./config.status --file=$@

mkbl4.html: mkbl4.html.in
	./config.status --file=$@

socksify: socksify.in libsockswrap.la libsocks.la
	./config.status --file=$@
	. ./libsockswrap.la || exit 1 ; LIBNAME="$${dlname}"; . ./libsocks.la || exit 1 ; LIBNAME="$$LIBNAME $${dlname}"; sed 's/@LIBNAME@/'"$$LIBNAME"'/' <socksify >socksify.tmp
	mv socksify.tmp socksify

check-am:
	@SHELL@ ./testcidr.sh

DATE := $(shell date -R)

dist-hook:
	rm -f $(distdir)/socks.h
	chmod +x $(distdir)/courier-debuild
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog.in
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog

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

bump.rpm.release: @PACKAGE_NAME@.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=100; \
	test "$$VERSION" = "@PACKAGE_VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@PACKAGE_VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{?dist}%{courier_release}/' \
		<courier-sox.spec >courier-sox.spec.new; \
	mv courier-sox.spec.new courier-sox.spec;

EXTRA_DIST += courier-debuild \
	packaging/debian/auto_install.sh.in \
	packaging/debian/changelog \
	packaging/debian/changelog.in \
	packaging/debian/control \
	packaging/debian/copyright \
	packaging/debian/libcourier-sox.lintian-overrides \
	packaging/debian/libcourier-sox.soname.lintian-overrides \
	packaging/debian/rules \
	packaging/debian/source/format

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
	~/src/sftools/release.pl mrsam courier sox-devel `echo @PACKAGE_VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig

distrelease:
	umask 022; $(MAKE) distcheck
	$(MAKE) rpm
	$(MAKE)
	$(MAKE)
	rm -f @PACKAGE_NAME@.spec packaging/debian/changelog
	$(MAKE) @PACKAGE_NAME@.spec packaging/debian/changelog
	$(MAKE) dist

upload-dist:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	set -e; v="@PACKAGE_NAME@/@PACKAGE_VERSION@/`date +%Y%m%d%H%M%S`"; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'; cd libs; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'
	socksify ~/src/sftools/release.pl mrsam courier sox @PACKAGE_VERSION@ @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig
