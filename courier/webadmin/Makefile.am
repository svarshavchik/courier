#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.


noinst_PROGRAMS=webadmin
webadmin_SOURCES=webadmin.c
webadmin_DEPENDENCIES=../libs/cgi/libcgi.la
webadmin_LDADD=../libs/cgi/libcgi.la
webadmindir=@webadmindir@

HTMLFILES=login.html admin-save.html admin-main.html unsecure.html
SCRIPTFILES=admin-main.pl admin-save.pl admin-cancel.pl
webadmin_DATA=$(HTMLFILES) $(SCRIPTFILES)
webadmin_SCRIPTS=webadmin.pl dumpenv.pl

EXTRA_DIST=$(HTMLFILES) $(SCRIPTFILES)

CLEANFILES=sockdir.h
BUILT_SOURCES=sockdir.h

sockdir.h:
	echo '#define SOCKDIR "$(localstatedir)"' >sockdir.h

install-exec-hook:
	-$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/webadmin/added
	-$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/webadmin/removed
	chmod 700 $(DESTDIR)$(sysconfdir)/webadmin
	-chown @mailuser@ $(DESTDIR)$(sysconfdir)/webadmin
	-chown @mailuser@ $(DESTDIR)$(sysconfdir)/webadmin/added
	-chown @mailuser@ $(DESTDIR)$(sysconfdir)/webadmin/removed
	-chgrp @mailgroup@ $(DESTDIR)$(sysconfdir)/webadmin
	-chgrp @mailgroup@ $(DESTDIR)$(sysconfdir)/webadmin/added
	-chgrp @mailgroup@ $(DESTDIR)$(sysconfdir)/webadmin/removed

install-data-hook:
	-$(mkinstalldirs) $(DESTDIR)$(webadmindir)
	chmod 755 $(DESTDIR)$(webadmindir)
	echo '$(authdaemond) restart' >>$(DESTDIR)$(sysconfdir)/webadmin/restartauthcmd
	-chown @mailuser@ $(DESTDIR)$(sysconfdir)/webadmin/restartcmd
	-chgrp @mailgroup@ $(DESTDIR)$(sysconfdir)/webadmin/restartcmd

uninstall-local:
	rm -f $(DESTDIR)$(sysconfdir)/webadmin/restartauthcmd

install-perms:
	@echo $(sysconfdir)/webadmin 700 @mailuser@ @mailgroup@ >permissions.dat
	@echo $(sysconfdir)/webadmin/added 700 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(sysconfdir)/webadmin/removed 700 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(sysconfdir)/webadmin/restartauthcmd 444 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(sysconfdir)/webadmin/restartcmd 444 @mailuser@ @mailgroup@ config >>permissions.dat
	@echo $(webadmindir) 755 bin bin >>permissions.dat
	@for f in $(webadmin_SCRIPTS) ; do echo $(webadmindir)/$$f 555 bin bin >>permissions.dat ; done
	@for f in $(webadmin_DATA) ; do echo $(webadmindir)/$$f 444 bin bin >>permissions.dat ; done
