#
# Copyright 1998 - 2018 S. Varshavchik.  See COPYING for
# distribution information.

@SET_MAKE@

AUTOMAKE_OPTIONS=dist-bzip2
AM_DISTCHECK_CONFIGURE_FLAGS=--with-notice=unicode

# make distcheck fix

DISTCHECK_CONFIGURE_FLAGS=--enable-cgibindir=$$dc_install_base/cgi-bin --enable-imagedir=$$dc_install_base/images

HTML2TXT=elinks -dump -no-numbering --no-references

SUBDIRS=libs/numlib libs/gdbmobj libs/bdbobj libs/md5 libs/sha1 \
	libs/random128 libs/libhmac libs/rfc822 libs/rfc2045 \
	libs/liblock libs/maildir libs/soxwrap libs/cgi \
	libs/http11 libs/ldapaddressbook libs/gpglib libs/pcp libs/sqwebmail

noinst_SCRIPTS= sqwebmail.spec

EXTRA_DIST=INSTALL.html README NEWS README.html README.pam \
	sysconftool COPYING.GPL README.logindomainlist.html \
	pgpkeys.txt config.rpath

scriptbindir=$(sbindir)
scriptbin_SCRIPTS= \
	sharedindexinstall sharedindexsplit

pkglibPROGRAMS=maildirmake deliverquota reformime makemime pcpd

pkglibexecdir=$(libexecdir)/sqwebmail
pkglibexec_PROGRAMS= @pkglibPROGRAMS_exec@
libexec_SCRIPTS=sqwebmaild.rc

EXTRA_PROGRAMS=

CLEANFILES=$(pkgman1_DATA) $(pkgman8_DATA) $(sysconf_DATA) \
	sharedindexinstall sharedindexsplit

sqwebmaild.dist: libs/sqwebmail/sqwebmaild.dist
	cp libs/sqwebmail/sqwebmaild.dist .

pcpd: libs/pcp/pcpd
	cp libs/pcp/pcpd .

maildirmake: libs/maildir/maildirmake
	cp libs/maildir/maildirmake .

deliverquota: libs/maildir/deliverquota
	cp libs/maildir/deliverquota .

reformime: libs/rfc2045/reformime
	cp libs/rfc2045/reformime .

makemime: libs/rfc2045/makemime
	cp libs/rfc2045/makemime .

sharedindexinstall: libs/maildir/sharedindexinstall
	cp -f libs/maildir/sharedindexinstall .

sharedindexsplit: libs/maildir/sharedindexsplit
	cp -f libs/maildir/sharedindexsplit .

cgibindir=@cgibindir@

#
# Install SqWebMail setuid to root, if we're running as root
#

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(cgibindir)
	test -w $(DESTDIR)$(cgibindir) || exit 0; \
		$(INSTALL_PROGRAM) libs/sqwebmail/sqwebmail \
			$(DESTDIR)$(cgibindir)/sqwebmail
	$(mkinstalldirs) $(DESTDIR)$(pkglibexecdir)
	$(INSTALL_PROGRAM) libs/sqwebmail/sqwebmaild $(DESTDIR)$(pkglibexecdir)/sqwebmaild

install-data-local:
	$(mkinstalldirs) $(DESTDIR)@imagedir@
	for f in `cat libs/sqwebmail/images/filelist` ; do \
		$(INSTALL_DATA) $(srcdir)/libs/sqwebmail/images/$$f $(DESTDIR)@imagedir@/$$f ;\
		done
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	@cat libs/sqwebmail/sqwebmail.msg

uninstall-local:
	test -w $(DESTDIR)$(cgibindir) || exit 0; \
		rm -f $(DESTDIR)$(cgibindir)/sqwebmail
	rm -f $(DESTDIR)$(pkglibexecdir)/sqwebmaild
	for f in `cat libs/sqwebmail/images/filelist` ; do \
		rm -f $(DESTDIR)@imagedir@/$$f ;\
		done

configure-check:
	@echo "SqWebMail CGI will be installed in @cgibindir@"
	@echo "Images will be installed in @imagedir@"
	@cd sqwebmail && $(MAKE) configure-check

all-recursive: all-make-check

all-make-check:
	$(MAKE) all-gmake-check FOO=BAR

all-gmake-check:
	@test "$(FOO)" = "BAR" && exit 0 ; echo "" ; \
		echo "ERROR: Something's wrong.  If you're using xBSD, you must use gmake" ; \
		echo "instead of make, as you were told by the INSTALL file (you did read the" ; \
		echo "INSTALL file, did you?)" ; echo "" ; exit 1

check-am:
	cd libs/sqwebmail; $(MAKE) check-version TOPLEVEL_VERSION="`echo $(VERSION) | cut -d. -f1-3`"

# Maintainer stuff

README: README.html
	$(HTML2TXT) $(srcdir)/README.html >README

distrelease:
	umask 022; $(MAKE) distcheck
	$(MAKE) rpm
	$(MAKE)
	$(MAKE)
	$(MAKE) release

rpm: rpm-build

rpm-build:
	$(MAKE) bump.rpm.release
	$(MAKE) dist
	rm -rf rpm/BUILD/*
	rm -f rpm/RPMS/*/*
	rm -f rpm/SOURCES/*
	rm -f rpm/SPECS/*
	rm -f rpm/SRPMS/*
	rpmbuild -ta --clean \
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
	$$HOME/bin/rpmrepos.sh

release:
	rm -f sqwebmail.spec
	$(MAKE) sqwebmail.spec
	$(MAKE) dist

WWW=$$HOME/www/hostrocket/courier-mta.org

htmldoc:
	for f in index.html links.html screenshots.html download.html; do xsltproc --nonet $(WWW)/xsl/sqwebmail.xsl html/$$f | perl -ne 's~\$(id):.*\$$~\$(id):'"$$h"'\$$~; print' >$(WWW)/sqwebmail/$$f; done
	cp html/*.gif html/*.js html/*.css $(WWW)/sqwebmail
	cp html/images/*.png $(WWW)/sqwebmail/images

if HAVE_HTMLPAGES
NEWS_SOURCE=html/index.html

NEWS: $(NEWS_SOURCE)
	$(HTML2TXT) $(NEWS_SOURCE) >NEWS
endif

INSTALL.txt: INSTALL.html
	$(HTML2TXT) $(srcdir)/INSTALL.html >INSTALL.txt
	rm -f INSTALL
	cp INSTALL.txt INSTALL

DATE := $(shell date -R)

dist-hook:
	rm -f $(distdir)/INSTALL $(distdir)/README
	$(HTML2TXT) $(srcdir)/README.html >$(distdir)/README
	$(HTML2TXT) $(srcdir)/INSTALL.html >$(distdir)/INSTALL
	cp sqwebmail.spec $(distdir)
	test -f $(srcdir)/rpm.release && cp $(srcdir)/rpm.release $(distdir) \
		; exit 0
	chmod +x $(distdir)/courier-debuild
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog.in
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog

EXTRA_DIST += courier-debuild \
	packaging/debian/auto_fixperms.sh.in \
	packaging/debian/auto_install.sh.in \
	packaging/debian/changelog \
	packaging/debian/changelog.in \
	packaging/debian/control \
	packaging/debian/copyright \
	packaging/debian/lintian-overrides.allow-errors \
	packaging/debian/rules \
	packaging/debian/source/format \
	packaging/debian/source/lintian-overrides \
	packaging/debian/sqwebmail.lintian-overrides \
	packaging/debian/sqwebmail-apache2.lintian-overrides

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

bump.rpm.release: sqwebmail.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@PACKAGE_VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@PACKAGE_VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{?dist}%{courier_release}/' \
		<sqwebmail.spec >sqwebmail.spec.new; \
	mv sqwebmail.spec.new sqwebmail.spec;

install-configure:
	list="ldapaddressbook.dist sqwebmaild.dist"; \
		for file in $$list; do \
		file="$(DESTDIR)$(sysconfdir)/$$file" ; \
		case "$$file" in *.dist) \
			@SYSCONFTOOL@ $$file;; esac ; done

upload-beta:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam courier webmail-devel `echo @PACKAGE_VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig

upload-dist:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	set -e; v="@PACKAGE_NAME@/@PACKAGE_VERSION@/`date +%Y%m%d%H%M%S`"; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'; cd libs; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'
	socksify ~/src/sftools/release.pl mrsam courier webmail @PACKAGE_VERSION@ @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig
