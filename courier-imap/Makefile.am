#
# Copyright 1998 - 2022 Double Precision, Inc.  See COPYING for
# distribution information.

@SET_MAKE@

AM_DISTCHECK_CONFIGURE_FLAGS=--with-notice=unicode

TEMPLFILES=packaging/suse/courier-imap.init.in \
	packaging/suse/imap.pam \
	packaging/suse/pop3.pam

SUBDIRS=libs/numlib libs/bdbobj libs/gdbmobj libs/soxwrap \
	libs/rfc822 libs/rfc2045 \
	libs/md5 libs/sha1 libs/random128 libs/rfc1035 libs/liblock \
	libs/maildir libs/makedat libs/libhmac \
	libs/waitlib libs/tcpd libs/rootcerts libs/imap

sbin_SCRIPTS=sharedindexinstall sharedindexsplit
CLEANFILES=$(databin_SCRIPTS) $(man_MANS) $(sysconf_DATA) $(sbin_SCRIPTS)
databindir=$(datadir)
databin_SCRIPTS=mkimapdcert mkpop3dcert mkdhparams

binPROGRAMS=imapd pop3d maildirmake maildiracl deliverquota maildirkw \
	makeimapaccess makedat
sbinPROGRAMS=imaplogin pop3login
libexecPROGRAMS=makedatprog couriertcpd

bin_PROGRAMS= @binPROGRAMS_exec@
sbin_PROGRAMS=@sbinPROGRAMS_exec@
libexec_PROGRAMS=@libexecPROGRAMS_exec@

EXTRA_PROGRAMS=

man_MANS=couriertcpd.1 deliverquota.8 maildirmake.1 maildiracl.1 imapd.8 mkdhparams.8 mkimapdcert.8 mkpop3dcert.8 maildirkw.1 makeimapaccess.8
EXTRA_DIST=README README.cygwin\
	COPYING.GPL NEWS.html $(TEMPLFILES) pgpkeys.txt

sysconf_DATA=imapd.dist imapd-ssl.dist pop3d.dist pop3d-ssl.dist \
	imapd.cnf pop3d.cnf quotawarnmsg.example
libexec_SCRIPTS=imapd.rc imapd-ssl.rc pop3d.rc pop3d-ssl.rc

HTML2TXT=elinks -dump -no-numbering -no-references

NEWS: NEWS.html
	$(HTML2TXT) NEWS.html >NEWS

imaplogin: libs/imap/imaplogin
	cp libs/imap/imaplogin .

maildirkw: libs/maildir/maildirkw
	cp libs/maildir/maildirkw .

imapd: libs/imap/imapd
	cp libs/imap/imapd .

sharedindexinstall: libs/maildir/sharedindexinstall
	cp libs/maildir/sharedindexinstall .

sharedindexsplit: libs/maildir/sharedindexsplit
	cp libs/maildir/sharedindexsplit .

pop3login: libs/imap/pop3login
	cp libs/imap/pop3login .

pop3d: libs/imap/pop3d
	cp libs/imap/pop3d .

makedat: libs/makedat/makedat
	cp libs/makedat/makedat .

makedatprog: libs/makedat/makedatprog
	cp libs/makedat/makedatprog .

couriertcpd: libs/tcpd/couriertcpd
	cp libs/tcpd/couriertcpd .

maildirmake: libs/maildir/maildirmake
	cp libs/maildir/maildirmake .

maildiracl: libs/maildir/maildiracl
	cp libs/maildir/maildiracl .

deliverquota: libs/maildir/deliverquota
	cp libs/maildir/deliverquota .

pop3d.dist: libs/imap/pop3d.dist
	cp libs/imap/pop3d.dist .

pop3d-ssl.dist: libs/imap/pop3d-ssl.dist
	cp libs/imap/pop3d-ssl.dist .

imapd.dist: libs/imap/imapd.dist
	cp libs/imap/imapd.dist .

imapd-ssl.dist: libs/imap/imapd-ssl.dist
	cp libs/imap/imapd-ssl.dist .

couriertcpd.1: libs/tcpd/couriertcpd.1
	cp $(srcdir)/libs/tcpd/couriertcpd.1 .

deliverquota.8: libs/maildir/deliverquota.8
	cp libs/maildir/deliverquota.8 .

maildirmake.1: libs/maildir/maildirmake.1
	cp libs/maildir/maildirmake.1 .

maildiracl.1: libs/maildir/maildiracl.1
	cp libs/maildir/maildiracl.1 .

maildirkw.1: libs/maildir/maildirkw.1
	cp $(srcdir)/libs/maildir/maildirkw.1 .

imapd.8: libs/imap/imapd.8
	cp libs/imap/imapd.8 .

mkdhparams: libs/imap/mkdhparams
	cp libs/imap/mkdhparams .

mkimapdcert: libs/imap/mkimapdcert
	cp libs/imap/mkimapdcert .

mkpop3dcert: libs/imap/mkpop3dcert
	cp libs/imap/mkpop3dcert .

mkdhparams.8: libs/imap/mkdhparams.8
	cp -f libs/imap/mkdhparams.8 .

mkimapdcert.8: libs/imap/mkimapdcert.8
	cp -f libs/imap/mkimapdcert.8 .

makeimapaccess.8: libs/imap/makeimapaccess.8
	cp -f libs/imap/makeimapaccess.8 .

mkpop3dcert.8: libs/imap/mkpop3dcert.8
	cp -f libs/imap/mkpop3dcert.8 .

imapd.cnf: libs/imap/imapd.cnf
	cp libs/imap/imapd.cnf .

pop3d.cnf: libs/imap/pop3d.cnf
	cp libs/imap/pop3d.cnf .

quotawarnmsg.example: libs/maildir/quotawarnmsg
	cp -f $(srcdir)/libs/maildir/quotawarnmsg quotawarnmsg.example

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(databindir)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	test ! -x libs/tcpd/couriertls || $(INSTALL_PROGRAM) $(INSTALL_STRIP_FLAG) \
		libs/tcpd/couriertls $(DESTDIR)$(bindir)/couriertls
	$(mkinstalldirs) $(DESTDIR)$(sbindir)
	for f in mkimapdcert mkpop3dcert mkdhparams; \
		do rm -f $(DESTDIR)$(sbindir)/$$f ; \
		$(LN_S) $(databindir)/$$f $(DESTDIR)$(sbindir)/$$f ; done
	$(mkinstalldirs) $(DESTDIR)$(localstatedir)

install-exec-hook:
	chmod 444 $(DESTDIR)$(sysconfdir)/quotawarnmsg.example
	mkdir -p $(DESTDIR)$(sysconfdir)/shared
	chmod 755 $(DESTDIR)$(sysconfdir)/shared
	mkdir -p $(DESTDIR)$(sysconfdir)/shared.tmp
	chmod 755 $(DESTDIR)$(sysconfdir)/shared.tmp
	mkdir -p $(DESTDIR)$(sysconfdir)/imapaccess
	chmod 755 $(DESTDIR)$(sysconfdir)/imapaccess

install-data-local: install-man
	for f in $(man_MANS); do chmod a+r $(DESTDIR)$(mandir)/*/$$f; done
	@echo "Do not forget to run make install-configure"

uninstall-local:
	for f in mkimapdcert mkpop3dcert mkdhparams; \
		do rm -f $(DESTDIR)$(sbindir)/$$f ; done
	rm -f $(DESTDIR)$(bindir)/couriertls

#
# For maintainer's use only
#

DATE := $(shell date -R)

dist-hook: courier-imap.spec
	test -f $(srcdir)/rpm.release && cp $(srcdir)/rpm.release $(distdir) \
		; exit 0
	rm -f $(distdir)/INSTALL
	$(HTML2TXT) INSTALL.html >$(distdir)/INSTALL
	cp $(srcdir)/courier-imap.spec $(distdir)
	chmod +x $(distdir)/courier-debuild
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog.in
	@PERL@ -p -i -e 's/\@DATE\@/'"$(DATE)"'/' $(distdir)/packaging/debian/changelog

EXTRA_DIST += courier-debuild \
	packaging/debian/auto_install.sh.in \
	packaging/debian/changelog \
	packaging/debian/changelog.in \
	packaging/debian/control \
	packaging/debian/copyright \
	packaging/debian/courier-imap.lintian-overrides \
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

bump.deb.release: packaging/debian/changelog
	VERSION="x"; \
	test -f $(srcdir)/deb.release && . $(srcdir)/deb.release; \
	NEXT_RELEASE=100; \
	test "$$VERSION" = "@PACKAGE_VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@PACKAGE_VERSION@" >$(srcdir)/deb.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/deb.release; \
	. /etc/lsb-release; \
	sed 's/^\([^ ][^ ]*\).*/\1 (@VERSION@-'$$RELEASE'.'$$DISTRIB_RELEASE') devel; urgency=medium/' \
		<packaging/debian/changelog >packaging/debian/changelog.new; \
	mv packaging/debian/changelog.new packaging/debian/changelog

if HAVE_HTML

README: html/main.html
	$(HTML2TXT) $(srcdir)/html/index.html >README
endif

distrelease:
	umask 022; $(MAKE) distcheck
	$(MAKE) rpm
	$(MAKE)
	$(MAKE)
	$(MAKE) release

release: dist

rpm: build-rpm

build-rpm:
	$(MAKE) courier-imap.spec
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
		@PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2
	$$HOME/bin/rpmrepos.sh

bump.rpm.release: @PACKAGE_NAME@.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@PACKAGE_VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@PACKAGE_VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{courier_release}/' \
		courier-imap.spec >courier-imap.spec.new ; \
	mv courier-imap.spec.new courier-imap.spec

all-recursive: all-make-check

all-make-check:
	$(MAKE) all-gmake-check FOO=BAR

all-gmake-check:
	@test "$(FOO)" = "BAR" && exit 0 ; echo "" ; \
		echo "ERROR: Something's wrong.  If you're using xBSD, you must use gmake" ; \
		echo "instead of make, as you were told by the INSTALL file (you did read the" ; \
		echo "INSTALL file, did you?)" ; echo "" ; exit 1


# Copyright 2000 Double Precision, Inc.  See COPYING for
# distribution information.

# Need to add sysconftoolize to package:

EXTRA_DIST += sysconftool

# NOTE: when using a complex directory hierarchy it is only necessary to
# include sysconftool in the top level directory, and it is not necessary
# to add sysconftool to EXTRA_DIST in subdirectories.

SYSCONFFILES=$(sysconf_DATA)

# SYSCONFFILES should be all of your .dist files, this is a default.
# You can also change install-configure-local target below, instead.

# If -- in a complex directory hierarchy -- a certain subdirectory does not
# need install-configure, leave it defined as an empty rule to prevent
# sysconftoolize from appending another form template to this Makefile.am

install-configure:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
		( case "$$subdir" in .) continue;; \
			*) target=install-configure;; esac ; \
		cd $$subdir ; $(MAKE) install-configure DESTDIR=$(DESTDIR) 2>/dev/null) \
			; done ; exit 0
	$(MAKE) install-configure-local DESTDIR=$(DESTDIR)

# Modify the following target as appropriate:

install-configure-local:
	@list="$(SYSCONFFILES)"; \
		for file in $$list; do \
		file="$(DESTDIR)$(sysconfdir)/$$file" ; \
		case "$$file" in *.dist) \
			@SYSCONFTOOL@ $$file;; esac ; done
upload-beta:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam courier imap-devel `echo @PACKAGE_VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig

upload-dist:
	rm -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2; test -f @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig || exit 1
	set -e; v="@PACKAGE_NAME@/@PACKAGE_VERSION@/`date +%Y%m%d%H%M%S`"; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'; cd libs; git tag -s $$v -m 'Tag version @PACKAGE_VERSION@'
	socksify ~/src/sftools/release.pl mrsam courier imap @PACKAGE_VERSION@ @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2 @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.bz2.sig

dollar=$$
id=$(dollar)Id

WWW=$$HOME/www/hostrocket/courier-mta.org

htmldoc: INSTALL.html libs/maildir/README.maildirquota.html libs/maildir/README.sharedfolders.html libs/maildir/README.imapkeywords.html libs/imap/BUGS.html libs/imap/README.proxy.html
	for f in $^; do xsltproc --nonet $(srcdir)/imap-format.xsl $$f | perl -ne 's~\$(id):.*\$$~\$(id):'"$$h"'\$$~; print' >$(WWW)/imap/`basename $$f`; done
	rm -rf doc
	mkdir doc
	xsltproc --nonet -o doc/ $(srcdir)/imap.xsl $(srcdir)/imap.xml
	for f in doc/*.html; do \
		echo $$f; \
		tidy -i -asxml --doctype transitional \
			--input-encoding utf8 --output-encoding ascii \
			-q --tidy-mark no $$f >$$f.tmp; \
		xsltproc --nonet $(WWW)/xsl/postproc.xsl \
			$$f.tmp > $$f.tmp2; \
		xsltproc --nonet $(srcdir)/imap-format.xsl \
			 $$f.tmp2 >$$f; \
		rm -f $$f.tmp*; done
	mv -f doc/*.html $(WWW)/imap
	cp -f courier-imap.png bg.png $(WWW)/imap
	rm -rf doc

AUTOMAKE_OPTIONS=dist-bzip2
