#
# Copyright 1998 - 2009 Double Precision, Inc.  See COPYING for
# distribution information.

@SET_MAKE@

AUTOMAKE_OPTIONS=dist-bzip2
AM_CXXFLAGS=@COURIER_UNICODE_CXXFLAGS@

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

HTML2TXT=links -dump -no-numbering -no-references

INSTALL: courier/doc/install.html
	rm -f INSTALL
	$(HTML2TXT) courier/doc/install.html >INSTALL


NEWS: NEWS.html
	$(HTML2TXT) NEWS.html >NEWS

EXTRA_DIST=BENCHMARKS sysconftool ChangeLog.2001 \
	COPYING.GPL NEWS.html pgpkeys.txt config.rpath

DISTCLEANFILES=version configure.args
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

dist-hook:
	test -f $(srcdir)/rpm.release && cp $(srcdir)/rpm.release $(distdir) ; \
		exit 0
	$(MAKE) -$(MAKEFLAGS) courier.spec
	cp courier.spec $(distdir)

release: dist

distrelease:
	umask 022; $(MAKE) distcheck
	$(MAKE) rpm
	$(MAKE)
	$(MAKE)
	rm -f INSTALL courier.spec
	$(MAKE) INSTALL courier.spec
	$(MAKE) dist

rpm: rpm-build

rpm-build:
	$(MAKE) bump.rpm.release
	$(MAKE) dist
	$(MAKE) dorpm

lpm: lpm-build

lpm-build:
	$(MAKE) bump.rpm.release
	$(MAKE) dist
	rm -rf lpm
	mkdir lpm
	lpbuild `test "@REPOSITORY@" == "" || echo --define "repository=1"` --srcbuild -o lpm @PACKAGE@-@VERSION@.tar.bz2


publish: publish-@REPOSITORY@

publish-dev:
	$(MAKE) dopublish SUBDIR=dev

publish-prod:
	$(MAKE) dopublish SUBDIR=prod

dopublish:
	lpbuild --resign lpm/*
	rm -rf lpmtmp
	mkdir lpmtmp
	ln lpm/* lpmtmp
	rm -f lpmtmp/*debuginfo*
	mv lpmtmp/*.src.lp repository-$(SUBDIR)/`lpm -q --distribution`/sources
	mv lpmtmp/*.lp repository-$(SUBDIR)/`lpm -q --distribution`/packages
	rm -rf lpmtmp
	ls -t repository-$(SUBDIR)/`lpm -q --distribution`/sources | tail -n +6 | xargs -r -n 1 lpm --erase repository-$(SUBDIR)
	lpm --sync repository-$(SUBDIR)
	rsync -a -v --delete-after repository-$(SUBDIR)/`lpm -q --distribution`/. headache:lpmtool.com/www/repositories/$(SUBDIR)/courier/`lpm -q --distribution`

dorpm:
	rm -rf rpm/BUILD/*
	rm -f rpm/RPMS/*/*
	rm -f rpm/SOURCES/*
	rm -f rpm/SPECS/*
	rm -f rpm/SRPMS/*
	rpmbuild -ta --clean $(RPMXFLAGS) \
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

bump.rpm.release: courier.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{?dist}%{courier_release}/' \
		<courier.spec >courier.spec.new; \
	mv courier.spec.new courier.spec

install-configure:
	@$(MAKE) install-perms
	@SYSCONFTOOL@ `awk ' $$5 == "config" && $$1 ~ /\.dist$$/ { print $$1 }' <permissions.dat`

install-webadmin-password:
	@stty -echo ; echo -n "Password: " ; read P; stty echo ; echo "" ; echo $$P >@sysconfdir@/webadmin/password && chown @mailuser@ @sysconfdir@/webadmin/password && chgrp @mailgroup@ @sysconfdir@/webadmin/password && chmod 400 @sysconfdir@/webadmin/password

upload-beta:
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam courier courier-devel `echo @VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig


upload-dist:
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	set -e; v="@PACKAGE@/@VERSION@/`date +%Y%m%d%H%M%S`"; git tag -s $$v -m 'Tag version @VERSION@'; cd libs; git tag -s $$v -m 'Tag version @VERSION@'
	socksify ~/src/sftools/release.pl mrsam courier courier @VERSION@ @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig
