#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.


noinst_PROGRAMS=webadmin
webadmin_SOURCES=webadmin.c

webadmindir=@webadmindir@

HTMLFILES=login.html admin-save.html admin-main.html unsecure.html
SCRIPTFILES=admin-main.pl admin-save.pl admin-cancel.pl
webadmin_DATA=$(HTMLFILES)
webadmin_SCRIPTS=webadmin.pl dumpenv.pl $(SCRIPTFILES)

EXTRA_DIST=$(HTMLFILES) $(SCRIPTFILES)

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

install-perms:
	@echo $(sysconfdir)/webadmin 700 @mailuser@ @mailgroup@ >permissions.dat
	@echo $(sysconfdir)/webadmin/added 700 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(sysconfdir)/webadmin/removed 700 @mailuser@ @mailgroup@ >>permissions.dat
	@echo $(webadmindir) 755 bin bin >>permissions.dat
	@for f in $(webadmin_SCRIPTS) ; do echo $(webadmindir)/$$f 555 bin bin >>permissions.dat ; done
	@for f in $(webadmin_DATA) ; do echo $(webadmindir)/$$f 444 bin bin >>permissions.dat ; done


