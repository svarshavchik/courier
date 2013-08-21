#! perl
#
# TITLE: MySQL
#
#
# Copyright 2001-2004 Double Precision, Inc.  See COPYING for
# distribution information.

use webadmin;

my $errstr="";

display_form("notsupp.html")
    unless -f "$authmysqlrc";

if ($cgi->param("Save"))
{
    my $mysql=ReadKWConfigFile("authmysqlrc");

    my $newkw={
	"LOCATION" => "MYSQL_SERVER\t" . param('SERVER') . "\nMYSQL_USERNAME\t" . param('USERNAME') . "\nMYSQL_PASSWORD\t" . $$mysql{'MYSQL_PASSWORD'},
	'MYSQL_OPT' => "MYSQL_OPT\t" . $$mysql{'MYSQL_OPT'},
	"MYSQL_PORT" => "MYSQL_PORT\t" . param('PORT'),
	"MYSQL_DATABASE" => "MYSQL_DATABASE\t" . param('DATABASE'),
	"MYSQL_USER_TABLE" => "MYSQL_USER_TABLE\t" . param('USER_TABLE'),
	"MYSQL_UID_FIELD" => "MYSQL_UID_FIELD\t" . param('UID_FIELD'),
	"MYSQL_GID_FIELD" => "MYSQL_GID_FIELD\t" . param('GID_FIELD'),
	"MYSQL_LOGIN_FIELD" => "MYSQL_LOGIN_FIELD\t" . param('LOGIN_FIELD'),
	"MYSQL_HOME_FIELD" => "MYSQL_HOME_FIELD\t" . param('HOME_FIELD'),
	} ;

    $errstr="\@REQUIRED\@"
	unless required
		 ('SERVER', 'USERNAME', 'DATABASE', 'USER_TABLE',
		  'UID_FIELD', 'GID_FIELD', 'LOGIN_FIELD', 'HOME_FIELD') &&
		      (param('CRYPT_PWFIELD') || param('CLEAR_PWFIELD'));

    foreach (("MYSQL_SOCKET=MYSQL_SOCKET=SOCKET",
	      "MYSQL_CRYPT_PWFIELD=MYSQL_CRYPT_PWFIELD=CRYPT_PWFIELD",
	      "MYSQL_CLEAR_PWFIELD=MYSQL_CLEAR_PWFIELD=CLEAR_PWFIELD",
	      "MYSQL_DEFAULT_DOMAIN=DEFAULT_DOMAIN=DEFAULT_DOMAIN",
	      "MYSQL_NAME_FIELD=MYSQL_NAME_FIELD=NAME_FIELD",
	      "MYSQL_MAILDIR_FIELD=MYSQL_MAILDIR_FIELD=MAILDIR_FIELD",
	      "MYSQL_DEFAULTDELIVERY=MYSQL_DEFAULTDELIVERY=DEFAULTDELIVERY",
	      "MYSQL_QUOTA_FIELD=MYSQL_QUOTA_FIELD=QUOTA_FIELD",
	      "MYSQL_WHERE_CLAUSE=MYSQL_WHERE_CLAUSE=WHERE_CLAUSE"
	      ))
    {
	next unless /(.*)=(.*)=(.*)/;

	$$newkw{$1}="$2\t" . param($3)
	    if param($3);
    }

    SaveKWConfigFile("authmysqlrc", $newkw);

    unless ($errstr)
    {
	$errstr="\@SAVED\@";
    }
    changed("$authdaemond restart");
}

my $mysql=ReadKWConfigFile("authmysqlrc");

display_form("admin-15mysql.html",
	     {
		 "ERROR" => $errstr,

		 "SERVER" => $$mysql{'MYSQL_SERVER'},
		 "PORT" => $$mysql{'MYSQL_PORT'},
		 "USERNAME" => $$mysql{'MYSQL_USERNAME'},

		 "SOCKET" => $$mysql{'MYSQL_SOCKET'},

		 "DATABASE" => $$mysql{'MYSQL_DATABASE'},

		 "USER_TABLE" => $$mysql{'MYSQL_USER_TABLE'},
		 "DEFAULT_DOMAIN" => $$mysql{'DEFAULT_DOMAIN'},
		 "LOGIN_FIELD" => $$mysql{'MYSQL_LOGIN_FIELD'},
		 "HOME_FIELD" => $$mysql{'MYSQL_HOME_FIELD'},
		 "MAILDIR_FIELD" => $$mysql{'MYSQL_MAILDIR_FIELD'},
		 "DEFAULTDELIVERY" => $$mysql{'MYSQL_DEFAULTDELIVERY'},
		 "QUOTA_FIELD" => $$mysql{'MYSQL_QUOTA_FIELD'},
		 "NAME_FIELD" => $$mysql{'MYSQL_NAME_FIELD'},
		 "CLEAR_PWFIELD" => $$mysql{'MYSQL_CLEAR_PWFIELD'},
		 "CRYPT_PWFIELD" => $$mysql{'MYSQL_CRYPT_PWFIELD'},
		 "UID_FIELD" => $$mysql{'MYSQL_UID_FIELD'},
		 "GID_FIELD" => $$mysql{'MYSQL_GID_FIELD'},
		 "WHERE_CLAUSE" => $$mysql{'MYSQL_WHERE_CLAUSE'},
	     }
	     );
