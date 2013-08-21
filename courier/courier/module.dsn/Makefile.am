#
# Copyright 1998 - 2005 Double Precision, Inc.  See COPYING for
# distribution information.

CLEANFILES=sysconfdir.h module.dsn permissions.dat

moduledir=$(libexecdir)/courier/modules/dsn

BUILT_SOURCES=sysconfdir.h

INSTALL=@INSTALL@ `test \`id -u\` != 0 || echo -o @mailuser@ -g @mailgroup@`
INSTALL_PROGRAM=${INSTALL} -m 550

DSNTXTIN=dsnheader.txt
DSNTXT=dsndelayed.txt dsndelivered.txt dsnfailed.txt dsnrelayed.txt \
       dsnfooter.txt dsnsubjectnotice.txt dsnsubjectwarn.txt

sysconf_DATA=$(DSNTXT) $(DSNTXTIN) module.dsn
module_PROGRAMS=courierdsn

module.dsn: courier.config
	cp -f $(srcdir)/courier.config module.dsn

EXTRA_DIST=courier.config $(DSNTXT)

courierdsn_SOURCES=dsn.c
courierdsn_DEPENDENCIES=@courierdir@/courier/libs/libmodule.la \
	@courierdir@/courier/libs/libcommon.la \
	@courierdir@/courier/libs/libcourier.la \
	@courierdir@/libs/rfc2045/librfc2045.la \
	@courierdir@/libs/rfc822/librfc822.la \
	@courierdir@/libs/numlib/libnumlib.la \
	@courierdir@/libs/waitlib/libwaitlib.a
courierdsn_LDADD=$(courierdsn_DEPENDENCIES)

noinst_LTLIBRARIES=librewrite.la

install-data-hook:
	@test `id -u` = 0 || exit 0 ; cd $(DESTDIR)$(moduledir) || exit 1 ; chown @mailuser@ . ; chgrp @mailgroup@ .

install-perms:
	@$(MAKE) install-perms-recursive
	@$(MAKE) install-perms-local

install-perms-recursive:
	@>permissions.dat
	@SUBDIRS="$(SUBDIRS)" ; for f in $$SUBDIRS ; do ( cd $$f && $(MAKE) $(AM_MAKEFLAGS) install-perms ) ; cat /dev/null >>$$f/permissions.dat ; cat $$f/permissions.dat >>permissions.dat ; done

install-perms-local:
	@echo $(moduledir) 755 @mailuser@ @mailgroup@ >>permissions.dat
	@for f in $(sysconf_DATA) ; do \
	echo $(sysconfdir)/$$f 644 @mailuser@ @mailgroup@ config\
		>>permissions.dat ; done
	@echo $(moduledir)/courierdsn 755 @mailuser@ @mailgroup@ >>permissions.dat

librewrite_la_SOURCES=dsnstub.c
librewrite_la_LDFLAGS=-static

sysconfdir.h: config.status
	echo "#define SYSCONFDIR \"$(sysconfdir)\"" >sysconfdir.h
