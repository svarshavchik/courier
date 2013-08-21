#! perl
#
# TITLE: PostgreSQL
#
#
# Copyright 2001-2004 Double Precision, Inc.  See COPYING for
# distribution information.

use webadmin;

my $errstr="";

display_form("notsupp.html")
    unless -f "$authpgsqlrc";

if ($cgi->param("Save"))
{
    my $pgsql=ReadKWConfigFile("authpgsqlrc");

    my $newkw={
	"LOCATION" => "PGSQL_HOST\t" . param('HOST') . "\nPGSQL_USERNAME\t" . param('USERNAME') . "\nPGSQL_PASSWORD\t" . $$pgsql{'PGSQL_PASSWORD'} . "\nPGSQL_PORT\t" . param('PORT'),
	"PGSQL_OPT" => "PGSQL_OPT\t" . param('PGSQL_OPT'),
	"PGSQL_DATABASE" => "PGSQL_DATABASE\t" . param('DATABASE'),
	"PGSQL_USER_TABLE" => "PGSQL_USER_TABLE\t" . param('USER_TABLE'),
	"PGSQL_UID_FIELD" => "PGSQL_UID_FIELD\t" . param('UID_FIELD'),
	"PGSQL_GID_FIELD" => "PGSQL_GID_FIELD\t" . param('GID_FIELD'),
	"PGSQL_LOGIN_FIELD" => "PGSQL_LOGIN_FIELD\t" . param('LOGIN_FIELD'),
	"PGSQL_HOME_FIELD" => "PGSQL_HOME_FIELD\t" . param('HOME_FIELD'),
	} ;

    $errstr="\@REQUIRED\@"
	unless required
		 ('HOST', 'USERNAME', 'DATABASE', 'USER_TABLE',
		  'UID_FIELD', 'GID_FIELD', 'LOGIN_FIELD', 'HOME_FIELD') &&
		      (param('CRYPT_PWFIELD') || param('CLEAR_PWFIELD'));

    foreach (("PGSQL_CRYPT_PWFIELD=PGSQL_CRYPT_PWFIELD=CRYPT_PWFIELD",
	      "PGSQL_CLEAR_PWFIELD=PGSQL_CLEAR_PWFIELD=CLEAR_PWFIELD",
	      "PGSQL_DEFAULT_DOMAIN=DEFAULT_DOMAIN=DEFAULT_DOMAIN",
	      "PGSQL_NAME_FIELD=PGSQL_NAME_FIELD=NAME_FIELD",
	      "PGSQL_MAILDIR_FIELD=PGSQL_MAILDIR_FIELD=MAILDIR_FIELD",
	      "PGSQL_DEFAULTDELIVERY=PGSQL_DEFAULTDELIVERY=DEFAULTDELIVERY",
	      "PGSQL_QUOTA_FIELD=PGSQL_QUOTA_FIELD=QUOTA_FIELD",
	      "PGSQL_WHERE_CLAUSE=PGSQL_WHERE_CLAUSE=WHERE_CLAUSE"
	      ))
    {
	next unless /(.*)=(.*)=(.*)/;

	$$newkw{$1}="$2\t" . param($3)
	    if param($3);
    }

    SaveKWConfigFile("authpgsqlrc", $newkw);

    unless ($errstr)
    {
	$errstr="\@SAVED\@";
    }
    changed("$authdaemond restart");
}

my $pgsql=ReadKWConfigFile("authpgsqlrc");

display_form("admin-15pgsql.html",
	     {
		 "ERROR" => $errstr,

		 "HOST" => $$pgsql{'PGSQL_HOST'},
		 "PORT" => $$pgsql{'PGSQL_PORT'},
		 "OPT" => $$pgsql{'PGSQL_OPT'},
		 "USERNAME" => $$pgsql{'PGSQL_USERNAME'},

		 "DATABASE" => $$pgsql{'PGSQL_DATABASE'},

		 "USER_TABLE" => $$pgsql{'PGSQL_USER_TABLE'},
		 "DEFAULT_DOMAIN" => $$pgsql{'DEFAULT_DOMAIN'},
		 "LOGIN_FIELD" => $$pgsql{'PGSQL_LOGIN_FIELD'},
		 "HOME_FIELD" => $$pgsql{'PGSQL_HOME_FIELD'},
		 "DEFAULTDELIVERY" => $$pgsql{'PGSQL_DEFAULTDELIVERY'},
		 "MAILDIR_FIELD" => $$pgsql{'PGSQL_MAILDIR_FIELD'},
		 "QUOTA_FIELD" => $$pgsql{'PGSQL_QUOTA_FIELD'},
		 "NAME_FIELD" => $$pgsql{'PGSQL_NAME_FIELD'},
		 "CLEAR_PWFIELD" => $$pgsql{'PGSQL_CLEAR_PWFIELD'},
		 "CRYPT_PWFIELD" => $$pgsql{'PGSQL_CRYPT_PWFIELD'},
		 "UID_FIELD" => $$pgsql{'PGSQL_UID_FIELD'},
		 "GID_FIELD" => $$pgsql{'PGSQL_GID_FIELD'},
		 "WHERE_CLAUSE" => $$pgsql{'PGSQL_WHERE_CLAUSE'},
	     }
	     );
