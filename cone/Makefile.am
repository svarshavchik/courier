#
# Copyright 2003-2009, Double Precision Inc.
#
# See COPYING for distribution information.
#
SUBDIRS=libs/unicode libs/numlib libs/liblock libs/soxwrap libs/md5 libs/sha1 libs/libhmac libs/random128 libs/rfc822 libs/rfc1035 libs/rfc2045 libs/gpglib libs/maildir libs/tcpd curses libs/ldapaddressbook libmail libs/rootcerts cone

sysconf_DATA=cone.dist

cone.dist: cone/cone.dist
	cp -f cone/cone.dist cone.dist

noinst_SCRIPTS=cone.sh
BUILT_SOURCES=coneversion

cone.sh: cone/cone.sh
	cp -f cone/cone.sh cone.sh

HELPNAME="cone.hlp"

noinst_DATA=help.txt coneversion

help.txt: cone/help.txt
	cp -f cone/help.txt help.txt

CLEANFILES=cone.sh cone.dist help.txt
DISTCLEANFILES=coneversion

coneversion:
	echo "@VERSION@" | cmp -s - coneversion && exit 0; echo "@VERSION@" >coneversion

AUTOMAKE_OPTIONS=dist-bzip2

HTML2TXT=links -dump -no-numbering

onlinehelp: help.txt
	cp help.txt $$HOME/Mail/libmail

install-data-local:
	${mkinstalldirs} ${DESTDIR}${pkgdatadir}
	${INSTALL} -m 444 help.txt ${DESTDIR}${pkgdatadir}/${HELPNAME}


install-exec-local:
	${mkinstalldirs} ${DESTDIR}${libexecdir}
	${INSTALL} -m 755 cone/cone ${DESTDIR}${libexecdir}/cone
	${mkinstalldirs} ${DESTDIR}${bindir}
	${INSTALL} -m 755 cone.sh ${DESTDIR}${bindir}/cone
	${INSTALL} -m 755 cone/leaf ${DESTDIR}${bindir}/leaf
	${INSTALL} -m 755 libmail/mailtool ${DESTDIR}${bindir}/mailtool

uninstall-local:
	rm -f ${DESTDIR}${bindir}/cone
	rm -f ${DESTDIR}${bindir}/leaf
	rm -f ${DESTDIR}${bindir}/mailtool
	rm -f ${DESTDIR}${libexecdir}/cone
	rm -f ${DESTDIR}${pkgdatadir}/${HELPNAME}

EXTRA_DIST = cone.spec COPYING COPYING.GPL AUTHORS

dist-hook:
	rm -f $(distdir)/NEWS $(distdir)/README $(distdir)/INSTALL
	$(HTML2TXT) $(srcdir)/cone/html/index.html >$(distdir)/NEWS
	$(HTML2TXT) $(srcdir)/cone/html/README.html >$(distdir)/README
	$(HTML2TXT) $(srcdir)/cone/html/INSTALL.html >$(distdir)/INSTALL



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
	@list='$(SYSCONFFILES)'; for file in $$list; do \
		file="$(DESTDIR)$(sysconfdir)/$$file" ; \
		case "$$file" in *.dist) \
			@SYSCONFTOOL@ $$file;; esac ; done

rpm:
	$(MAKE) bump.rpm.release
	$(MAKE) dist
	$(MAKE) dorpm

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
		cone-@VERSION@.tar.bz2

bump.rpm.release: cone.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'%{?dist}%{cone_release}/' \
		<cone.spec >cone.spec.new; \
	mv cone.spec.new cone.spec

distrelease:
	cd cone/po && make update-po
	$(MAKE) distcheck 'DISTCHECK_CONFIGURE_FLAGS=CPPFLAGS="-I /usr/include/ncursesw"'
	$(MAKE) rpm
	$(MAKE) -$(MAKEFLAGS)
	$(MAKE) -$(MAKEFLAGS)
	rm -f cone.spec
	$(MAKE) cone.spec
	$(MAKE) dist

upload-beta:
	test -z "$(SFPASSWD)" && exit 1; exit 0
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	~/src/sftools/release.pl mrsam $(SFPASSWD) courier @PACKAGE@-devel `echo @VERSION@|sed 's/.*\(........\)/\1/'` @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig


upload-dist:
	test -z "$(SFPASSWD)" && exit 1; exit 0
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	git tag -s @PACKAGE@/@VERSION@/`date +%Y%m%d%H%M%S` -m 'Tag version @VERSION@'
	socksify ~/src/sftools/release.pl mrsam $(SFPASSWD) courier cone @VERSION@ @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig

.PHONY: rpm

pub:
	rm -rf tmp
	mkdir tmp
	set -e; for f in `cd cone/html; ls *.html`; \
	do \
		xsltproc --nonet $$HOME/www/www.courier-mta.org/xsl/cone.xsl cone/html/$$f >tmp/$$f ; \
	done
	cp -p cone/html/*.css cone/html/*.gif tmp
	rsync -a --exclude .svn --delete-after tmp/. $$HOME/www/www.courier-mta.org/cone
	rm -rf tmp

tst:
	rm -f valgrind.log*
	valgrind --demangle=yes --num-callers=16 --logfile=valgrind.log --leak-check=yes --suppressions=cone.suppress cone/cone 2>/dev/null
#	valgrind --demangle=yes --num-callers=8 --logfile=valgrind.log cone/cone -c CONFIG 2>/dev/null
