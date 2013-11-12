#
# Copyright 1998 - 2009 Double Precision, Inc.  See COPYING for
# distribution information.

@SET_MAKE@

AUTOMAKE_OPTIONS=dist-bzip2

# make distcheck fix

DISTCHECK_CONFIGURE_FLAGS=--enable-cgibindir=$$dc_install_base/cgi-bin --enable-imagedir=$$dc_install_base/images

HTML2TXT=links -dump -no-numbering --no-references

SUBDIRS=libs/numlib libs/gdbmobj libs/bdbobj libs/md5 libs/sha1 \
	libs/random128 libs/libhmac libs/unicode libs/rfc822 libs/rfc2045 \
	libs/liblock libs/maildir libs/soxwrap libs/cgi \
	libs/http11 libs/ldapaddressbook libs/gpglib libs/pcp libs/sqwebmail

noinst_SCRIPTS= sqwebmail.spec sqwebmail.lpspec

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

pkgman8dir=@mandir@/man8
pkgman8_DATA=deliverquota.8

pkgman1dir=@mandir@/man1
pkgman1_DATA=maildirmake.1

sqwebmaild.dist: libs/sqwebmail/sqwebmaild.dist
	cp libs/sqwebmail/sqwebmaild.dist .

pcpd: libs/pcp/pcpd
	cp libs/pcp/pcpd .

maildirmake: libs/maildir/maildirmake
	cp libs/maildir/maildirmake .

maildirmake.1: libs/maildir/maildirmake.1
	cp libs/maildir/maildirmake.1 .

deliverquota: libs/maildir/deliverquota
	cp libs/maildir/deliverquota .

deliverquota.8: libs/maildir/deliverquota.8
	cp libs/maildir/deliverquota.8 .

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

clean-local:
	rm -f config.rpath

all-recursive: all-make-check

all-make-check:
	$(MAKE) all-gmake-check FOO=BAR

all-gmake-check:
	@test "$(FOO)" = "BAR" && exit 0 ; echo "" ; \
		echo "ERROR: Something's wrong.  If you're using xBSD, you must use gmake" ; \
		echo "instead of make, as you were told by the INSTALL file (you did read the" ; \
		echo "INSTALL file, did you?)" ; echo "" ; exit 1

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

release:
	rm -f sqwebmail.spec sqwebmail.lpspec
	$(MAKE) sqwebmail.spec sqwebmail.lpspec
	$(MAKE) dist

WWW=$$HOME/www/www.courier-mta.org

htmldoc:
	for f in index.html links.html screenshots.html download.html; do xsltproc --nonet $(WWW)/xsl/sqwebmail.xsl html/$$f | perl -ne 's~\$(id):.*\$$~\$(id):'"$$h"'\$$~; print' >$(WWW)/sqwebmail/$$f; done
	cp html/*.gif html/*.js html/*.css $(WWW)/sqwebmail
	cp html/images/*.png $(WWW)/sqwebmail/images

if HAVE_HTMLPAGES
NEWS_SOURCE=html/index2.html

NEWS: $(NEWS_SOURCE)
	$(HTML2TXT) $(NEWS_SOURCE) >NEWS
endif

INSTALL.txt: INSTALL.html
	$(HTML2TXT) $(srcdir)/INSTALL.html >INSTALL.txt
	rm -f INSTALL
	cp INSTALL.txt INSTALL

dist-hook:
	rm -f $(distdir)/INSTALL $(distdir)/README
	$(HTML2TXT) $(srcdir)/README.html >$(distdir)/README
	$(HTML2TXT) $(srcdir)/INSTALL.html >$(distdir)/INSTALL
	cp sqwebmail.spec sqwebmail.lpspec $(distdir)
	test -f $(srcdir)/rpm.release && cp $(srcdir)/rpm.release $(distdir) \
		; exit 0

bump.rpm.release: sqwebmail.spec sqwebmail.lpspec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{?dist}%{courier_release}/' \
		<sqwebmail.spec >sqwebmail.spec.new; \
	mv sqwebmail.spec.new sqwebmail.spec; \
	sed 's/^Release: .*/Release: '$$RELEASE'/' \
		<sqwebmail.lpspec >sqwebmail.lpspec.new; \
	mv sqwebmail.lpspec.new sqwebmail.lpspec

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
	mv lpmtmp/*.src.lp repository-$(SUBDIR)/`lpm -q --distribution`/sources
	mv lpmtmp/*.lp repository-$(SUBDIR)/`lpm -q --distribution`/packages
	rm -rf lpmtmp
	ls -t repository-$(SUBDIR)/`lpm -q --distribution`/sources | tail -n +6 | xargs -r -n 1 lpm --erase repository-$(SUBDIR)
	lpm --sync repository-$(SUBDIR)
	rsync -a -v --delete-after repository-$(SUBDIR)/`lpm -q --distribution`/. headache:lpmtool.com/www/repositories/$(SUBDIR)/libs/sqwebmail/`lpm -q --distribution`

install-configure:
	list="ldapaddressbook.dist sqwebmaild.dist"; \
		for file in $$list; do \
		file="$(DESTDIR)$(sysconfdir)/$$file" ; \
		case "$$file" in *.dist) \
			@SYSCONFTOOL@ $$file;; esac ; done

upload-beta:
	test -z "$(SFPASSWD)" && exit 1; exit 0
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam $(SFPASSWD) courier webmail-devel `echo @VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig

upload-dist:
	test -z "$(SFPASSWD)" && exit 1; exit 0
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	set -e; v="@PACKAGE@/@VERSION@/`date +%Y%m%d%H%M%S`"; git tag -s $$v -m 'Tag version @VERSION@'; cd libs; git tag -s $$v -m 'Tag version @VERSION@'
	socksify ~/src/sftools/release.pl mrsam $(SFPASSWD) courier webmail @VERSION@ @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig
