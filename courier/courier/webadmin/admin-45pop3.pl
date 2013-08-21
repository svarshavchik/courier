#! perl
#
# TITLE: POP3
#
#
# Copyright 2001 Double Precision, Inc.  See COPYING for
# distribution information.

use webadmin;

my $errstr="";

my $saved=0;

my %cap_all;
my %cap_pop3;
my %cap_pop3_ssl;

{
    my $pop3=ReadEnvVarConfigFile("pop3d");
    my $pop3ssl=ReadEnvVarConfigFile("pop3d-ssl");

    foreach (grep (/./, split (/\s+/, $$pop3{'POP3AUTH_ORIG'})))
    {
	$cap_all{$_}=1;
	$cap_pop3{$_}=1;
    }

    foreach (grep (/./, split (/\s+/, $$pop3{'POP3AUTH_TLS_ORIG'})))
    {
	$cap_all{$_}=1;
	$cap_pop3_ssl{$_}=1;
    }
}

if ($cgi->param("Save"))
{
    ReplaceEnvVarConfigFile("pop3d", "POP3DSTART",
                            $cgi->param("ENABLED") ? "YES":"NO");
    ReplaceEnvVarConfigFile("pop3d-ssl", "POP3DSSLSTART",
                            $cgi->param("ENABLED_SSL") ? "YES":"NO");

    ReplaceEnvVarConfigFile("pop3d-ssl", "POP3_TLS_REQUIRED",
                            $cgi->param("REQUIRE_STARTTLS") ? "1":"0");

    ReplaceEnvVarConfigFile("pop3d", "MAXDAEMONS",
                            $cgi->param("MAXDAEMONS"));
    ReplaceEnvVarConfigFile("pop3d", "MAXPERIP",
                            $cgi->param("MAXPERIP"));
    $saved=1;
}

if ($cgi->param("SaveCAPA"))
{
    my @capa;
    my @capaSSL;

    foreach (sort keys %cap_all)
    {
	my $cap=$_;
	my $capfldname=$cap;

	$capfldname =~ s/=/--/g;

	push @capa, $cap if $cap_pop3{$cap} && $cgi->param($capfldname);

	push @capaSSL, $cap if $cap_pop3_ssl{$cap} &&
	    $cgi->param("SSL-$capfldname");
    }

    ReplaceEnvVarConfigFile("pop3d", "POP3AUTH",
			    join(" ", @capa));
    ReplaceEnvVarConfigFile("pop3d", "POP3AUTH_TLS",
			    join(" ", @capaSSL));

    $saved=1;
}


if ($saved)
{
    changed("$sbindir/pop3d stop ; . $sysconfdir/pop3d ; test \"\$POP3DSTART\" != YES || $sbindir/pop3d start");

    changed("$sbindir/pop3d-ssl stop ; . $sysconfdir/pop3d-ssl ; test \"\$POP3DSSLSTART\" != YES || $sbindir/pop3d-ssl start");
    $errstr="\@SAVED\@";
}

my $pop3=ReadEnvVarConfigFile("pop3d");
my $pop3ssl=ReadEnvVarConfigFile("pop3d-ssl");


my %cap_pop3_set;
my %cap_pop3_ssl_set;


foreach (grep (/./, split (/\s+/, $$pop3{'POP3AUTH'})))
{
    $cap_pop3_set{$_}=1;
}

foreach (grep (/./, split (/\s+/, $$pop3{'POP3AUTH_TLS'})))
{
    $cap_pop3_ssl_set{$_}=1;
}

my $capHTML="<table border=\"0\" cellpadding=\"8\"><tr><th>\@SASL\@</th><th>\@NONTLS\@</th><th>\@TLS\@</th></tr>\n";

foreach (sort keys %cap_all)
{
    my $cap=$_;
    my $capfldname=$cap;

    $capfldname =~ s/=/--/g;

    $capHTML .= "<tr><td><tt>$cap</tt></td><td>";

    if ($cap_pop3{$cap})
    {
	$capHTML .= "<input type=\"checkbox\" name=\"$capfldname\"" . ($cap_pop3_set{$cap} ? " checked=\"checked\"":"") . " />";
    }
    else
    {
	$capHTML .= "&nbsp;";
    }

    $capHTML .= "</td><td>";

    if ($cap_pop3_ssl{$cap})
    {
	$capHTML .= "<input type=\"checkbox\" name=\"SSL-$capfldname\"" . ($cap_pop3_ssl_set{$cap} ? " checked=\"checked\"":"") . " />";
    }
    else
    {
	$capHTML .= "&nbsp;";
    }
    $capHTML .= "</tr>\n";
}

$capHTML .= "</table>\n";

display_form("admin-45pop3.html",
	     {
		 "ENABLED" => "<input type='checkbox' name='ENABLED'" . (($$pop3{"POP3DSTART"} =~ /[yY]/) ? " checked=\"checked\"":"") . " />",
		 "ENABLED_SSL" => "<input type='checkbox' name='ENABLED_SSL'" . ($$pop3ssl{"POP3DSSLSTART"} =~ /[yY]/ ? " checked=\"checked\"":"") . " />",

		 "REQUIRE_STARTTLS" => "<input type='checkbox' name='REQUIRE_STARTTLS'" . ($$pop3ssl{"POP3_TLS_REQUIRED"} ? " checked=\"checked\"":"") . " />",

		 "MAXDAEMONS" => $$pop3{'MAXDAEMONS'},
		 "MAXPERIP" => $$pop3{'MAXPERIP'},

		 "CAPA" => $capHTML,

		 "ERROR" => $errstr,
	     }
	     );
