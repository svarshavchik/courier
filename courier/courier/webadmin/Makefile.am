#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.

webadmindir=@webadmindir@

CLEANFILES=permissions.dat

HTMLFILES=admin-00machine.html admin-05local.html admin-10password.html \
	admin-15ldap.html admin-15mysql.html admin-15pgsql.html \
	admin-20aliases.html \
	admin-15ldapa.html admin-30esmtp.html admin-30xspf.html \
	admin-31esmtp.html \
	admin-34fax.html admin-35fax.html \
	admin-40imap.html admin-45pop3.html admin-47webmail.html \
	admin-50bofh.html webadmin.pm notsupp.html

SCRIPTFILES=admin-00machine.pl admin-10password.pl admin-15ldap.pl \
	admin-15mysql.pl admin-15pgsql.pl admin-20aliases.pl \
	admin-15ldapa.pl admin-30esmtp.pl admin-30xspf.pl \
	admin-31esmtp.pl admin-34fax.pl admin-35fax.pl \
	admin-40imap.pl admin-45pop3.pl admin-47webmail.pl admin-50bofh.pl

webadmin_DATA=$(HTMLFILES)
webadmin_SCRIPTS=$(SCRIPTFILES) admin-05local.pl

EXTRA_DIST=$(HTMLFILES) $(SCRIPTFILES)

install-perms:
	@cp /dev/null permissions.dat
	@for f in $(webadmin_SCRIPTS) ; do echo $(webadmindir)/$$f 555 bin bin >>permissions.dat ; done
	@for f in $(webadmin_DATA) ; do echo $(webadmindir)/$$f 444 bin bin >>permissions.dat ; done


