#! perl
#
# TITLE: LDAP-based aliasing/routing
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
    my $ldap=ReadKWConfigFile("ldapaliasrc");

    my $newkw={
	"LDAP_ALIAS" => "LDAP_ALIAS\t" . (param('ENABLED') ? "1":"0"),
	"LDAP_LOCATION" => "LDAP_URI\t" . param('URI'),
	"LDAP_NUMPROCS" => "LDAP_NUMPROCS\t" . param('NUMPROCS'),
	
	"LDAP_BASEDN" => "LDAP_BASEDN\t" . param('BASEDN'),
	"LDAP_BINDINFO" => "LDAP_BINDDN\t" . param('BINDDN') . "\nLDAP_BINDPW\t" . $$ldap{'LDAP_BINDPW'},
	"LDAP_TIMEOUT" => "LDAP_TIMEOUT\t" . param('TIMEOUT'),
	"LDAP_MAIL" => "LDAP_MAIL\t" . param('MAIL'),
	"LDAP_MAILDROP" => "LDAP_MAILDROP\t" . param('MAILDROP'),
	"LDAP_SOURCE" => "LDAP_SOURCE\t" . param('SOURCE'),
	"LDAP_VIRTUALMAP" => "LDAP_VDOMAIN\t" . param('VDOMAIN') . "\nLDAP_VUSER\t" . param('VUSER'),

	} ;
    $errstr="\@SAVED\@";
    SaveKWConfigFile("ldapaliasrc", $newkw);

    changed("test -x $sbindir/courierldapaliasd || exit 0; $sbindir/courierldapaliasd start; $sbindir/courierldapaliasd restart");
}

my $ldap=ReadKWConfigFile("ldapaliasrc");

display_form("admin-15ldapa.html",
	     {
		 "ERROR" => $errstr,
		 "ENABLED" => ("<input type=\"checkbox\" name=\"ENABLED\"" . ($$ldap{'LDAP_ALIAS'} ? " checked=\"checked\"":"")) . " />",

		 "URI" => $$ldap{'LDAP_URI'},
		 "BASEDN" => $$ldap{'LDAP_BASEDN'},

		 "BINDDN" => $$ldap{'LDAP_BINDDN'},

		 "TIMEOUT" => $$ldap{'LDAP_TIMEOUT'},

		 "NUMPROCS" => $$ldap{'LDAP_NUMPROCS'},
		 "MAIL" => $$ldap{'LDAP_MAIL'},
		 "MAILDROP" => $$ldap{'LDAP_MAILDROP'},
		 "SOURCE" => $$ldap{'LDAP_SOURCE'},

		 "VDOMAIN" => $$ldap{'LDAP_VDOMAIN'},
		 "VUSER" => $$ldap{'LDAP_VUSER'},
	     }
	     );
