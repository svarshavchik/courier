#
# Copyright 1998 - 2018 Double Precision, Inc.  See COPYING for
# distribution information.

@SET_MAKE@

AUTOMAKE_OPTIONS=dist-bzip2
AM_CXXFLAGS=@COURIER_UNICODE_CXXFLAGS@
AM_DISTCHECK_CONFIGURE_FLAGS=--with-notice=unicode
SUBDIRS=afx @DBSUBDIR@ libs/numlib libs/soxwrap libs/md5 libs/sha1 \
	libs/libhmac libs/random128 libs/rfc822 libs/rfc1035 \
	libs/rfc2045 libs/liblock libs/maildir libs/waitlib libs/tcpd \
	libs/threadlib libs/makedat libs/rootcerts libs/imap libs/maildrop \
	libs/cgi libs/http11 libs/ldapaddressbook libs/gpglib libs/pcp \
	libs/sqwebmail webadmin courier

DIST_SUBDIRS=afx libs/gdbmobj libs/bdbobj libs/numlib libs/soxwrap \
	libs/md5 libs/sha1 libs/libhmac libs/random128 \
	libs/rfc822 libs/rfc1035 libs/rfc2045 libs/liblock libs/maildir \
	libs/waitlib libs/tcpd libs/threadlib libs/makedat libs/rootcerts \
	libs/imap libs/maildrop libs/cgi libs/http11 libs/ldapaddressbook \
	libs/gpglib libs/pcp libs/sqwebmail webadmin courier

INSTALL: courier/doc/install.html
	rm -f INSTALL
	elinks -dump -no-numbering -no-references courier/doc/install.html >INSTALL.tmp
	mv -f INSTALL.tmp INSTALL

NEWS: NEWS.html
	elinks -dump -no-numbering -no-references NEWS.html >NEWS.tmp
	mv NEWS.tmp NEWS

EXTRA_DIST=BENCHMARKS sysconftool ChangeLog.2001 \
	COPYING.GPL NEWS.html pgpkeys.txt config.rpath

DISTCLEANFILES=version configure.args

EXTRA_DIST += \
	courier-debuild \
	packaging/debian/auto_install.sh.in \
	packaging/debian/changelog \
	packaging/debian/changelog.in \
	packaging/debian/control \
	packaging/debian/copyright \
	packaging/debian/courier.lintian-overrides \
	packaging/debian/courier-fax.lintian-overrides \
	packaging/debian/courier-imapd.lintian-overrides \
	packaging/debian/courier-ldap.lintian-overrides \
	packaging/debian/courier-maildrop.lintian-overrides \
	packaging/debian/courier-mlm.lintian-overrides \
	packaging/debian/courier-mlm-web.lintian-overrides \
	packaging/debian/courier-mlm-web-apache2.lintian-overrides \
	packaging/debian/courier-mysql.lintian-overrides \
	packaging/debian/courier-pgsql.lintian-overrides \
	packaging/debian/courier-pop3d.lintian-overrides \
	packaging/debian/courier-webadmin.lintian-overrides \
	packaging/debian/courier-webadmin-apache2.lintian-overrides \
	packaging/debian/courier-webmail.lintian-overrides \
	packaging/debian/courier-webmail-apache2.lintian-overrides \
	packaging/debian/mkinstall.pl.in \
	packaging/debian/rules \
	packaging/debian/source/format \
	packaging/debian/source/lintian-overrides

#
# install-perms creates permissions.dat, listing all files and directories,
# and their corresponding permissions.  This is used to build an RPM.
#

install-perms:
	@$(MAKE) install-perms-recursive
	@$(MAKE) install-perms-local

install-perms-recursive:
	@>permissions.dat
	@SUBDIRS="$(SUBDIRS)" ; for f in $$SUBDIRS ; do ( cd $$f && $(MAKE) $(AM_MAKEFLAGS) install-perms ) ; cat /dev/null >>$$f/permissions.dat ; done
	@SUBDIRS="$(SUBDIRS)" ; for f in $$SUBDIRS ; do  cat $$f/permissions.dat >>permissions.dat ; done

install-perms-local:

install-data-local:
	cd courier && $(MAKE) -$(MAKEFLAGS) DESTDIR=$(DESTDIR) install-permissions
	@cat libs/sqwebmail/sqwebmail.msg
	@echo "Do not forget to run make install-configure"

install-recursive: install-check-umask

install-check-umask:
	@test `umask | sed 's/^0*//'` = 22 && exit 0; \
	test `umask | sed 's/^0*//'` = 2 && exit 0; \
	test "$$INSTALL_IGNORE_UMASK" = 1 && exit 0; \
	echo "ERROR: your umask MUST be 022."; \
	echo "Change your umask to 022, then";\
	echo "rerun (g)make install." ; exit 1

all-recursive: all-make-check

all-make-check:
	$(MAKE) all-gmake-check FOO=BAR

all-gmake-check:
	@test "$(FOO)" = "BAR" && exit 0 ; echo "" ; \
		echo "ERROR: Something's wrong.  If you're using xBSD, you must use gmake" ; \
		echo "instead of make, as you were told by the INSTALL file (you did read the" ; \
		echo "INSTALL file, did you?)" ; echo "" ; exit 1

#
# For maintainer's use only
#

DATE := $(shell date -R)

dist-hook:
	test -f $(srcdir)/rpm.release && cp $(srcdir)/rpm.release $(distdir) ; \
		exit 0
	$(MAKE) -$(MAKEFLAGS) courier.spec
	cp courier.spec $(distdir)
	chmod +x $(distdir)/courier-debuild
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog.in
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog

release: dist

distrelease:
	umask 022; $(MAKE) distcheck
	$(MAKE) rpm
	$(MAKE)
	$(MAKE)
	rm -f INSTALL courier.spec packaging/debian/changelog
	$(MAKE) INSTALL courier.spec packaging/debian/changelog
	$(MAKE) dist

rpm: rpm-build

rpm-build:
	$(MAKE) bump.rpm.release
	$(MAKE) dist
	$(MAKE) dorpm

dorpm:
	rm -rf rpm/BUILD/*
	rm -f rpm/RPMS/*/*
	rm -f rpm/SOURCES/*
	rm -f rpm/SPECS/*
	rm -f rpm/SRPMS/*
	rpmbuild -ta --clean $(RPMXFLAGS) \
		--define 'notice_option --with-notice=unicode' \
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
	sh $$HOME/bin/rpmrepos.sh

bump.rpm.release: courier.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=100; \
	test "$$VERSION" = "@PACKAGE_VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@PACKAGE_VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{?dist}%{courier_release}/' \
		<courier.spec >courier.spec.new; \
	mv courier.spec.new courier.spec

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

install-configure:
	@$(MAKE) install-perms
	@SYSCONFTOOL@ `awk ' $$5 == "config" && $$1 ~ /\.dist$$/ { print $$1 }'  <permissions.dat | sed s@^@$$DESTDIR@`

install-webadmin-password:
	@stty -echo ; echo -n "Password: " ; read P; stty echo ; echo "" ; echo $$P >@sysconfdir@/webadmin/password && chown @mailuser@ @sysconfdir@/webadmin/password && chgrp @mailgroup@ @sysconfdir@/webadmin/password && chmod 400 @sysconfdir@/webadmin/password

upload-beta:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam courier courier-devel `echo @PACKAGE_VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig


upload-dist:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	set -e; v="@PACKAGE_NAME@/@PACKAGE_VERSION@/`date +%Y%m%d%H%M%S`"; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'; cd libs; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'
	socksify ~/src/sftools/release.pl mrsam courier courier @PACKAGE_VERSION@ @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig
