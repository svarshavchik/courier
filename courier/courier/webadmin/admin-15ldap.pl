#! perl
#
# TITLE: LDAP
#
#
# Copyright 2001-2004 Double Precision, Inc.  See COPYING for
# distribution information.

use webadmin;

my $errstr="";

display_form("notsupp.html")
    unless -f "$authldaprc";

if ($cgi->param("Save"))
{
    my $ldap=ReadKWConfigFile("authldaprc");

    my $newkw={
	"LOCATION" => "LDAP_URI\t" . param('URI'),
	"LDAP_BASEDN" => "LDAP_BASEDN\t" . param('BASEDN'),
	"LDAP_BINDDN" => "LDAP_BINDDN\t" . param('BINDDN') . "\nLDAP_BINDPW\t" . $$ldap{'LDAP_BINDPW'},
	"LDAP_TIMEOUT" => "LDAP_TIMEOUT\t" . param('TIMEOUT'),
	"LDAP_MAIL" => "LDAP_MAIL\t" . param('MAIL'),
	"LDAP_HOMEDIR" => "LDAP_HOMEDIR\t" . param('HOMEDIR'),
	"LDAP_PW" => "LDAP_CLEARPW\t" . param('CLEARPW') . "\nLDAP_CRYPTPW\t" . param('CRYPTPW'),
	"LDAP_FILTER" => "LDAP_FILTER\t" . param('FILTER'),
	} ;

    $errstr="\@REQUIRED\@"
	unless required
		 ('URI', 'BASEDN', 'TIMEOUT', 'MAIL', 'HOMEDIR') &&
		     (param('CLEARPW') || param('CRYPTPW')) &&
			 (param('UID') || param('GLOB_UID')) &&
			     (param('GID') || param('GLOB_GID'));

    $$newkw{"LDAP_AUTHBIND"}="LDAP_AUTHBIND\t1"
	if param('AUTHBIND');

    $$newkw{"LDAP_DOMAIN"}="LDAP_DOMAIN\t" . param('DOMAIN')
	if param('DOMAIN');

    $$newkw{"LDAP_GLOB_IDS"}="LDAP_GLOB_UID\t" . param('GLOB_UID') . "\nLDAP_GLOB_GID\t" . param('GLOB_GID')
	if param('GLOB_UID') || param('GLOB_GID');

    $$newkw{"LDAP_IDS"}=
	"LDAP_UID\t" . param('UID') . "\nLDAP_GID\t" . param('GID')
	    if param('UID') || param('GID');

    $$newkw{"LDAP_MAILDIR"}="LDAP_MAILDIR\t" . param('MAILDIR')
	if param('MAILDIR');

    $$newkw{"LDAP_DEFAULTDELIVERY"}="LDAP_DEFAULTDELIVERY\t" . param('DEFAULTDELIVERY')
	if param('DEFAULTDELIVERY');

    $$newkw{"LDAP_MAILDIRQUOTA"}=
	"LDAP_MAILDIRQUOTA\t" . param('MAILDIRQUOTA')
	    if param('MAILDIRQUOTA');

    $$newkw{"LDAP_FULLNAME"}="LDAP_FULLNAME\t" . param('FULLNAME')
	if param('FULLNAME');

    $$newkw{"LDAP_MAILDIR"}="LDAP_MAILDIR\t" . param('MAILDIR')
	if param('MAILDIR');
    SaveKWConfigFile("authldaprc", $newkw);

    unless ($errstr)
    {
	$errstr="\@SAVED\@";
    }
    changed("$authdaemond restart");
}

my $ldap=ReadKWConfigFile("authldaprc");

display_form("admin-15ldap.html",
	     {
		 "ERROR" => $errstr,

		 "URI" => $$ldap{'LDAP_URI'},
		 "PORT" => $$ldap{'LDAP_PORT'},
		 "BASEDN" => $$ldap{'LDAP_BASEDN'},

		 "BINDDN" => $$ldap{'LDAP_BINDDN'},

		 "TIMEOUT" => $$ldap{'LDAP_TIMEOUT'},

		 "AUTHBIND" => ("<input type=\"checkbox\" name=\"AUTHBIND\"" . ($$ldap{'LDAP_AUTHBIND'} ? " checked=\"checked\"":"")) . " />",
		 "MAIL" => $$ldap{'LDAP_MAIL'},
		 "DOMAIN" => $$ldap{'LDAP_DOMAIN'},
		 "GLOB_UID" => $$ldap{'LDAP_GLOB_UID'},
		 "GLOB_GID" => $$ldap{'LDAP_GLOB_GID'},
		 "HOMEDIR" => $$ldap{'LDAP_HOMEDIR'},
		 "DEFAULTDELIVERY" => $$ldap{'LDAP_DEFAULTDELIVERY'},
		 "MAILDIR" => $$ldap{'LDAP_MAILDIR'},
		 "MAILDIRQUOTA" => $$ldap{'LDAP_MAILDIRQUOTA'},
		 "FULLNAME" => $$ldap{'LDAP_FULLNAME'},
		 "CLEARPW" => $$ldap{'LDAP_CLEARPW'},
		 "CRYPTPW" => $$ldap{'LDAP_CRYPTPW'},
		 "UID" => $$ldap{'LDAP_UID'},
		 "GID" => $$ldap{'LDAP_GID'},
		 "FILTER" => $$ldap{'LDAP_FILTER'},
	     }
	     );
