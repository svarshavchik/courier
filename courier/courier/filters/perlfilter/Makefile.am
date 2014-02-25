#
# Copyright 2002-2014 Double Precision, Inc.  See COPYING for
# distribution information.

EXTRA_PROGRAMS=perlfilter

EXTRA_DIST=$(script_SCRIPTS)
BUILT_SOURCES=filtersocketdir.h wrapperpl.h xsinit.c

CLEANFILES=$(BUILT_SOURCES) permissions.dat

filterbindir=$(libexecdir)/filters
filterbin_PROGRAMS=@perlfilter_prog@

scriptdir=$(datadir)
script_SCRIPTS=perlfilter-wrapper.pl perlfilter-example.pl \
		perlfilter-ratelimit.pl

filtersocketdir=$(localstatedir)/filters
allfiltersocketdir=$(localstatedir)/allfilters

filtersocketdir.h: config.status
	echo '#define FILTERSOCKETDIR "$(filtersocketdir)"' >filtersocketdir.h
	echo '#define ALLFILTERSOCKETDIR "$(allfiltersocketdir)"' >>filtersocketdir.h

wrapperpl.h: config.status
	echo '#define WRAPPERPL "$(scriptdir)/perlfilter-wrapper.pl"' >wrapperpl.h

perlfilter_SOURCES=perlfilter.c
perlfilter_DEPENDENCIES=\
	../libfilter/libfilter.a ../../../libs/waitlib/libwaitlib.a \
	../../libs/libcommon.la ../../libs/libcourier.la
perlfilter_LDADD=$(perlfilter_DEPENDENCIES) @PERL_LDFLAGS@

xsinit.c: config.status
	@PERL@ -MExtUtils::Embed -e xsinit -- -o xsinit.c -std

install-perms:
	@$(MAKE) -$(MAKEFLAGS) install-perms-recursive
	@$(MAKE) -$(MAKEFLAGS) install-perms-local

install-perms-recursive:
	@>permissions.dat
	@SUBDIRS="$(SUBDIRS)" ; for f in $$SUBDIRS ; do ( cd $$f && $(MAKE) $(AM_MAKEFLAGS) install-perms ) ; cat /dev/null >>$$f/permissions.dat ; cat $$f/permissions.dat >>permissions.dat ; done

install-perms-local:
	@test "@perlfilter_prog@" = "perlfilter" || exit 0; \
		echo "$(filterbindir)/perlfilter 755 bin bin" >>permissions.dat
	@echo "$(scriptdir)/perlfilter-wrapper.pl 755 bin bin" >>permissions.dat
	@echo "$(scriptdir)/perlfilter-ratelimit.pl 755 bin bin" >>permissions.dat
	@echo "$(scriptdir)/perlfilter-example.pl 755 bin bin config" >>permissions.dat

install-data-hook:
	chmod 755 $(DESTDIR)$(scriptdir)/perlfilter-wrapper.pl
	chmod 755 $(DESTDIR)$(scriptdir)/perlfilter-example.pl
	@test "@perlfilter_prog@" = "perlfilter" || exit 0; \
		chmod 755 $(DESTDIR)$(filterbindir)/perlfilter
	test `id -u` = 0 || exit 0; for f in $(DESTDIR)$(scriptdir)/perlfilter-wrapper.pl $(DESTDIR)$(scriptdir)/perlfilter-example.pl $(DESTDIR)$(filterbindir)/perlfilter ; do test -f $$f || continue; chown @mailuser@ $$f ; chgrp @mailgroup@ $$f ; done
